#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/Faceplate.h"
#include "ui/Meters.h"

/** Rack-panel editor drawn at a fixed design size and scaled as a whole (fixed aspect ratio). */
class DamnFineDelayEditor final : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit DamnFineDelayEditor (DamnFineDelayProcessor&);
    ~DamnFineDelayEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    DamnFineDelayProcessor& processor;
    dfd::ui::RackLook look;
    dfd::ui::Faceplate face;
    dfd::ui::SignalDiagram diagram;
    dfd::ui::LevelMeterBay inMeters { true }, outMeters { false };
    dfd::ui::VuMeter vu;
    std::array<Knob, 11> knobs; // 0-5 normal mode, 6-9 doubler (dry pan, dry level, delay pan, delay level), 10 human amount
    dfd::ui::ToggleSwitch color { { "A", "B" } }, wetOnly { { "ON", "OFF" } };
    dfd::ui::ToggleSwitch noise { { "XTRA", "AUTH", "OFF" } }, quality { { "4x", "2x", "1x" } };
    dfd::ui::ToggleSwitch meterSelect { { "A", "B" } };
    dfd::ui::LampButton doubleButton { "DOUBLE" }, humanizeButton { "HUMANIZE" };
    dfd::ui::RotarySelector routing;
    dfd::ui::PilotLamp lamp;

    std::uint32_t lastBlocks = 0;
    double lastActiveMs = -1e9, lastTickMs = 0.0;
    bool sizeRestored = false;
    int shownMode = -1;

    void timerCallback() override;
    void addKnob (Knob&, const char* paramId, dfd::ui::layout::KnobSpot);
    void showMode (bool doubler, bool human);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DamnFineDelayEditor)
};
