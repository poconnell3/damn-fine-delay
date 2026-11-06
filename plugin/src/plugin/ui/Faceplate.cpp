#include "Faceplate.h"

namespace dfd::ui
{
namespace
{
void brushed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, int seed)
{
    g.setColour (base);
    g.fillRect (r);
    juce::Random rng (seed);
    for (float y = r.getY(); y < r.getBottom(); y += 1.0f)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.015f + 0.035f * rng.nextFloat()));
        const float x0 = r.getX() + r.getWidth() * 0.3f * rng.nextFloat();
        g.drawHorizontalLine ((int) y, x0, juce::jmin (r.getRight(), x0 + r.getWidth() * (0.5f + 0.5f * rng.nextFloat())));
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(),
                                             juce::Colours::black.withAlpha (0.25f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
}

void bevel (juce::Graphics& g, juce::Rectangle<float> r, bool raised)
{
    const auto light = juce::Colours::white.withAlpha (0.10f), dark = juce::Colours::black.withAlpha (0.7f);
    g.setColour (raised ? light : dark);
    g.drawLine (r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f, 1.0f);
    g.drawLine (r.getX() + 0.5f, r.getY(), r.getX() + 0.5f, r.getBottom(), 1.0f);
    g.setColour (raised ? dark : light);
    g.drawLine (r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, 1.0f);
    g.drawLine (r.getRight() - 0.5f, r.getY(), r.getRight() - 0.5f, r.getBottom(), 1.0f);
}

