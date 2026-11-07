#include "Meters.h"

namespace rrd::ui
{
namespace
{
constexpr float kReleaseDbPerSecond = 24.0f;
constexpr double kHoldSeconds = 1.5, kClipSeconds = 3.0;

/** IEC 60268-18 deflection, 0..1. */
float iecDeflection (float db)
{
    if (db < -70.0f) return 0.0f;
    if (db < -60.0f) return (db + 70.0f) * 0.0025f;
    if (db < -50.0f) return (db + 60.0f) * 0.005f + 0.025f;
    if (db < -40.0f) return (db + 50.0f) * 0.0075f + 0.075f;
    if (db < -30.0f) return (db + 40.0f) * 0.015f + 0.15f;
    if (db < -20.0f) return (db + 30.0f) * 0.02f + 0.3f;
    return juce::jmin (1.0f, (db + 20.0f) * 0.025f + 0.5f);
}

juce::Colour zoneColour (float db) { return db >= -1.0f ? colours::red : (db >= -6.0f ? colours::yellow : colours::amber); }

// VU: 0 VU = -18 dBFS (sine-referenced RMS); +3 VU is full scale. Deflection is linear in voltage.
constexpr double kVuAlignment = 0.12589254117941673; // -18 dB
constexpr double kVuFullScale = 1.4125375446227544;  // +3 VU
constexpr float kVuHalfAngle = 0.82f;                 // 47 degrees
constexpr double kNeedleOmega = 13.0, kNeedleDamping = 0.8; // 99 % in 300 ms, 1.5 % overshoot

float vuDeflection (float vu) { return (float) (std::pow (10.0, vu / 20.0) / kVuFullScale); }
float vuAngle (float deflection) { return -kVuHalfAngle + 2.0f * kVuHalfAngle * deflection; }

juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }
} // namespace

// ------------------------------------------------------------------------------------------- LevelMeterBay

LevelMeterBay::LevelMeterBay (bool scaleOnRight) : scaleRight (scaleOnRight)
{
    setTitle ("Level meters");
}

void LevelMeterBay::setChannelLabels (const juce::StringArray& newLabels)
{
    jassert (newLabels.size() == 1 || newLabels.size() == 2);
    if (newLabels != labels)
    {
        labels = newLabels;
        repaint();
    }
}

void LevelMeterBay::update (std::array<float, 2> peaks, double seconds)
{
    for (size_t ch = 0; ch < channels.size(); ++ch)
    {
        auto& c = channels[ch];
        const float db = juce::Decibels::gainToDecibels (peaks[ch], -100.0f);
        c.levelDb = juce::jmax (db, c.levelDb - kReleaseDbPerSecond * (float) seconds);
        if (db >= c.holdDb)
        {
            c.holdDb = db;
            c.holdAge = 0.0;
        }
        else if ((c.holdAge += seconds) > kHoldSeconds)
            c.holdDb = juce::jmax (c.levelDb, c.holdDb - kReleaseDbPerSecond * (float) seconds);
        c.clipAge = peaks[ch] >= 0.999f ? 0.0 : c.clipAge + seconds;
    }
    repaint();
}

void LevelMeterBay::mouseDown (const juce::MouseEvent&)
{
    for (auto& c : channels)
    {
        c.holdDb = c.levelDb;
        c.clipAge = 1e9;
    }
    repaint();
}

juce::Rectangle<float> LevelMeterBay::barArea() const
{
    const auto b = getLocalBounds().toFloat();
    return { scaleRight ? 2.0f : 22.0f, 16.0f, b.getWidth() - 24.0f, b.getHeight() - 38.0f };
}

juce::Rectangle<float> LevelMeterBay::bar (int index) const
{
    const auto a = barArea();
    if (labels.size() == 1)
        return a.withSizeKeepingCentre (13.0f, a.getHeight());
    const float w = 11.0f, gap = 6.0f, x0 = a.getCentreX() - w - gap * 0.5f;
    return { x0 + (float) index * (w + gap), a.getY(), w, a.getHeight() };
}

