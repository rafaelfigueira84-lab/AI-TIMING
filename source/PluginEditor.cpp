#include "PluginEditor.h"

namespace
{
constexpr juce::uint32 background = 0xff0d1320;
constexpr juce::uint32 panel = 0xff192334;
constexpr juce::uint32 accent = 0xff38d9bd;
constexpr juce::uint32 text = 0xffebf3fc;
constexpr juce::uint32 muted = 0xff9bacc1;
}

RELINKAITimingAlignAudioProcessorEditor::RELINKAITimingAlignAudioProcessorEditor(RELINKAITimingAlignAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(660, 390);
    setResizable(false, false);
    subtitle.setText("RELINK  /  AI TIMING ALIGN", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(text));
    subtitle.setFont(juce::Font(22.0f, juce::Font::bold));
    addAndMakeVisible(subtitle);

    autoButton.setButtonText("AUTO DETECT");
    autoButton.setColour(juce::ToggleButton::textColourId, juce::Colour(text));
    addAndMakeVisible(autoButton);
    autoAttachment = std::make_unique<ButtonAttachment>(processor.state, "auto", autoButton);

    modeBox.addItemList({ "Tight", "Natural", "Groove" }, 1);
    modeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(panel));
    modeBox.setColour(juce::ComboBox::textColourId, juce::Colour(text));
    addAndMakeVisible(modeBox);
    modeAttachment = std::make_unique<ComboAttachment>(processor.state, "mode", modeBox);
    modeLabel.setText("MODE", juce::dontSendNotification);
    modeLabel.setColour(juce::Label::textColourId, juce::Colour(muted));
    addAndMakeVisible(modeLabel);

    const std::array<const char*, 6> names { "STRENGTH", "MANUAL ms", "GROOVE ms", "THRESHOLD", "MATCH ms", "WET" };
    const std::array<const char*, 6> ids { "strength", "manual", "groove", "threshold", "maxmatch", "mix" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 21);
        k.slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(accent));
        k.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(panel));
        k.slider.setColour(juce::Slider::thumbColourId, juce::Colour(accent));
        k.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(text));
        k.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(panel));
        addAndMakeVisible(k.slider);
        k.label.setText(names[i], juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setColour(juce::Label::textColourId, juce::Colour(muted));
        addAndMakeVisible(k.label);
        k.attachment = std::make_unique<SliderAttachment>(processor.state, ids[i], k.slider);
    }
    for (auto* label : { &offsetDisplay, &confidenceDisplay, &statusDisplay })
    {
        label->setColour(juce::Label::textColourId, juce::Colour(text));
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*label);
    }
    offsetDisplay.setFont(juce::Font(25.0f, juce::Font::bold));
    confidenceDisplay.setFont(juce::Font(18.0f));
    statusDisplay.setColour(juce::Label::textColourId, juce::Colour(muted));
    statusDisplay.setFont(juce::Font(12.0f));
    startTimerHz(10);
}

void RELINKAITimingAlignAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(background));
    g.setColour(juce::Colour(panel));
    g.fillRoundedRectangle(18.0f, 82.0f, 624.0f, 72.0f, 10.0f);
    g.setColour(juce::Colour(accent));
    g.fillRoundedRectangle(18.0f, 82.0f, 4.0f, 72.0f, 2.0f);
    g.setColour(juce::Colour(muted));
    g.setFont(12.0f);
    g.drawText("DETECTED OFFSET", 36, 92, 220, 19, juce::Justification::centredLeft);
    g.drawText("CONFIDENCE", 290, 92, 175, 19, juce::Justification::centredLeft);
    g.setColour(juce::Colour(muted));
    g.drawText("Insert on target track  |  Route kick to sidechain  |  v0.2.1 SAFE / ZERO LATENCY", 22, 359, 615, 22,
               juce::Justification::centredLeft);
}

void RELINKAITimingAlignAudioProcessorEditor::resized()
{
    subtitle.setBounds(20, 15, 370, 39);
    autoButton.setBounds(415, 23, 132, 27);
    modeLabel.setBounds(18, 57, 56, 20);
    modeBox.setBounds(78, 56, 142, 23);
    offsetDisplay.setBounds(35, 111, 231, 38);
    confidenceDisplay.setBounds(290, 113, 156, 34);
    statusDisplay.setBounds(457, 108, 165, 35);
    const int x0 = 12;
    const int step = 106;
    for (int i = 0; i < static_cast<int>(knobs.size()); ++i)
    {
        auto& k = knobs[static_cast<size_t>(i)];
        k.label.setBounds(x0 + step * i, 180, 103, 25);
        k.slider.setBounds(x0 + step * i, 207, 103, 137);
    }
}

void RELINKAITimingAlignAudioProcessorEditor::timerCallback()
{
    const auto offset = processor.observedOffsetMs.load();
    const auto conf = processor.confidence.load();
    offsetDisplay.setText(juce::String(offset, 1) + " ms", juce::dontSendNotification);
    confidenceDisplay.setText(juce::String(juce::roundToInt(conf * 100.0f)) + " %", juce::dontSendNotification);
    statusDisplay.setText(conf >= 0.5f ? "REFERENCE TRACKED" : "WAITING FOR PAIRS", juce::dontSendNotification);
}