void thumbScrew (juce::Graphics& g, juce::Point<float> c)
{
    const float r = 7.5f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - r + 1.0f, c.y - r + 1.5f, 2.0f * r, 2.0f * r);
    g.setGradientFill (juce::ColourGradient (colours::chrome, c.x - r, c.y - r, colours::chromeDark.darker (0.4f), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (c.x + (r - 2.0f) * std::cos (a), c.y + (r - 2.0f) * std::sin (a), c.x + r * std::cos (a), c.y + r * std::sin (a), 0.8f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.fillEllipse (c.x - 3.0f, c.y - 3.5f, 3.0f, 2.0f);
}

void ear (juce::Graphics& g, juce::Rectangle<float> r)
{
    for (float y : { 34.0f, (float) layout::kHeight - 34.0f })
    {
        const auto slot = juce::Rectangle<float> (0.0f, 0.0f, 24.0f, 12.0f).withCentre ({ r.getCentreX(), y });
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (slot, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (slot.getX() + 5.0f, slot.getBottom() + 0.5f, slot.getRight() - 5.0f, slot.getBottom() + 0.5f, 1.0f);
    }
}

void bay (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (colours::recess);
    g.fillRoundedRectangle (r, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX(), r.getY() + 10.0f, false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    silkText (g, title, r.withHeight (28.0f).withTrimmedTop (8.0f), 10.0f);
}

void knobLabel (juce::Graphics& g, layout::KnobSpot k) { silkText (g, k.label, layout::knobLabel (k), 11.0f); }
} // namespace

Faceplate::Faceplate()
{
    setOpaque (true);
    setSize (layout::kWidth, layout::kHeight);
}

void Faceplate::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (cache.isNull() || ! juce::approximatelyEqual (scale, cacheScale))
    {
        cache = juce::Image (juce::Image::RGB, juce::roundToInt (layout::kWidth * scale), juce::roundToInt (layout::kHeight * scale), false);
        juce::Graphics cg (cache);
        cg.addTransform (juce::AffineTransform::scale (scale));
        paintStatic (cg);
        cacheScale = scale;
    }
    g.setOpacity (1.0f);
    g.drawImage (cache, getLocalBounds().toFloat());
}

void Faceplate::paintStatic (juce::Graphics& g)
{
    using namespace layout;
    const auto all = juce::Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kHeight);
    brushed (g, all, colours::rack, 7);
    for (float x : { (float) earLeft.getRight(), (float) earRight.getX() })
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, 0.0f, (float) kHeight);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x + 1, 0.0f, (float) kHeight);
    }
    ear (g, earLeft.toFloat());
    ear (g, earRight.toFloat());
    bay (g, inBay.toFloat(), "INPUT");
    bay (g, outBay.toFloat(), "OUTPUT");

    for (auto [plate, seed] : { std::pair { doublerPlate, 5 }, std::pair { leftPlate, 11 }, std::pair { rightPlate, 23 } })
    {
        const auto r = plate.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (r.translated (1.5f, 2.5f));
        brushed (g, r, colours::plate, seed);
        bevel (g, r, true);
    }

    // Left plate: title, maker and captive screws, as on the original's front panel.
    const auto lp = leftPlate.toFloat();
    silkText (g, "DAMN FINE DELAY", lp.withY (lp.getY() + 12.0f).withHeight (22.0f), 18.0f);
    silkText (g, "MODEL 1416", lp.withY (lp.getY() + 34.0f).withHeight (14.0f), 10.5f);
    silkText (g, "DAMN FINE AUDIO", lp.withY (lp.getBottom() - 30.0f).withHeight (18.0f), 13.0f);
    thumbScrew (g, { lp.getX() + 13.0f, lp.getCentreY() });
    thumbScrew (g, { lp.getRight() - 13.0f, lp.getCentreY() });

    // Doubler plate: mode switch, persona switch and amount, then pan over level for the dry signal and the 30 ms cascade.
    const auto dp = doublerPlate.toFloat();
    silkText (g, "MODE", modeTitle, 10.0f, juce::Justification::centredLeft);
    silkText (g, "PERSONA", personaTitle, 10.0f, juce::Justification::centredLeft);
    silkText (g, humanAmount.label, knobLabel (humanAmount), 9.5f, juce::Justification::centred, colours::silkDim);
    silkText (g, "DRY", dryHeader, 12.0f, juce::Justification::centred, colours::amberDim);
    silkText (g, "30 MS", wetHeader, 12.0f, juce::Justification::centred, colours::amberDim);
    g.setColour (colours::silkDim);
    for (int y : { modeDivider, personaDivider })
        g.drawHorizontalLine (y, dp.getX() + 10.0f, dp.getRight() - 10.0f);
    g.drawVerticalLine ((int) dp.getCentreX(), dryHeader.getY(), dp.getBottom() - 4.0f);
    for (auto k : { dryPan, dryLevel, wetPan, wetLevel })
        silkText (g, k.label, knobLabel (k), 9.5f, juce::Justification::centred, colours::silkDim);

    const auto rp = rightPlate.toFloat();
    for (auto c : { rp.getTopLeft() + juce::Point<float> (9.0f, 9.0f), rp.getTopRight() + juce::Point<float> (-9.0f, 9.0f),
                    rp.getBottomLeft() + juce::Point<float> (9.0f, -9.0f), rp.getBottomRight() + juce::Point<float> (-9.0f, -9.0f) })
        drawScrew (g, c, 3.5f);

    for (auto k : { levelA, levelB, drive, mix, output, temperature })
        knobLabel (g, k);
    silkText (g, "OUTPUT LEVEL", vu.toFloat().withY ((float) vu.getBottom() + 6.0f).withHeight (14.0f), 11.0f);
    silkText (g, "ROUTING", routing.toFloat().withY ((float) routing.getBottom() - 16.0f).withHeight (14.0f), 11.0f);

    const char* titles[2][3] = { { "COLOR", "NOISE", "METER" }, { "WET ONLY", "QUALITY", "ACTIVE" } };
    for (int col = 0; col < 2; ++col)
        for (int row = 0; row < 3; ++row)
            silkText (g, titles[col][row], toggleTitle (col, row), 10.0f, juce::Justification::centredLeft);
}

// ------------------------------------------------------------------------------------------- SignalDiagram

