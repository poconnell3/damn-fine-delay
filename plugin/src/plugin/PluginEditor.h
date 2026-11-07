#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/Faceplate.h"
#include "ui/Meters.h"

/** Rack-panel editor drawn at a fixed design size and scaled as a whole (fixed aspect ratio). */
class RedRoomDelayEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit RedRoomDelayEditor (RedRoomDelayProcessor&);
    ~RedRoomDelayEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    RedRoomDelayProcessor& processor;
    rrd::ui::RackLook look;
    rrd::ui::Faceplate face;
    rrd::ui::SignalDiagram diagram;
    rrd::ui::LevelMeterBay inMeters { true }, outMeters { false };
    rrd::ui::VuMeter vu;
    std::array<Knob, 11> knobs; // 0-5 normal mode, 6-9 doubler (dry pan, dry level, delay pan, delay level), 10 human amount
    rrd::ui::ToggleSwitch color { { "A", "B" } }, wetOnly { { "ON", "OFF" } };
    rrd::ui::ToggleSwitch noise { { "XTRA", "AUTH", "OFF" } }, quality { { "4x", "2x", "1x" } };
    rrd::ui::ToggleSwitch meterSelect { { "A", "B" } };
    rrd::ui::LampButton doubleButton { "DOUBLE" }, humanizeButton { "HUMANIZE" };
    rrd::ui::RotarySelector routing;
    rrd::ui::PilotLamp lamp;

    std::uint32_t lastBlocks = 0;
    double lastActiveMs = -1e9, lastTickMs = 0.0;
    bool sizeRestored = false;
    int shownMode = -1;

    void timerCallback() override;
    void addKnob (Knob&, const char* paramId, rrd::ui::layout::KnobSpot);
    void showMode (bool doubler, bool human);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedRoomDelayEditor)
};
