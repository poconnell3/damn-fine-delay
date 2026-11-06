#include "PluginEditor.h"

namespace
{
const juce::Colour kPanel { 0xff2b2f33 };
const juce::Colour kPanelEdge { 0xff3c4247 };
const juce::Colour kInk { 0xffe8e4da };
const juce::Colour kInkDim { 0xff9aa0a6 };
const juce::Colour kAccent { 0xfff0a42a };
const juce::Colour kKnob { 0xff15181a };
} // namespace

DamnFineDelayEditor::PanelLook::PanelLook()
{
    setColour (juce::Slider::textBoxTextColourId, kInk);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, kKnob);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, kPanelEdge);
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, kKnob);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, kKnob);
    setColour (juce::Label::textColourId, kInkDim);
}

void DamnFineDelayEditor::PanelLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                                       float start, float end, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour (kPanelEdge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, angle, true);
    g.setColour (kAccent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - 7.0f;
    g.setColour (kKnob);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, 2.0f * knobR, 2.0f * knobR);
    const juce::Point<float> tip = centre.getPointOnCircumference (knobR - 4.0f, angle);
    g.setColour (kInk);
    g.drawLine ({ centre.getPointOnCircumference (knobR * 0.35f, angle), tip }, 2.0f);
}

DamnFineDelayEditor::DamnFineDelayEditor (DamnFineDelayProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&look);
    setUpKnob (knobs[0], ParamIds::drive, "DRIVE");
    setUpKnob (knobs[1], ParamIds::mix, "MIX");
    setUpKnob (knobs[2], ParamIds::output, "OUTPUT");
    setUpKnob (knobs[3], ParamIds::levelA, "LEVEL A  16 ms");
    setUpKnob (knobs[4], ParamIds::levelB, "LEVEL B  14 ms");
    setUpKnob (knobs[5], ParamIds::temperature, "ROOM TEMP");
    setUpChoice (choices[0], ParamIds::routing, "ROUTING");
    setUpChoice (choices[1], ParamIds::color, "COLOR");
    setUpChoice (choices[2], ParamIds::noise, "NOISE");
    setUpChoice (choices[3], ParamIds::quality, "QUALITY");
    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, kInkDim);
    addAndMakeVisible (status);
    timerCallback();
    startTimerHz (10);
    setResizable (false, false);
    setSize (720, 360);
}

void DamnFineDelayEditor::timerCallback()
{
    const auto& d = processor.diagnostics();
    const auto blocks = d.blocks.load(), bypassed = d.bypassedBlocks.load();
    juce::String text;
    text << juce::String (d.sampleRate.load() / 1000.0, 1) << " kHz   " << d.numIn.load() << " in / " << d.numOut.load() << " out   "
         << "blocks " << juce::String ((juce::int64) blocks) << " (bypassed " << juce::String ((juce::int64) bypassed) << ")   "
         << "in " << juce::String (d.inDb.load(), 1) << " dB   out " << juce::String (d.outDb.load(), 1) << " dB   "
         << "latency " << processor.getLatencySamples() << (processor.isSuspended() ? "   SUSPENDED" : "");
    status.setText (text, juce::dontSendNotification);
}

DamnFineDelayEditor::~DamnFineDelayEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DamnFineDelayEditor::setUpKnob (Knob& k, const char* paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state(), paramId, k.slider);
}

void DamnFineDelayEditor::setUpChoice (Choice& c, const char* paramId, const juce::String& name)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.state().getParameter (paramId)))
        c.box.addItemList (param->choices, 1);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (c.box);
    addAndMakeVisible (c.label);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.state(), paramId, c.box);
}

void DamnFineDelayEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPanel);
    auto area = getLocalBounds().reduced (14);
    g.setColour (kPanelEdge);
    g.drawRoundedRectangle (area.toFloat(), 6.0f, 1.0f);

    auto header = area.removeFromTop (56).reduced (16, 10);
    g.setColour (kInk);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("DAMN FINE DELAY", header.removeFromLeft (360), juce::Justification::centredLeft);
    g.setColour (kAccent);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("MODEL 1416  ACOUSTIC TUBE DELAY", header, juce::Justification::centredRight);
}

void DamnFineDelayEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    status.setBounds (area.removeFromBottom (22).reduced (16, 0));
    area.removeFromTop (56);
    area = area.reduced (16, 6);

    auto selectors = area.removeFromTop (56);
    const int choiceW = selectors.getWidth() / (int) choices.size();
    for (auto& c : choices)
    {
        auto cell = selectors.removeFromLeft (choiceW).reduced (6, 0);
        c.label.setBounds (cell.removeFromTop (20));
        c.box.setBounds (cell.removeFromTop (28));
    }

    area.removeFromTop (14);
    const int knobW = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto cell = area.removeFromLeft (knobW).reduced (4, 0);
        k.label.setBounds (cell.removeFromTop (20));
        k.slider.setBounds (cell.removeFromTop (130));
    }
}
