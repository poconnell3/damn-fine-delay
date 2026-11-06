#include "Controls.h"

namespace dfd::ui
{
namespace
{
juce::Point<float> polar (juce::Point<float> c, float radius, float angleFromUp)
{
    return { c.x + radius * std::sin (angleFromUp), c.y - radius * std::cos (angleFromUp) };
}

void fillChromeDisc (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setGradientFill (juce::ColourGradient (colours::chrome, c.x - r * 0.7f, c.y - r * 0.7f, colours::chromeDark, c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
}
} // namespace

// ------------------------------------------------------------------------------------------- RackLook

RackLook::RackLook()
{
    setColour (juce::Slider::textBoxTextColourId, colours::amberDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::amber.withAlpha (0.4f));
    setColour (juce::TextEditor::textColourId, colours::amber);
    setColour (juce::TextEditor::backgroundColourId, colours::recess);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::amberDim);
    setColour (juce::CaretComponent::caretColourId, colours::amber);
    setColour (juce::PopupMenu::backgroundColourId, colours::recess);
    setColour (juce::PopupMenu::textColourId, colours::silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::recess);
}

juce::Label* RackLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (silkFont (11.0f, false));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void RackLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto c = area.getCentre();
    const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
    const float r = outer - 6.0f;
    const float angle = start + pos * (end - start);

    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (float) i / 10.0f * (end - start);
        const bool major = i == 0 || i == 5 || i == 10;
        g.setColour (major ? colours::silk : colours::silkDim);
        g.drawLine ({ polar (c, r + 2.0f, a), polar (c, major ? outer : outer - 2.0f, a) }, major ? 1.4f : 1.0f);
    }

    // Drop shadow, then the knurled skirt.
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - r + 1.5f, c.y - r + 2.5f, 2.0f * r, 2.0f * r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34373b), c.x, c.y - r, juce::Colour (0xff08090a), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    const int ridges = juce::roundToInt (r * 2.2f);
    for (int i = 0; i < ridges; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        g.setColour (i % 2 ? juce::Colours::white.withAlpha (0.10f) : juce::Colours::black.withAlpha (0.55f));
        g.drawLine ({ polar (c, r * 0.80f, a), polar (c, r - 0.5f, a) }, 1.0f);
    }

    // Cap.
    const float cap = r * 0.72f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4e53), c.x - cap * 0.6f, c.y - cap * 0.8f,
                                             juce::Colour (0xff111214), c.x + cap * 0.5f, c.y + cap, true));
    g.fillEllipse (c.x - cap, c.y - cap, 2.0f * cap, 2.0f * cap);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (c.x - cap, c.y - cap, 2.0f * cap, 2.0f * cap, 1.0f);

    g.setColour (colours::silk);
    g.drawLine ({ polar (c, r * 0.22f, angle), polar (c, r * 0.95f, angle) }, juce::jmax (2.0f, r * 0.08f));
}

// ------------------------------------------------------------------------------------------- ToggleSwitch

ToggleSwitch::ToggleSwitch (juce::StringArray legendsInOrder, Orientation o) : legends (std::move (legendsInOrder)), orientation (o)
{
    jassert (legends.size() == 2 || legends.size() == 3);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ToggleSwitch::attach (juce::RangedAudioParameter& param, std::vector<int> paramIndexForPosition)
{
    jassert ((int) paramIndexForPosition.size() == legends.size());
    paramIndex = std::move (paramIndexForPosition);
    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
    {
        const auto it = std::find (paramIndex.begin(), paramIndex.end(), juce::roundToInt (v));
        if (it != paramIndex.end())
            setPosition ((int) std::distance (paramIndex.begin(), it), juce::dontSendNotification);
    });
    onChange = [this] (int p) { attachment->setValueAsCompleteGesture ((float) paramIndex[(size_t) p]); };
    attachment->sendInitialUpdate();
}

void ToggleSwitch::setPosition (int newPosition, juce::NotificationType notification)
{
    newPosition = juce::jlimit (0, legends.size() - 1, newPosition);
    if (newPosition == position)
        return;
    position = newPosition;
    repaint();
    if (notification != juce::dontSendNotification && onChange)
        onChange (position);
}

juce::Point<float> ToggleSwitch::nutCentre() const
{
    return orientation == Orientation::vertical ? juce::Point<float> { 15.0f, getHeight() * 0.5f } : getLocalBounds().toFloat().getCentre();
}

