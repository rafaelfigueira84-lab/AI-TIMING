#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class RELINKAITimingAlignAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     private juce::Timer
{
public:
    explicit RELINKAITimingAlignAudioProcessorEditor(RELINKAITimingAlignAudioProcessor&);
    ~RELINKAITimingAlignAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    RELINKAITimingAlignAudioProcessor& processor;
    juce::Label subtitle, offsetDisplay, confidenceDisplay, statusDisplay;
    juce::ToggleButton autoButton;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    std::unique_ptr<ButtonAttachment> autoAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::array<Knob, 6> knobs;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RELINKAITimingAlignAudioProcessorEditor)
};
