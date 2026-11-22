#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

static constexpr int maxDelayMilliseconds = 40; // delay only; no lookahead

juce::AudioProcessorValueTreeState::ParameterLayout RELINKAITimingAlignAudioProcessor::createParams()
{
    using AP = juce::AudioParameterFloat;
    using BP = juce::AudioParameterBool;
    using CP = juce::AudioParameterChoice;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<BP>(juce::ParameterID { "auto", 1 }, "Auto Detection", true));
    params.push_back(std::make_unique<CP>(juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Tight", "Natural", "Groove" }, 1));
    params.push_back(std::make_unique<AP>(juce::ParameterID { "strength", 1 }, "Correction Strength %", juce::NormalisableRange<float>(0, 100, 1), 80));
    params.push_back(std::make_unique<AP>(juce::ParameterID { "manual", 1 }, "Manual Offset ms", juce::NormalisableRange<float>(-35, 35, 0.1f), 0));
    params.push_back(std::make_unique<AP>(juce::ParameterID { "groove", 1 }, "Desired Groove ms", juce::NormalisableRange<float>(-25, 25, 0.1f), 0));
    params.push_back(std::make_unique<AP>(juce::ParameterID { "threshold", 1 }, "Attack Threshold", juce::NormalisableRange<float>(0.01f, 0.5f, 0.005f), 0.06f));
    params.push_back(std::make_unique<AP>(juce::ParameterID { "maxmatch", 1 }, "Match Window ms", juce::NormalisableRange<float>(5, 35, 1), 22));
    params.push_back(std::make_unique<AP>(juce::ParameterID { "mix", 1 }, "Wet %", juce::NormalisableRange<float>(0, 100, 1), 100));
    return { params.begin(), params.end() };
}

RELINKAITimingAlignAudioProcessor::RELINKAITimingAlignAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
         .withInput("Main", juce::AudioChannelSet::stereo(), true)
         .withInput("Kick Reference (Sidechain)", juce::AudioChannelSet::stereo(), false)
         .withOutput("Main", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "TimingAlignState", createParams()) {}

bool RELINKAITimingAlignAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut || (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo()))
        return false;
    const auto side = layouts.getChannelSet(true, 1);
    return side.isDisabled() || side == juce::AudioChannelSet::mono() || side == juce::AudioChannelSet::stereo();
}

void RELINKAITimingAlignAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    baseSamples = 0; // Report and apply ZERO plugin latency.
    ringSize = juce::roundToInt(sr * maxDelayMilliseconds / 1000.0) + 32;
    for (auto& ch : ring) ch.assign(static_cast<size_t>(ringSize), 0.0f);
    writePos = 0; counter = 0; currentDelay = 0.0f;
    targetDetector = {}; referenceDetector = {};
    lastRefHit = lastTargetHit = usedRefHit = usedTargetHit = -100000000;
    trackedOffsetSamples = 0; stablePairs = 0;
    observedOffsetMs.store(0); confidence.store(0);
    setLatencySamples(0);
}

int64_t RELINKAITimingAlignAudioProcessor::Detector::detect(float amplitude, int64_t sample, float alpha, int64_t cooldown, float threshold)
{
    // Fast attack and short exponential envelope; threshold compares against local floor.
    envelope = juce::jmax(amplitude, envelope * alpha);
    const bool onset = (envelope > threshold && envelope > previousEnvelope * 1.6f
                        && sample - lastHit > cooldown);
    previousEnvelope = envelope;
    if (onset) { lastHit = sample; return sample; }
    return -1;
}

void RELINKAITimingAlignAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer(buffer, false, 0);
    auto side = getBusBuffer(buffer, true, 1);
    const auto channels = main.getNumChannels();
    const auto sideChannels = side.getNumChannels();
    const auto samples = main.getNumSamples();
    if (ringSize == 0 || channels == 0) return;

    const auto get = [this](const char* id) -> float { return state.getRawParameterValue(id)->load(); };
    const bool automatic = get("auto") > 0.5f;
    const int mode = static_cast<int>(get("mode"));
    const float strength = get("strength") / 100.0f;
    const float manual = get("manual") * static_cast<float>(sr / 1000.0);
    const float groove = get("groove") * static_cast<float>(sr / 1000.0);
    const float matchWindow = get("maxmatch") * static_cast<float>(sr / 1000.0);
    const float threshold = get("threshold");
    const float wet = get("mix") / 100.0f;
    const float alpha = std::exp(-1.0f / static_cast<float>(0.006 * sr));
    const auto cooldown = static_cast<int64_t>(0.065 * sr);

    for (int i = 0; i < samples; ++i)
    {
        float t = 0, r = 0;
        for (int c = 0; c < channels; ++c) t = juce::jmax(t, std::abs(main.getReadPointer(c)[i]));
        for (int c = 0; c < sideChannels; ++c) r = juce::jmax(r, std::abs(side.getReadPointer(c)[i]));

        if (sideChannels > 0 && automatic)
        {
            const auto tref = referenceDetector.detect(r, counter, alpha, cooldown, threshold);
            const auto ttarget = targetDetector.detect(t, counter, alpha, cooldown, threshold);
            if (tref >= 0) lastRefHit = tref;
            if (ttarget >= 0) lastTargetHit = ttarget;
            if ((tref >= 0 || ttarget >= 0)
                && lastRefHit != usedRefHit && lastTargetHit != usedTargetHit
                && std::abs(lastTargetHit - lastRefHit) <= matchWindow)
            {
                const auto diff = static_cast<float>(lastTargetHit - lastRefHit);
                trackedOffsetSamples = (stablePairs == 0) ? diff : 0.83f * trackedOffsetSamples + 0.17f * diff;
                usedRefHit = lastRefHit;
                usedTargetHit = lastTargetHit;
                stablePairs = juce::jmin(100, stablePairs + 1);
                observedOffsetMs.store(trackedOffsetSamples * 1000.0f / static_cast<float>(sr));
                confidence.store(juce::jmin(1.0f, stablePairs / 8.0f));
            }
        }

        // Safe mode: no anticipatory shifts. A zero offset is a true, sample-exact
        // passthrough, without relying on the Cubase host latency compensation.
        // Negative offset = early target -> positive delay. Positive offset = late
        // target -> would require lookahead; do not attempt it in zero-latency mode.
        const float offset = automatic ? (stablePairs >= 2 ? trackedOffsetSamples : 0.0f) : manual;
        const float amount = strength * (mode == 0 ? 1.0f : (mode == 1 ? 0.7f : 0.5f));
        const float desiredDiff = (mode == 2 ? groove : 0.0f);
        const float correction = (offset - desiredDiff) * amount;
        const float wantedDelay = juce::jlimit(0.0f, static_cast<float>(ringSize - 3), -correction);
        // Smoothing prevents zipper noise when the estimated timing changes.
        const float smoothing = 1.0f - std::exp(-1.0f / static_cast<float>(0.03 * sr));
        currentDelay += (wantedDelay - currentDelay) * smoothing;
        if (wantedDelay == 0.0f && currentDelay < 0.00001f)
            currentDelay = 0.0f;
        for (int c = 0; c < channels; ++c)
        {
            float* audio = main.getWritePointer(c);
            const float input = audio[i];
            ring[static_cast<size_t>(c)][static_cast<size_t>(writePos)] = input;
            float shifted = input;
            if (currentDelay > 0.00001f)
            {
                float readPos = static_cast<float>(writePos) - currentDelay;
                if (readPos < 0.0f) readPos += static_cast<float>(ringSize);
                const int a = static_cast<int>(std::floor(readPos));
                const int b = (a + 1) % ringSize;
                const float frac = readPos - static_cast<float>(a);
                shifted = ring[static_cast<size_t>(c)][static_cast<size_t>(a)] * (1.0f - frac)
                        + ring[static_cast<size_t>(c)][static_cast<size_t>(b)] * frac;
            }
            audio[i] = shifted * wet + input * (1.0f - wet);
        }
        writePos = (writePos + 1) % ringSize;
        ++counter;
    }
}

juce::AudioProcessorEditor* RELINKAITimingAlignAudioProcessor::createEditor()
{
    return new RELINKAITimingAlignAudioProcessorEditor(*this);
}

void RELINKAITimingAlignAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, destination);
}
void RELINKAITimingAlignAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RELINKAITimingAlignAudioProcessor();
}
