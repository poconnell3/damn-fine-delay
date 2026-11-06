#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"

namespace dfd::ui
{
/** Knurled black knobs with silkscreen ticks, dark menus and small value readouts. */
class RackLook final : public juce::LookAndFeel_V4
{
public:
    RackLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

/** Bat-handle toggle with 2 or 3 positions. Legends are listed top to bottom (vertical) or left to right (horizontal). */
class ToggleSwitch final : public juce::Component
{
public:
    enum class Orientation { vertical, horizontal };

    explicit ToggleSwitch (juce::StringArray legends, Orientation = Orientation::vertical);

    int getPosition() const noexcept { return position; }
    void setPosition (int newPosition, juce::NotificationType);
    std::function<void (int)> onChange;

    /** Drives a choice or bool parameter; paramIndexForPosition maps each position to the parameter's index. */
    void attach (juce::RangedAudioParameter&, std::vector<int> paramIndexForPosition);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray legends;
    Orientation orientation;
    int position = 0;
    std::vector<int> paramIndex;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    juce::Point<float> nutCentre() const;
    float throwLength() const;
    void paintLegends (juce::Graphics&, juce::Point<float> nut, float len) const;
    void paintLever (juce::Graphics&, juce::Point<float> nut, juce::Point<float> direction, float len) const;
};

/** Latching illuminated push button (grey housing, translucent red cap), attached to a bool parameter. */
class LampButton final : public juce::Component
{
public:
    explicit LampButton (juce::String text);

    void attach (juce::RangedAudioParameter&);
    bool isOn() const noexcept { return on; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String text;
    bool on = false, pressed = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Pointer knob that snaps between labelled positions spread over the top arc. */
class RotarySelector final : public juce::Component
{
public:
    RotarySelector (juce::RangedAudioParameter&, juce::StringArray shortLegends);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::StringArray legends;
    int index = 0, dragStartIndex = 0;
    juce::ParameterAttachment attachment;

    juce::Point<float> centre() const;
    float angleFor (int i) const;
    void select (int i);
};

/** Jewel pilot lamp. */
class PilotLamp final : public juce::Component
{
public:
    void setLit (bool shouldBeLit);
    void paint (juce::Graphics&) override;

private:
    bool lit = false;
};
} // namespace dfd::ui
