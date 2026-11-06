#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

/** Shared look and fixed layout of the rack face. The editor is drawn at kWidth x kHeight and scaled as a whole. */
namespace dfd::ui
{
namespace colours
{
    inline const juce::Colour rack { 0xff16171a };
    inline const juce::Colour plate { 0xff1e2023 };
    inline const juce::Colour plateEdge { 0xff34373b };
    inline const juce::Colour recess { 0xff0d0e10 };
    inline const juce::Colour silk { 0xffeae6dc };
    inline const juce::Colour silkDim { 0xff8f8c86 };
    inline const juce::Colour amber { 0xffffb04a };
    inline const juce::Colour amberDim { 0xffd6a35c };
    inline const juce::Colour yellow { 0xffffd65a };
    inline const juce::Colour red { 0xffe8432f };
    inline const juce::Colour chrome { 0xffd4d7da };
    inline const juce::Colour chromeDark { 0xff6c7075 };
} // namespace colours

inline juce::Font silkFont (float height, bool bold = true)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (0.08f));
}

inline void silkText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                      juce::Justification just = juce::Justification::centred, juce::Colour colour = colours::silk, bool bold = true)
{
    g.setColour (colour);
    g.setFont (silkFont (height, bold));
    g.drawText (text, area, just, false);
}

/** Chrome slotted screw head. */
inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float slotAngle = 0.6f)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r - 1.0f, c.y - r + 0.5f, 2.0f * r + 2.0f, 2.0f * r + 2.0f);
    g.setGradientFill (juce::ColourGradient (colours::chrome, c.x - r, c.y - r, colours::chromeDark, c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    const auto d = juce::Point<float> (std::cos (slotAngle), std::sin (slotAngle)) * (r * 0.8f);
    g.setColour (juce::Colour (0xff2a2c2f));
    g.drawLine ({ c - d, c + d }, juce::jmax (1.2f, r * 0.28f));
}

namespace layout
{
    inline constexpr int kWidth = 1384, kHeight = 330;
    inline constexpr float kDefaultScale = 0.85f;
    inline constexpr int kX = 154; // everything right of the doubler plate

    inline const juce::Rectangle<int> earLeft { 0, 0, 40, kHeight };
    inline const juce::Rectangle<int> earRight { 1190 + kX, 0, 40, kHeight };
    inline const juce::Rectangle<int> inBay { 42, 12, 66, 306 };
    inline const juce::Rectangle<int> outBay { 1122 + kX, 12, 66, 306 };
    inline const juce::Rectangle<int> doublerPlate { 112, 12, 150, 306 };
    inline const juce::Rectangle<int> leftPlate { 112 + kX, 12, 376, 306 };
    inline const juce::Rectangle<int> rightPlate { 492 + kX, 12, 626, 306 };

    inline juce::Rectangle<int> meterArea (juce::Rectangle<int> bay) { return bay.withTrimmedTop (30).reduced (4, 6); }

    inline const juce::Rectangle<int> diagram { 140 + kX, 66, 320, 210 };
    inline const juce::Rectangle<int> vu { 624 + kX, 40, 210, 130 };
    inline const juce::Rectangle<int> routing { 659 + kX, 196, 140, 104 };

    /** Knob centres and diameters; labels sit above, values below. */
    struct KnobSpot
    {
        int cx, cy, size;
        const char* label;
    };
    inline constexpr KnobSpot levelA { 552 + kX, 102, 66, "LEVEL A" }, levelB { 552 + kX, 230, 66, "LEVEL B" };
    inline constexpr KnobSpot drive { 878 + kX, 102, 50, "DRIVE" }, mix { 946 + kX, 102, 50, "MIX" };
    inline constexpr KnobSpot output { 878 + kX, 230, 50, "OUTPUT" }, temperature { 946 + kX, 230, 50, "ROOM" };
    inline constexpr KnobSpot dryPan { 152, 182, 44, "PAN" }, dryLevel { 152, 265, 44, "LEVEL" };
    inline constexpr KnobSpot wetPan { 222, 182, 44, "PAN" }, wetLevel { 222, 265, 44, "LEVEL" };

    inline juce::Rectangle<int> knobBounds (KnobSpot k)
    {
        const int w = juce::jmin (80, k.size + 26);
        return { k.cx - w / 2, k.cy - k.size / 2, w, k.size + 18 };
    }
    inline juce::Rectangle<float> knobLabel (KnobSpot k) { return { (float) k.cx - 45.0f, (float) (k.cy - k.size / 2 - 20), 90.0f, 14.0f }; }

    /** Toggle grid on the right plate: two columns, three rows. */
    inline juce::Rectangle<int> toggle (int col, int row) { return { 990 + kX + col * 60, 44 + row * 90, 58, 66 }; }
    inline juce::Rectangle<float> titleAbove (juce::Rectangle<int> r) { return r.translated (0, -18).withHeight (14).toFloat().expanded (6.0f, 0.0f); }
    inline juce::Rectangle<float> toggleTitle (int col, int row) { return titleAbove (toggle (col, row)); }

    inline const juce::Rectangle<int> modeSwitch { 150, 44, 84, 66 };
    inline const juce::Rectangle<float> dryHeader { 117.0f, 122.0f, 70.0f, 13.0f }, wetHeader { 187.0f, 122.0f, 70.0f, 13.0f };
} // namespace layout
} // namespace dfd::ui