namespace
{
constexpr float kRowA = 58.0f, kRowB = 142.0f, kMid = 100.0f;
constexpr float kTerm = 44.0f, kSplit = 54.0f, kPot = 62.0f, kAmp = 92.0f, kTube = 122.0f, kTubeEnd = 214.0f, kAmp2 = 222.0f, kOut = 258.0f;
constexpr float kReturn = kAmp2 + 24.0f; // cascade: A's output turns back to feed B
constexpr float kDryRail = 14.0f, kPanBox = 250.0f, kPanBoxW = 18.0f, kMixOut = 278.0f;
constexpr float kStroke = 1.4f;

double delayMs (double tauNominal, double tempC)
{
    auto c = [] (double t) { return 331.3 * std::sqrt (1.0 + t / 273.15); };
    return 1000.0 * tauNominal * c (20.0) / c (tempC);
}

void wire (juce::Graphics& g, std::initializer_list<juce::Point<float>> points)
{
    juce::Path p;
    bool first = true;
    for (auto pt : points)
    {
        first ? p.startNewSubPath (pt) : p.lineTo (pt);
        first = false;
    }
    g.strokePath (p, juce::PathStrokeType (kStroke, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

void terminal (juce::Graphics& g, juce::Point<float> c)
{
    g.setColour (colours::plate);
    g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7.0f, 7.0f);
    g.setColour (colours::silk);
    g.drawEllipse (c.x - 3.5f, c.y - 3.5f, 7.0f, 7.0f, kStroke);
}

void junction (juce::Graphics& g, juce::Point<float> c) { g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f); }

void twoLineLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& main, const juce::String& sub, juce::Justification j)
{
    silkText (g, main, r.withHeight (12.0f), 9.5f, j);
    silkText (g, sub, r.withTrimmedTop (12.0f).withHeight (11.0f), 8.0f, j, colours::silkDim, false);
}

/** One line: variable drive, send amp, tube, receive amp. */
void line (juce::Graphics& g, float y, double ms, const juce::String& name)
{
    g.setColour (colours::silk);
    wire (g, { { kPot - 8.0f, y }, { kPot, y } });

    juce::Path zig;
    zig.startNewSubPath (kPot, y);
    for (int i = 1; i <= 6; ++i)
        zig.lineTo (kPot + 22.0f * (float) i / 6.0f - (i == 6 ? 0.0f : 1.8f), y + (i == 6 ? 0.0f : (i % 2 ? -4.5f : 4.5f)));
    g.strokePath (zig, juce::PathStrokeType (kStroke));
    juce::Path arrow;
    arrow.addArrow ({ kPot + 2.0f, y + 9.0f, kPot + 21.0f, y - 10.0f }, 1.1f, 6.0f, 6.0f);
    g.fillPath (arrow);

    wire (g, { { kPot + 22.0f, y }, { kAmp, y } });
    for (float x0 : { kAmp, kAmp2 })
    {
        juce::Path tri;
        tri.addTriangle (x0, y - 10.0f, x0, y + 10.0f, x0 + 20.0f, y);
        g.strokePath (tri, juce::PathStrokeType (kStroke));
    }
    wire (g, { { kAmp + 20.0f, y }, { kTube, y } });
    wire (g, { { kTubeEnd, y }, { kAmp2, y } });

    // Tube drawn as a pipe with end rings.
    g.drawLine (kTube + 3.0f, y - 6.0f, kTubeEnd - 3.0f, y - 6.0f, kStroke);
    g.drawLine (kTube + 3.0f, y + 6.0f, kTubeEnd - 3.0f, y + 6.0f, kStroke);
    for (float x : { kTube + 3.0f, kTubeEnd - 3.0f, (kTube + kTubeEnd) * 0.5f })
        g.drawEllipse (x - 3.0f, y - 6.0f, 6.0f, 12.0f, kStroke * 0.8f);
    silkText (g, name + "  " + juce::String (ms, 1) + " ms", { kTube - 4.0f, y - 21.0f, kTubeEnd - kTube + 8.0f, 12.0f }, 9.0f);
}
} // namespace

SignalDiagram::SignalDiagram()
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Signal flow");
}

void SignalDiagram::setState (const State& s)
{
    if (! (s == state))
    {
        state = s;
        repaint();
    }
}

