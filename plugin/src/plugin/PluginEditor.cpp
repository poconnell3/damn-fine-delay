#include "PluginEditor.h"

namespace
{
namespace layout = dfd::ui::layout;

const juce::Identifier kUiScale { "uiScale" }, kMeterChannel { "meterChannel" };
constexpr float kMinScale = 0.6f, kMaxScale = 1.6f;

juce::StringArray inputLabels (int numIn, int routing)
{
    if (numIn < 2)
        return { "IN" };
    return routing == 1 ? juce::StringArray { "A", "B" } : juce::StringArray { "L", "R" };
}

juce::StringArray outputLabels (int numOut, int routing)
{
    if (numOut < 2)
        return { "OUT" };
    return routing == 2 ? juce::StringArray { "L", "R" } : juce::StringArray { "A", "B" };
}
} // namespace

DamnFineDelayEditor::DamnFineDelayEditor (DamnFineDelayProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      routing (*p.state().getParameter (ParamIds::routing), { "WIDE", "DUAL", "CASCADE" })
{
    auto& apvts = p.state();
    addAndMakeVisible (face);

    diagram.setBounds (layout::diagram);
    inMeters.setBounds (layout::meterArea (layout::inBay));
    outMeters.setBounds (layout::meterArea (layout::outBay));
    vu.setBounds (layout::vu);
    routing.setBounds (layout::routing);
    for (auto* c : std::initializer_list<juce::Component*> { &diagram, &inMeters, &outMeters, &vu, &routing })
        face.addAndMakeVisible (c);

    addKnob (knobs[0], ParamIds::levelA, layout::levelA);
    addKnob (knobs[1], ParamIds::levelB, layout::levelB);
    addKnob (knobs[2], ParamIds::drive, layout::drive);
    addKnob (knobs[3], ParamIds::mix, layout::mix);
    addKnob (knobs[4], ParamIds::output, layout::output);
    addKnob (knobs[5], ParamIds::temperature, layout::temperature);
    addKnob (knobs[6], ParamIds::dryPan, layout::dryPan);
    addKnob (knobs[7], ParamIds::dryLevel, layout::dryLevel);
    addKnob (knobs[8], ParamIds::wetPan, layout::wetPan);
    addKnob (knobs[9], ParamIds::wetLevel, layout::wetLevel);
    addKnob (knobs[10], ParamIds::humanAmount, layout::humanAmount);

    color.attach (*apvts.getParameter (ParamIds::color), { 0, 1 });
    wetOnly.attach (*apvts.getParameter (ParamIds::wetOnly), { 1, 0 });
    noise.attach (*apvts.getParameter (ParamIds::noise), { 2, 1, 0 });
    quality.attach (*apvts.getParameter (ParamIds::quality), { 2, 1, 0 });
    modeSwitch.attach (*apvts.getParameter (ParamIds::doubler), { 0, 1 });
    personaSwitch.attach (*apvts.getParameter (ParamIds::persona), { 0, 1 });
    modeSwitch.setBounds (layout::modeSwitch);
    personaSwitch.setBounds (layout::personaSwitch);
    face.addAndMakeVisible (modeSwitch);
    face.addAndMakeVisible (personaSwitch);
    meterSelect.setPosition ((int) apvts.state.getProperty (kMeterChannel, 0), juce::dontSendNotification);
    meterSelect.onChange = [&apvts] (int pos) { apvts.state.setProperty (kMeterChannel, pos, nullptr); };

    const std::pair<dfd::ui::ToggleSwitch*, juce::Point<int>> toggles[] = {
        { &color, { 0, 0 } }, { &wetOnly, { 1, 0 } }, { &noise, { 0, 1 } }, { &quality, { 1, 1 } }, { &meterSelect, { 0, 2 } }
    };
    for (auto [t, cell] : toggles)
    {
        t->setBounds (layout::toggle (cell.x, cell.y));
        face.addAndMakeVisible (t);
    }
    lamp.setBounds (layout::toggle (1, 2).withSize (36, 36).translated (-3, 14));
    face.addAndMakeVisible (lamp);
    face.setLookAndFeel (&look); // after the children exist, so their text boxes pick it up

    const float scale = juce::jlimit (kMinScale, kMaxScale, (float) (double) apvts.state.getProperty (kUiScale, layout::kDefaultScale));
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (layout::kWidth * kMinScale), juce::roundToInt (layout::kHeight * kMinScale),
                     juce::roundToInt (layout::kWidth * kMaxScale), juce::roundToInt (layout::kHeight * kMaxScale));
    getConstrainer()->setFixedAspectRatio ((double) layout::kWidth / layout::kHeight);
    setSize (juce::roundToInt (layout::kWidth * scale), juce::roundToInt (layout::kHeight * scale));
    sizeRestored = true;

    lastBlocks = processor.metering().processedBlocks.load();
    lastTickMs = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimerHz (60);
}

