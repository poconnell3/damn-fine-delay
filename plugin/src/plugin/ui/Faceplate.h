#pragma once

#include "Theme.h"

namespace rrd::ui
{
/** The rack face: ears, meter bays, brushed plates, screws and all static silkscreen. Cached per pixel scale. */
class Faceplate final : public juce::Component
{
public:
    Faceplate();
    void paint (juce::Graphics&) override;

private:
    juce::Image cache;
    float cacheScale = 0.0f;

    static void paintStatic (juce::Graphics&);
};

/** Silkscreen block diagram of the current routing, with the delay times at the current room temperature. */
class SignalDiagram final : public juce::Component
{
public:
    struct State
    {
        int routing = 0; // 0 widener, 1 dual mono, 2 cascade
        float temperatureC = 20.0f;
        int numIn = 2, numOut = 2;
        bool doubler = false;
        float dryPan = -0.6f, wetPan = 0.6f; // -1 left .. +1 right
        bool human = false;
        float humanMs = 3.0f;
        bool operator== (const State&) const = default;
    };

    SignalDiagram();
    void setState (const State&);
    void paint (juce::Graphics&) override;

private:
    State state;

    void paintDoubler (juce::Graphics&, double msA, double msB);
};
} // namespace rrd::ui