void SignalDiagram::paintDoubler (juce::Graphics& g, double msA, double msB)
{
    const bool monoIn = state.numIn == 1, monoOut = state.numOut == 1;
    g.setColour (colours::silk);

    // Input feeds both the cascade (A into B) and the dry rail across the top.
    wire (g, { { kTerm, kRowA }, { kPot - 8.0f, kRowA } });
    wire (g, { { kSplit, kRowA }, { kSplit, kDryRail }, { kPanBox + kPanBoxW * 0.5f, kDryRail }, { kPanBox + kPanBoxW * 0.5f, kRowA - 14.0f } });
    wire (g, { { kAmp2 + 20.0f, kRowA }, { kReturn, kRowA }, { kReturn, kMid }, { kSplit, kMid }, { kSplit, kRowB }, { kPot - 8.0f, kRowB } });
    wire (g, { { kAmp2 + 20.0f, kRowB }, { kPanBox, kRowB } });
    junction (g, { kSplit, kRowA });
    terminal (g, { kTerm, kRowA });
    twoLineLabel (g, { 0.0f, kRowA - 12.0f, kTerm - 7.0f, 24.0f }, "IN", monoIn ? "MONO" : "L + R", juce::Justification::centredRight);
    silkText (g, "DRY", { kTube, kDryRail - 12.0f, kTubeEnd - kTube, 10.0f }, 8.5f);

    // Pan and level block, then the two outputs.
    const auto box = juce::Rectangle<float> (kPanBox, kRowA - 14.0f, kPanBoxW, kRowB - kRowA + 28.0f);
    g.drawRect (box, kStroke);
    for (int i = 0; i < 3; ++i)
        silkText (g, juce::String::charToString ("PAN"[i]), { box.getX(), box.getCentreY() - 17.0f + 12.0f * (float) i, box.getWidth(), 10.0f }, 8.5f);

    auto sources = [] (bool dry, bool wet) { return dry && wet ? juce::String ("DRY+DLY") : dry ? juce::String ("DRY") : wet ? juce::String ("DLY") : juce::String ("-"); };
    const juce::String leftSub = monoOut ? "MONO" : sources (state.dryPan <= 0.0f, state.wetPan <= 0.0f);
    const juce::String rightSub = monoOut ? "UNUSED" : sources (state.dryPan >= 0.0f, state.wetPan >= 0.0f);
    for (auto [y, name, sub] : { std::tuple { kRowA, "L", leftSub }, std::tuple { kRowB, "R", rightSub } })
    {
        g.setColour (colours::silk);
        wire (g, { { box.getRight(), y }, { kMixOut, y } });
        terminal (g, { kMixOut, y });
        silkText (g, name, { kMixOut + 7.0f, y - 12.0f, 40.0f, 12.0f }, 9.5f, juce::Justification::centredLeft);
        silkText (g, sub, { kMixOut + 7.0f, y, 40.0f, 11.0f }, 7.5f, juce::Justification::centredLeft, colours::silkDim, false);
    }

    const auto dashedBox = juce::Rectangle<float> (kTube - 8.0f, kRowA - 28.0f, kTubeEnd - kTube + 16.0f, kRowB - kRowA + 50.0f);
    const auto caption = dashedBox.withY (dashedBox.getBottom() + 5.0f).withHeight (12.0f).expanded (40.0f, 0.0f);
    silkText (g, "DOUBLER  DRY AND " + juce::String (msA + msB, 1) + " ms", caption, 8.5f, juce::Justification::centred, colours::silkDim, false);
    if (state.human)
        silkText (g, "HUMAN TIMING  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (state.humanMs, 1) + " ms",
                  caption.translated (0.0f, 13.0f), 8.5f, juce::Justification::centred, colours::amberDim);
}