DamnFineDelayEditor::~DamnFineDelayEditor()
{
    stopTimer();
    face.setLookAndFeel (nullptr);
}

void DamnFineDelayEditor::addKnob (Knob& k, const char* paramId, layout::KnobSpot spot)
{
    const auto bounds = layout::knobBounds (spot);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, bounds.getWidth(), 16);
    k.slider.setBounds (bounds);
    k.slider.setTitle (spot.label);
    face.addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state(), paramId, k.slider);
    if (auto* param = processor.state().getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void DamnFineDelayEditor::showMode (bool doubler, bool human)
{
    // Double mode mixes with its own pan and level knobs; the normal mix controls and routing do nothing there.
    // The persona section only applies in Double mode, and its amount only to Human.
    auto setActive = [] (juce::Component& c, bool active)
    {
        c.setEnabled (active);
        c.setAlpha (active ? 1.0f : 0.35f);
    };
    for (size_t i : { 0u, 1u, 3u })
        setActive (knobs[i].slider, ! doubler);
    for (size_t i = 6; i <= 9; ++i)
        setActive (knobs[i].slider, doubler);
    setActive (personaSwitch, doubler);
    setActive (knobs[10].slider, doubler && human);
    setActive (wetOnly, ! doubler);
    setActive (routing, ! doubler);
}

void DamnFineDelayEditor::resized()
{
    const float scale = (float) getWidth() / (float) layout::kWidth;
    face.setBounds (0, 0, layout::kWidth, layout::kHeight);
    face.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored && scale >= kMinScale * 0.99f)
        processor.state().state.setProperty (kUiScale, scale, nullptr);
}

void DamnFineDelayEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.1, (now - lastTickMs) / 1000.0);
    lastTickMs = now;

    auto& m = processor.metering();
    const int numIn = m.numIn.load(), numOut = m.numOut.load();
    const auto in0 = m.in[0].take(), in1 = m.in[1].take(), out0 = m.out[0].take(), out1 = m.out[1].take();
    inMeters.update ({ in0.peak, in1.peak }, dt);
    outMeters.update ({ out0.peak, out1.peak }, dt);

    auto& apvts = processor.state();
    const bool doubler = apvts.getRawParameterValue (ParamIds::doubler)->load() >= 0.5f;
    const bool human = apvts.getRawParameterValue (ParamIds::persona)->load() >= 0.5f;
    if (const int mode = (int) doubler * 2 + (int) human; mode != shownMode)
    {
        shownMode = mode;
        showMode (doubler, human);
    }
    const int route = doubler ? 2 : juce::roundToInt (apvts.getRawParameterValue (ParamIds::routing)->load());
    const auto outs = outputLabels (numOut, route);
    inMeters.setChannelLabels (inputLabels (numIn, route));
    outMeters.setChannelLabels (outs);

    const int vuChannel = numOut > 1 ? meterSelect.getPosition() : 0;
    vu.update (vuChannel == 0 ? out0.rms : out1.rms, dt);
    vu.setChannelName (outs[vuChannel]);

    diagram.setState ({ route, apvts.getRawParameterValue (ParamIds::temperature)->load(), numIn, numOut, doubler,
                        apvts.getRawParameterValue (ParamIds::dryPan)->load() / 100.0f, apvts.getRawParameterValue (ParamIds::wetPan)->load() / 100.0f,
                        doubler && human, apvts.getRawParameterValue (ParamIds::humanAmount)->load() });

    const auto blocks = m.processedBlocks.load();
    if (blocks != lastBlocks)
    {
        lastBlocks = blocks;
        lastActiveMs = now;
    }
    lamp.setLit (now - lastActiveMs < 250.0);
}