float ToggleSwitch::throwLength() const { return orientation == Orientation::vertical ? getHeight() * 0.5f - 9.0f : 15.0f; }

void ToggleSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (legends.size() == 2)
        setPosition (1 - position, juce::sendNotification);
    else
    {
        const bool before = orientation == Orientation::vertical ? e.position.y < nutCentre().y : e.position.x < nutCentre().x;
        setPosition (position + (before ? -1 : 1), juce::sendNotification);
    }
}

void ToggleSwitch::paintLegends (juce::Graphics& g, juce::Point<float> c, float len) const
{
    const int n = legends.size();
    for (int i = 0; i < n; ++i)
    {
        const auto colour = i == position ? colours::silk : colours::silkDim;
        if (orientation == Orientation::vertical)
        {
            const float ly = n == 2 ? (i == 0 ? c.y - len : c.y + len) : c.y + (float) (i - 1) * len;
            silkText (g, legends[i], { 28.0f, ly - 7.0f, (float) getWidth() - 28.0f, 14.0f }, 10.0f, juce::Justification::centredLeft, colour);
        }
        else if (i == 0) // clear of the lever tip when it points this way
            silkText (g, legends[i], { 0.0f, c.y - 7.0f, c.x - len - 4.0f, 14.0f }, 9.0f, juce::Justification::centredRight, colour);
        else if (i == n - 1)
            silkText (g, legends[i], { c.x + len + 4.0f, c.y - 7.0f, (float) getWidth() - c.x - len - 4.0f, 14.0f }, 9.0f, juce::Justification::centredLeft, colour);
        else
            silkText (g, legends[i], { c.x - 30.0f, 0.0f, 60.0f, c.y - 10.0f }, 9.5f, juce::Justification::centredBottom, colour);
    }
}

void ToggleSwitch::paintLever (juce::Graphics& g, juce::Point<float> c, juce::Point<float> dir, float len) const
{
    const juce::Point<float> across { -dir.y, dir.x };
    const auto tip = c + dir * (len - 3.0f);
    juce::Path lever;
    lever.startNewSubPath (c - across * 2.8f);
    lever.lineTo (tip - across * 1.9f);
    lever.lineTo (tip + across * 1.9f);
    lever.lineTo (c + across * 2.8f);
    lever.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (lever, juce::AffineTransform::translation (2.0f, 2.0f));
    const auto a = c - across * 3.0f, b = c + across * 3.0f;
    juce::ColourGradient shine (colours::chromeDark, a.x, a.y, colours::chromeDark, b.x, b.y, false);
    shine.addColour (0.4, colours::chrome);
    g.setGradientFill (shine);
    g.fillPath (lever);
    fillChromeDisc (g, tip, 3.6f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.fillEllipse (tip.x - 1.8f, tip.y - 2.2f, 1.6f, 1.6f);
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    const auto c = nutCentre();
    const float len = throwLength();
    const int n = legends.size();
    paintLegends (g, c, len);

    // Hex nut and bushing.
    juce::Path hex;
    for (int k = 0; k < 6; ++k)
    {
        const auto p = polar (c, 10.0f, juce::MathConstants<float>::pi / 6.0f + (float) k * juce::MathConstants<float>::pi / 3.0f);
        k == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (hex, juce::AffineTransform::translation (1.0f, 1.5f));
    g.setGradientFill (juce::ColourGradient (colours::chrome, c.x - 8.0f, c.y - 8.0f, colours::chromeDark, c.x + 8.0f, c.y + 8.0f, false));
    g.fillPath (hex);
    fillChromeDisc (g, c, 5.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f, 0.8f);

    if (n == 3 && position == 1)
    {
        fillChromeDisc (g, c, 4.2f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (c.x - 2.0f, c.y - 2.5f, 2.0f, 2.0f);
        return;
    }
    const float sign = position == 0 ? -1.0f : 1.0f;
    paintLever (g, c, orientation == Orientation::vertical ? juce::Point<float> { 0.0f, sign } : juce::Point<float> { sign, 0.0f }, len);
}

// ------------------------------------------------------------------------------------------- RotarySelector

RotarySelector::RotarySelector (juce::RangedAudioParameter& param, juce::StringArray shortLegends)
    : legends (std::move (shortLegends)),
      attachment (param, [this] (float v)
                  {
                      index = juce::jlimit (0, legends.size() - 1, juce::roundToInt (v));
                      repaint();
                  })
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

juce::Point<float> RotarySelector::centre() const { return { getWidth() * 0.5f, (float) getHeight() - 38.0f }; }

float RotarySelector::angleFor (int i) const
{
    const float spread = juce::degreesToRadians (55.0f);
    return legends.size() < 2 ? 0.0f : -spread + 2.0f * spread * (float) i / (float) (legends.size() - 1);
}

void RotarySelector::select (int i)
{
    i = juce::jlimit (0, legends.size() - 1, i);
    if (i != index)
        attachment.setValueAsCompleteGesture ((float) i);
}

void RotarySelector::mouseDown (const juce::MouseEvent& e)
{
    dragStartIndex = index;
    const auto d = e.position - centre();
    if (d.getDistanceFromOrigin() <= 20.0f)
        return;
    const float a = std::atan2 (d.x, -d.y);
    int best = 0;
    for (int i = 1; i < legends.size(); ++i)
        if (std::abs (angleFor (i) - a) < std::abs (angleFor (best) - a))
            best = i;
    select (best);
}

void RotarySelector::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mouseDownPosition.getDistanceFrom (centre()) <= 20.0f)
        select (dragStartIndex + juce::roundToInt ((float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 30.0f));
}

void RotarySelector::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && e.mouseDownPosition.getDistanceFrom (centre()) <= 20.0f)
        select ((index + 1) % legends.size());
}