void SignalDiagram::paint (juce::Graphics& g)
{
    const double t = state.temperatureC;
    const double msA = delayMs (0.016, t), msB = delayMs (0.014, t);
    const bool monoIn = state.numIn == 1, monoOut = state.numOut == 1;
    const auto leftLabel = [&] (float y, const juce::String& main, const juce::String& sub)
    { twoLineLabel (g, { 0.0f, y - 12.0f, kTerm - 7.0f, 24.0f }, main, sub, juce::Justification::centredRight); };
    const auto rightLabel = [&] (float y, const juce::String& main, const juce::String& sub)
    { twoLineLabel (g, { kOut + 7.0f, y - 12.0f, (float) getWidth() - kOut - 7.0f, 24.0f }, main, sub, juce::Justification::centredLeft); };

    // Dashed enclosure around the acoustic lines.
    const auto box = juce::Rectangle<float> (kTube - 8.0f, kRowA - 28.0f, kTubeEnd - kTube + 16.0f, kRowB - kRowA + 50.0f);
    juce::Path outline, dashed;
    outline.addRectangle (box);
    const float dashes[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (colours::silk);
    g.fillPath (dashed);

    line (g, kRowA, msA, "A");
    line (g, kRowB, msB, "B");
    g.setColour (colours::silk);
    if (state.doubler)
    {
        paintDoubler (g, msA, msB);
        return;
    }

    const juce::String bothIn = monoIn ? "MONO" : "L + R";
    switch (state.routing)
    {
        case 1: // dual mono
            for (auto [y, name, side] : { std::tuple { kRowA, "A IN", "LEFT" }, std::tuple { kRowB, "B IN", "RIGHT" } })
            {
                g.setColour (colours::silk);
                wire (g, { { kTerm, y }, { kPot - 8.0f, y } });
                terminal (g, { kTerm, y });
                leftLabel (y, name, monoIn ? "MONO" : side);
            }
            break;
        case 2: // cascade: A's output feeds B
            wire (g, { { kTerm, kRowA }, { kPot - 8.0f, kRowA } });
            wire (g, { { kAmp2 + 20.0f, kRowA }, { kReturn, kRowA }, { kReturn, kMid }, { kSplit, kMid }, { kSplit, kRowB }, { kPot - 8.0f, kRowB } });
            terminal (g, { kTerm, kRowA });
            leftLabel (kRowA, "IN", bothIn);
            break;
        default: // widener: one input feeds both lines
            wire (g, { { kTerm, kMid }, { kSplit, kMid } });
            wire (g, { { kSplit, kRowA }, { kSplit, kRowB } });
            wire (g, { { kSplit, kRowA }, { kPot - 8.0f, kRowA } });
            wire (g, { { kSplit, kRowB }, { kPot - 8.0f, kRowB } });
            junction (g, { kSplit, kMid });
            terminal (g, { kTerm, kMid });
            leftLabel (kMid, "IN", bothIn);
            break;
    }

    g.setColour (colours::silk);
    if (state.routing == 2)
    {
        wire (g, { { kAmp2 + 20.0f, kRowB }, { kOut, kRowB } });
        terminal (g, { kOut, kRowB });
        rightLabel (kRowB, "OUT", monoOut ? "MONO" : "L + R");
    }
    else
    {
        for (auto [y, name, side, isB] : { std::tuple { kRowA, "A OUT", "LEFT", false }, std::tuple { kRowB, "B OUT", "RIGHT", true } })
        {
            g.setColour (colours::silk);
            wire (g, { { kAmp2 + 20.0f, y }, { kOut, y } });
            terminal (g, { kOut, y });
            rightLabel (y, name, monoOut ? (isB ? "UNUSED" : "MONO") : side); // a mono output carries line A only
        }
    }

    const juce::String caption = state.routing == 2 ? "A INTO B  " + juce::String (msA + msB, 1) + " ms"
                                                    : "ACOUSTIC LINES  " + juce::String (t, 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb0" "C"));
    silkText (g, caption, box.withY (box.getBottom() + 5.0f).withHeight (12.0f).expanded (30.0f, 0.0f), 8.5f,
              juce::Justification::centred, colours::silkDim, false);
}
} // namespace dfd::ui