void LevelMeterBay::paint (juce::Graphics& g)
{
    const auto area = barArea();
    auto yFor = [&] (float db) { return area.getBottom() - iecDeflection (db) * area.getHeight(); };

    for (float db : { 0.0f, -3.0f, -6.0f, -10.0f, -20.0f, -30.0f, -40.0f, -60.0f })
    {
        const float y = yFor (db);
        const float x = scaleRight ? area.getRight() + 1.0f : 1.0f;
        g.setColour (colours::silkDim);
        g.drawHorizontalLine (juce::roundToInt (y), scaleRight ? area.getRight() - 1.0f : area.getX() - 3.0f,
                              scaleRight ? area.getRight() + 3.0f : area.getX() + 1.0f);
        silkText (g, juce::String (juce::roundToInt (-db)), { x + (scaleRight ? 3.0f : 0.0f), y - 5.0f, 17.0f, 10.0f }, 8.5f,
                  scaleRight ? juce::Justification::centredLeft : juce::Justification::centredRight, colours::silkDim, false);
    }

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto& c = channels[(size_t) i];
        const auto b = bar (i);

        const auto lamp = juce::Rectangle<float> (b.getX(), 3.0f, b.getWidth(), 7.0f);
        const bool clipped = c.clipAge < kClipSeconds;
        g.setColour (clipped ? colours::red : juce::Colour (0xff3a1410));
        g.fillRoundedRectangle (lamp, 1.5f);
        if (clipped)
        {
            g.setColour (colours::red.withAlpha (0.35f));
            g.fillRoundedRectangle (lamp.expanded (2.0f), 3.0f);
        }

        g.setColour (juce::Colours::black);
        g.fillRect (b.expanded (1.5f));
        g.setColour (juce::Colour (0xff1a1205));
        g.fillRect (b);

        const float top = yFor (c.levelDb);
        if (top < b.getBottom())
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (b.withTop (top).toNearestIntEdges());
            g.setColour (colours::amber);
            g.fillRect (b.withTop (yFor (-6.0f)));
            g.setColour (colours::yellow);
            g.fillRect (b.withTop (yFor (-1.0f)).withBottom (yFor (-6.0f)));
            g.setColour (colours::red);
            g.fillRect (b.withBottom (yFor (-1.0f)));
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        for (float y = b.getBottom() - 3.0f; y > b.getY(); y -= 3.0f)
            g.fillRect (b.getX(), y, b.getWidth(), 1.0f);

        if (c.holdDb > -60.0f)
        {
            g.setColour (zoneColour (c.holdDb));
            g.fillRect (b.getX(), yFor (c.holdDb) - 1.0f, b.getWidth(), 2.0f);
        }

        silkText (g, labels[i], { b.getCentreX() - 14.0f, b.getBottom() + 5.0f, 28.0f, 14.0f }, 11.0f);
    }
}

// ------------------------------------------------------------------------------------------- VuMeter

VuMeter::VuMeter()
{
    setTitle ("VU meter");
    setInterceptsMouseClicks (false, false);
}

void VuMeter::setChannelName (const juce::String& name)
{
    if (name != channelName)
    {
        channelName = name;
        repaint();
    }
}

void VuMeter::update (float rms, double seconds)
{
    target = juce::jlimit (0.0, 1.06, (double) rms * juce::MathConstants<double>::sqrt2 / kVuAlignment / kVuFullScale);
    const int steps = juce::jmax (1, (int) std::ceil (seconds / 0.001));
    const double dt = seconds / steps;
    for (int i = 0; i < steps; ++i)
    {
        velocity += dt * (kNeedleOmega * kNeedleOmega * (target - position) - 2.0 * kNeedleDamping * kNeedleOmega * velocity);
        position = juce::jlimit (-0.02, 1.08, position + dt * velocity);
    }
    repaint (face().toNearestInt().expanded (1));
}

juce::Rectangle<float> VuMeter::face() const { return getLocalBounds().toFloat().reduced (9.0f); }
juce::Point<float> VuMeter::pivot() const { return { face().getCentreX(), face().getBottom() + 30.0f }; }
float VuMeter::scaleRadius() const { return pivot().y - (face().getY() + 36.0f); }

