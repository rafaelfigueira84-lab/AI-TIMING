#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class RELINKAITimingAlignAudioProcessor final : public juce::AudioProcessor
{
public:
    RELINKAITimingAlignAudioProcessor();
    ~RELINKAITimingAlignAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    std::atomic<float> observedOffsetMs { 0.0f };
    std::atomic<float> confidence { 0.0f };
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    struct Detector
    {
        float envelope = 0.0f, previousEnvelope = 0.0f;
        int64_t lastHit = -100000000;
        int64_t detect(float amplitude, int64_t sample, float alpha, int64_t cooldown, float threshold);
    };
    Detector targetDetector, referenceDetector;
    double sr = 44100.0;
    int baseSamples = 0, ringSize = 0, writePos = 0;
    int64_t counter = 0, lastRefHit = -100000000, lastTargetHit = -100000000;
    int64_t usedRefHit = -100000000, usedTargetHit = -100000000;
    std::array<std::vector<float>, 2> ring;
    float trackedOffsetSamples = 0.0f, currentDelay = 0.0f;
    int stablePairs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RELINKAITimingAlignAudioProcessor)
};
