#pragma once

#include <array>

#include "Theme.h"

namespace rrd::ui
{
/** One or two vertical peak meters (IEC 60268-18 scale) with peak hold and clip lamps. Click to reset. */
class LevelMeterBay final : public juce::Component
{
public:
    explicit LevelMeterBay (bool scaleOnRight);

    /** One label per meter: one for mono, two for stereo. */
    void setChannelLabels (const juce::StringArray&);
    void update (std::array<float, 2> peaks, double seconds);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Channel
    {
        float levelDb = -100.0f, holdDb = -100.0f;
        double holdAge = 0.0, clipAge = 1e9;
    };

    const bool scaleRight;
    juce::StringArray labels { "L", "R" };
    std::array<Channel, 2> channels;

    juce::Rectangle<float> barArea() const;
    juce::Rectangle<float> bar (int index) const;
};

/** Needle VU meter: 0 VU = -18 dBFS, IEC 60268-17 ballistics. */
class VuMeter final : public juce::Component
{
public:
    VuMeter();

    /** rms: linear RMS of the metered channel over the last `seconds`. */
    void update (float rms, double seconds);
    void setChannelName (const juce::String&);

    void paint (juce::Graphics&) override;

private:
    double position = 0.0, velocity = 0.0, target = 0.0;
    juce::String channelName { "A" };
    juce::Image faceCache;
    float faceCacheScale = 0.0f;

    juce::Rectangle<float> face() const;
    juce::Point<float> pivot() const;
    float scaleRadius() const;
    void paintFace (juce::Graphics&) const;
};
} // namespace rrd::ui
