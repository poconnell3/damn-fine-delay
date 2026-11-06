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

/** Bat-handle toggle with 2 or 3 positions; legends listed top to bottom. */
class ToggleSwitch final : public juce::Component
{
public:
    explicit ToggleSwitch (juce::StringArray legendsTopToBottom);

    int getPosition() const noexcept { return position; }
    void setPosition (int newPosition, juce::NotificationType);
    std::function<void (int)> onChange;

    /** Drives a choice or bool parameter; paramIndexForPosition maps each position to the parameter's index. */
    void attach (juce::RangedAudioParameter&, std::vector<int> paramIndexForPosition);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray legends;
    int position = 0;
    std::vector<int> paramIndex;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    juce::Point<float> nutCentre() const;
    float throwLength() const;
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