void VuMeter::paintFace (juce::Graphics& g) const
{
    const auto f = face();
    const auto p = pivot();
    const float rs = scaleRadius();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6e9c6), f.getX(), f.getY(), juce::Colour (0xffe6c886), f.getX(), f.getBottom(), false));
    g.fillRect (f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc35a).withAlpha (0.45f), f.getCentreX(), f.getBottom(),
                                             juce::Colour (0xffffc35a).withAlpha (0.0f), f.getCentreX(), f.getY(), true));
    g.fillRect (f);

    const juce::Colour ink (0xff1b1a17);
    juce::Path arc;
    arc.addCentredArc (p.x, p.y, rs, rs, 0.0f, vuAngle (0.0f), vuAngle (vuDeflection (0.0f)), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.2f));
    juce::Path red;
    red.addCentredArc (p.x, p.y, rs + 2.0f, rs + 2.0f, 0.0f, vuAngle (vuDeflection (0.0f)), vuAngle (1.0f), true);
    g.setColour (colours::red);
    g.strokePath (red, juce::PathStrokeType (4.0f));

    for (float vu : { -9.0f, -8.0f, -6.0f, -4.0f, -2.0f })
    {
        const float a = vuAngle (vuDeflection (vu));
        g.setColour (ink);
        g.drawLine ({ polar (p, rs, a), polar (p, rs + 4.0f, a) }, 1.0f);
    }
    for (float vu : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = vuAngle (vuDeflection (vu));
        g.setColour (vu > 0.0f ? colours::red.darker (0.2f) : ink);
        g.drawLine ({ polar (p, rs, a), polar (p, rs + 8.0f, a) }, 1.3f);
        const auto t = polar (p, rs + 15.0f, a);
        const juce::String text = vu > 0.0f ? "+" + juce::String ((int) vu) : juce::String ((int) std::abs (vu));
        silkText (g, text, { t.x - 12.0f, t.y - 6.0f, 24.0f, 12.0f }, 9.5f, juce::Justification::centred,
                  vu > 0.0f ? colours::red.darker (0.2f) : ink);
    }
    for (int pct = 20; pct <= 100; pct += 20)
    {
        const float a = vuAngle ((float) pct / 100.0f * vuDeflection (0.0f));
        g.setColour (ink);
        g.drawLine ({ polar (p, rs, a), polar (p, rs - 4.0f, a) }, 1.0f);
        const auto t = polar (p, rs - 12.0f, a);
        silkText (g, juce::String (pct), { t.x - 12.0f, t.y - 5.0f, 24.0f, 10.0f }, 7.5f, juce::Justification::centred, ink.withAlpha (0.75f), false);
    }

    silkText (g, "VU", { f.getX() + 8.0f, f.getY() + 4.0f, 30.0f, 14.0f }, 12.0f, juce::Justification::centredLeft, ink);
    silkText (g, "VU", { f.getRight() - 38.0f, f.getY() + 4.0f, 30.0f, 14.0f }, 12.0f, juce::Justification::centredRight, ink);
    silkText (g, "FWA", { f.getCentreX() - 30.0f, f.getY() + 64.0f, 60.0f, 14.0f }, 11.0f, juce::Justification::centred, ink.withAlpha (0.55f));

    // Base plate hiding the needle pivot.
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (f.toNearestIntEdges());
    const float baseR = 44.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3a37), p.x, p.y - baseR, juce::Colour (0xff141412), p.x, f.getBottom(), false));
    g.fillEllipse (p.x - baseR * 1.6f, p.y - baseR, baseR * 3.2f, baseR * 2.0f);
}

void VuMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2c2f), 0.0f, 0.0f, juce::Colour (0xff050506), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    const auto f = face();
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (faceCache.isNull() || ! juce::approximatelyEqual (scale, faceCacheScale))
    {
        faceCache = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (f.getWidth() * scale)),
                                 juce::jmax (1, juce::roundToInt (f.getHeight() * scale)), true);
        juce::Graphics fg (faceCache);
        fg.addTransform (juce::AffineTransform::translation (-f.getX(), -f.getY()).scaled (scale));
        paintFace (fg);
        faceCacheScale = scale;
    }
    g.setOpacity (1.0f);
    g.drawImage (faceCache, f);

    silkText (g, "METER " + channelName, { f.getCentreX() - 40.0f, f.getBottom() - 15.0f, 80.0f, 12.0f }, 8.5f,
              juce::Justification::centred, colours::silkDim);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (f.toNearestIntEdges());
        const auto p = pivot();
        const float a = vuAngle ((float) position);
        const juce::Line<float> needle (polar (p, 46.0f, a), polar (p, scaleRadius() + 10.0f, a));
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.drawLine (needle.getStart().x + 3.0f, needle.getStart().y + 2.0f, needle.getEnd().x + 3.0f, needle.getEnd().y + 2.0f, 1.4f);
        g.setColour (juce::Colour (0xff121110));
        g.drawLine (needle, 1.4f);

        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), f.getX(), f.getY(),
                                                 juce::Colours::transparentBlack, f.getX(), f.getY() + 8.0f, false));
        g.fillRect (f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), f.getX(), f.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), f.getX() + f.getWidth() * 0.35f, f.getBottom(), false));
        g.fillRect (f);
    }
}
} // namespace rrd::ui
