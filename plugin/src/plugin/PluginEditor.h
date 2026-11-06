#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

/** Functional v0.1 editor: two rows of rotary controls and the mode selectors, on a plain rack-panel look. */
class DamnFineDelayEditor final : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit DamnFineDelayEditor (DamnFineDelayProcessor&);
    ~DamnFineDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    class PanelLook final : public juce::LookAndFeel_V4
    {
    public:
        PanelLook();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    };

    DamnFineDelayProcessor& processor;
    PanelLook look;
    std::array<Knob, 6> knobs;
    std::array<Choice, 4> choices;
    juce::Label status;

    void timerCallback() override;

    void setUpKnob (Knob&, const char* paramId, const juce::String& name);
    void setUpChoice (Choice&, const char* paramId, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DamnFineDelayEditor)
};