void RotarySelector::paint (juce::Graphics& g)
{
    const auto c = centre();
    for (int i = 0; i < legends.size(); ++i)
    {
        const float a = angleFor (i);
        g.setColour (i == index ? colours::silk : colours::silkDim);
        g.drawLine ({ polar (c, 22.0f, a), polar (c, 27.0f, a) }, 1.4f);
        const auto p = polar (c, 37.0f, a);
        silkText (g, legends[i], { p.x - 34.0f, p.y - 7.0f, 68.0f, 14.0f }, 10.0f, juce::Justification::centred,
                  i == index ? colours::silk : colours::silkDim);
    }

    const float r = 17.0f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - r + 1.5f, c.y - r + 2.5f, 2.0f * r, 2.0f * r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d41), c.x, c.y - r, juce::Colour (0xff0b0c0d), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);

    // Chicken-head pointer.
    juce::Path head;
    head.startNewSubPath (0.0f, -21.0f);
    head.cubicTo (6.0f, -14.0f, 9.0f, 8.0f, 7.0f, 13.0f);
    head.cubicTo (4.0f, 17.0f, -4.0f, 17.0f, -7.0f, 13.0f);
    head.cubicTo (-9.0f, 8.0f, -6.0f, -14.0f, 0.0f, -21.0f);
    head.closeSubPath();
    const auto t = juce::AffineTransform::rotation (angleFor (index)).translated (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (head, t.translated (1.5f, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4f54), c.x - 6.0f, c.y - 10.0f, juce::Colour (0xff0e0f10), c.x + 6.0f, c.y + 10.0f, false));
    g.fillPath (head, t);
    g.setColour (colours::silk);
    g.drawLine ({ polar (c, 4.0f, angleFor (index)), polar (c, 19.0f, angleFor (index)) }, 2.0f);
}

// ------------------------------------------------------------------------------------------- PilotLamp

void PilotLamp::setLit (bool shouldBeLit)
{
    if (lit != shouldBeLit)
    {
        lit = shouldBeLit;
        repaint();
    }
}

void PilotLamp::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    if (lit)
    {
        g.setGradientFill (juce::ColourGradient (colours::amber.withAlpha (0.45f), c.x, c.y, colours::amber.withAlpha (0.0f), c.x + 19.0f, c.y, true));
        g.fillEllipse (c.x - 19.0f, c.y - 19.0f, 38.0f, 38.0f);
    }
    fillChromeDisc (g, c, 10.0f);
    const float r = 7.0f;
    g.setGradientFill (lit ? juce::ColourGradient (juce::Colour (0xfffff2c4), c.x - 2.0f, c.y - 2.0f, juce::Colour (0xffe07a10), c.x + r, c.y + r, true)
                           : juce::ColourGradient (juce::Colour (0xff6a3b1c), c.x - 2.0f, c.y - 2.0f, juce::Colour (0xff1e0f06), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour (juce::Colours::white.withAlpha (lit ? 0.75f : 0.3f));
    g.fillEllipse (c.x - 3.5f, c.y - 4.0f, 2.6f, 2.0f);
}
} // namespace dfd::ui
