/*
    CurveBox -- PluginEditor.cpp
*/

#include "PluginEditor.h"

//==============================================================================
CurveBoxAudioProcessorEditor::CurveBoxAudioProcessorEditor (CurveBoxAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      solarSystem (p)
{
    setLookAndFeel (&spaceLookAndFeel);

    //--------------------------------------------------------------------------
    // Header
    //--------------------------------------------------------------------------
    logoLabel.setText ("CURVEBOX", juce::dontSendNotification);
    logoLabel.setFont (cb::spaceui::font (19.0f, true));
    logoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcbdcff));
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    logoLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (logoLabel);

    prevPresetButton.setTooltip ("Previous preset");
    nextPresetButton.setTooltip ("Next preset");
    prevPresetButton.onClick = [this]
    {
        const int count = juce::jmax (1, processor.getNumPresets());
        processor.loadPreset ((processor.getCurrentPresetIndex() + count - 1) % count);
    };
    nextPresetButton.onClick = [this]
    {
        const int count = juce::jmax (1, processor.getNumPresets());
        processor.loadPreset ((processor.getCurrentPresetIndex() + 1) % count);
    };
    addAndMakeVisible (prevPresetButton);
    addAndMakeVisible (nextPresetButton);

    presetBox.addItemList (processor.getPresetNames(), 1);
    presetBox.setSelectedId (processor.getCurrentPresetIndex() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;

        if (index >= 0 && index != processor.getCurrentPresetIndex())
            processor.loadPreset (index);
    };
    addAndMakeVisible (presetBox);

    shapeButton.setTooltip ("Insert a factory shape into the current curve");
    shapeButton.onClick = [this]
    {
        if (curveEditor != nullptr)
            curveEditor->showShapeMenu();
    };
    addAndMakeVisible (shapeButton);

    snapButton.setTooltip ("Snap curve points to the grid (hold Shift to bypass)");
    snapButton.setToggleState (processor.isSnapEnabled(), juce::dontSendNotification);
    snapButton.onClick = [this] { processor.setSnapEnabled (snapButton.getToggleState()); };
    addAndMakeVisible (snapButton);

    enableButton.setTooltip ("Enable / bypass this shaper");
    addAndMakeVisible (enableButton);

    hoverLabel.setFont (cb::spaceui::font (11.5f));
    hoverLabel.setColour (juce::Label::textColourId, cb::spaceui::textDim);
    hoverLabel.setJustificationType (juce::Justification::centredLeft);
    hoverLabel.setInterceptsMouseClicks (false, false);
    hoverLabel.setText ("Hover a control to see its name and value", juce::dontSendNotification);
    addAndMakeVisible (hoverLabel);

    //--------------------------------------------------------------------------
    // Solar system
    //--------------------------------------------------------------------------
    solarSystem.onModuleSelected = [this] (int m) { setModule (m); };
    solarSystem.onModuleToggled = [this] (int m)
    {
        if (auto* param = processor.apvts.getParameter (cb::pid (m, cb::ids::enabled)))
            param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);

        solarSystem.repaint();
    };
    addAndMakeVisible (solarSystem);

    //--------------------------------------------------------------------------
    // Curve editor + planet view
    //--------------------------------------------------------------------------
    curveEditor = std::make_unique<cb::CurveEditor> (processor);
    curveEditor->onCurveChanged = [] (int) {};
    addAndMakeVisible (*curveEditor);

    planetView = std::make_unique<cb::PlanetView> (processor);
    addAndMakeVisible (*planetView);

    //--------------------------------------------------------------------------
    // Controls
    //--------------------------------------------------------------------------
    buildControls();

    setModule (cb::Volume);

    //--------------------------------------------------------------------------
    processor.onPresetChanged = [this] { refreshPresetUI(); repaint(); };
    processor.onCurveEdited = [this] (int) { if (curveEditor != nullptr) curveEditor->repaint(); };

    setResizable (true, true);
    setResizeLimits (900, 700, 2400, 1500);
    setSize (1120, 780);

    startTimer (40);
}

CurveBoxAudioProcessorEditor::~CurveBoxAudioProcessorEditor()
{
    stopTimer();

    processor.onPresetChanged = nullptr;
    processor.onCurveEdited = nullptr;

    enableAttachment.reset();

    setLookAndFeel (nullptr);
}

//==============================================================================
float CurveBoxAudioProcessorEditor::uiScale() const
{
    const float sx = (float) getWidth() / 1120.0f;
    const float sy = (float) getHeight() / 780.0f;
    return juce::jlimit (0.75f, 1.8f, juce::jmin (sx, sy));
}

//==============================================================================
void CurveBoxAudioProcessorEditor::addSliderControl (int module, const juce::String& paramId,
                                                     const juce::String& title)
{
    auto control = std::make_unique<Control>();
    control->module = module;
    control->paramId = paramId;
    control->title = title;

    control->label = std::make_unique<juce::Label> (juce::String(), title);
    control->label->setFont (cb::spaceui::font (10.5f));
    control->label->setJustificationType (juce::Justification::centred);
    control->label->setColour (juce::Label::textColourId, cb::spaceui::textDim);
    control->label->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*control->label);

    control->slider = std::make_unique<juce::Slider>();
    control->slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 14);
    control->slider->setScrollWheelEnabled (true);

    if (module >= 0)
        control->slider->setColour (juce::Slider::rotarySliderFillColourId, cb::moduleColour (module));
    else
        control->slider->setColour (juce::Slider::rotarySliderFillColourId, cb::spaceui::neonCyan);

    addAndMakeVisible (*control->slider);

    control->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, *control->slider);

    controls.push_back (std::move (control));
}

void CurveBoxAudioProcessorEditor::addChoiceControl (int module, const juce::String& paramId,
                                                     const juce::String& title)
{
    auto control = std::make_unique<Control>();
    control->module = module;
    control->paramId = paramId;
    control->title = title;

    control->label = std::make_unique<juce::Label> (juce::String(), title);
    control->label->setFont (cb::spaceui::font (10.5f));
    control->label->setJustificationType (juce::Justification::centred);
    control->label->setColour (juce::Label::textColourId, cb::spaceui::textDim);
    control->label->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*control->label);

    control->combo = std::make_unique<juce::ComboBox>();
    addAndMakeVisible (*control->combo);

    control->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, paramId, *control->combo);

    controls.push_back (std::move (control));
}

void CurveBoxAudioProcessorEditor::addToggleControl (int module, const juce::String& paramId,
                                                     const juce::String& title)
{
    auto control = std::make_unique<Control>();
    control->module = module;
    control->paramId = paramId;
    control->title = title;

    control->toggle = std::make_unique<juce::ToggleButton> (title);

    if (module >= 0)
        control->toggle->setColour (juce::ToggleButton::tickColourId, cb::moduleColour (module));

    addAndMakeVisible (*control->toggle);

    control->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, paramId, *control->toggle);

    controls.push_back (std::move (control));
}

void CurveBoxAudioProcessorEditor::buildControls()
{
    using namespace cb;

    // global
    addSliderControl (-1, ids::mix, "Mix");
    addSliderControl (-1, ids::output, "Output");
    addSliderControl (-1, ids::smooth, "Smooth");
    addSliderControl (-1, ids::offset, "Offset");

    // per module
    for (int m = 0; m < kNumModules; ++m)
    {
        addSliderControl (m, pid (m, ids::amount), moduleAmountName (m));
        addChoiceControl (m, pid (m, ids::rate), "Rate");
    }

    addChoiceControl (Filter, pid (Filter, ids::filterMode), "Mode");
    addSliderControl (Filter, pid (Filter, ids::filterFreq), "Freq");
    addSliderControl (Filter, pid (Filter, ids::filterRes), "Resonance");

    addSliderControl (Drive, pid (Drive, ids::driveTone), "Tone");
    addToggleControl (Drive, pid (Drive, ids::driveOS), "2x OS");

    addSliderControl (Crush, pid (Crush, ids::crushBits), "Bits");
    addSliderControl (Crush, pid (Crush, ids::crushSR), "Downsample");
    addToggleControl (Crush, pid (Crush, ids::crushOS), "2x OS");

    addChoiceControl (Delay, pid (Delay, ids::delayTime), "Time");
    addSliderControl (Delay, pid (Delay, ids::delayFB), "Feedback");
    addSliderControl (Delay, pid (Delay, ids::delayTone), "Tone");
    addToggleControl (Delay, pid (Delay, ids::delayPP), "Ping-Pong");

    addSliderControl (Grain, pid (Grain, ids::grainSize), "Size");
    addSliderControl (Grain, pid (Grain, ids::grainOvl), "Overlap");
    addSliderControl (Grain, pid (Grain, ids::grainPitch), "Pitch");
    addSliderControl (Grain, pid (Grain, ids::grainRand), "Rand Pitch");
    addSliderControl (Grain, pid (Grain, ids::grainSpray), "Spray");
    addSliderControl (Grain, pid (Grain, ids::grainTime), "Time");
    addSliderControl (Grain, pid (Grain, ids::grainFB), "Feedback");

    addSliderControl (Reverb, pid (Reverb, ids::reverbSize), "Size");
    addSliderControl (Reverb, pid (Reverb, ids::reverbDamp), "Damping");
    addSliderControl (Reverb, pid (Reverb, ids::reverbWidth), "Width");
    addSliderControl (Reverb, pid (Reverb, ids::reverbPreDelay), "Pre-Delay");
}

//==============================================================================
void CurveBoxAudioProcessorEditor::setModule (int module)
{
    module = juce::jlimit (0, cb::kNumModules - 1, module);
    currentModule = module;

    solarSystem.setSelectedModule (module);

    if (curveEditor != nullptr) curveEditor->setModule (module);
    if (planetView != nullptr)  planetView->setModule (module);

    enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, cb::pid (module, cb::ids::enabled), enableButton);

    for (auto& control : controls)
    {
        if (control->module < 0)
            continue;

        const bool visible = (control->module == module);

        if (control->label != nullptr)
            control->label->setVisible (visible);

        if (auto* widget = control->widget())
            widget->setVisible (visible);
    }

    resized();
    repaint();
}

//==============================================================================
void CurveBoxAudioProcessorEditor::layoutControls()
{
    const float scale = uiScale();

    auto placeGrid = [scale] (juce::Rectangle<int> area,
                              const std::vector<Control*>& items, int columns)
    {
        if (items.empty() || area.getWidth() <= 0 || area.getHeight() <= 0)
            return;

        const int rows = (int) std::ceil ((float) items.size() / (float) columns);
        const int cellWidth = area.getWidth() / columns;
        const int cellHeight = area.getHeight() / juce::jmax (1, rows);

        for (int i = 0; i < (int) items.size(); ++i)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (i % columns) * cellWidth,
                                              area.getY() + (i / columns) * cellHeight,
                                              cellWidth, cellHeight).reduced (2);

            auto* control = items[(size_t) i];
            const int labelHeight = juce::jmax (11, (int) std::round (13.0f * scale));

            if (control->label != nullptr)
                control->label->setBounds (cell.removeFromTop (labelHeight));

            if (auto* widget = control->widget())
            {
                if (control->slider != nullptr)
                    widget->setBounds (cell.reduced (1));
                else
                    widget->setBounds (cell.withSizeKeepingCentre (cell.getWidth(),
                                                                   juce::jmin (cell.getHeight(), (int) std::round (26.0f * scale))));
            }
        }
    };

    std::vector<Control*> globals, moduleControls;

    for (auto& control : controls)
    {
        auto* widget = control->widget();

        if (widget == nullptr || ! widget->isVisible())
            continue;

        if (control->module < 0)
            globals.push_back (control.get());
        else
            moduleControls.push_back (control.get());
    }

    placeGrid (globalControlArea, globals, 2);

    const int columns = moduleControls.size() > 6 ? 3 : (moduleControls.size() > 4 ? 3 : 2);
    placeGrid (moduleControlArea, moduleControls, columns);
}

//==============================================================================
void CurveBoxAudioProcessorEditor::rebuildStarfield()
{
    const int width = juce::jmax (1, getWidth());
    const int height = juce::jmax (1, getHeight());

    starfield = juce::Image (juce::Image::ARGB, width, height, true);
    juce::Graphics g (starfield);

    juce::ColourGradient background (cb::spaceui::bgDeep, 0.0f, 0.0f,
                                     cb::spaceui::bgPurple, (float) width, (float) height, false);
    background.addColour (0.45, cb::spaceui::bgMid.darker (0.2f));
    g.setGradientFill (background);
    g.fillAll();

    // faint nebula clouds
    for (int i = 0; i < 8; ++i)
    {
        const float x = cb::spaceui::hashRand (1237, i) * (float) width;
        const float y = cb::spaceui::hashRand (2347, i) * (float) height;
        const float radius = (0.15f + 0.35f * cb::spaceui::hashRand (3457, i)) * (float) width;
        const auto colour = (i % 2 == 0 ? juce::Colour (0xff2a1a55) : juce::Colour (0xff14264f));

        juce::ColourGradient nebula (colour.withAlpha (0.16f), x, y,
                                     colour.withAlpha (0.0f), x + radius, y + radius * 0.6f, true);
        g.setGradientFill (nebula);
        g.fillEllipse (x - radius, y - radius * 0.7f, radius * 2.0f, radius * 1.4f);
    }

    // three sizes of stars, fixed seed via deterministic hash
    for (int i = 0; i < 420; ++i)
    {
        const float x = cb::spaceui::hashRand (5501, i) * (float) width;
        const float y = cb::spaceui::hashRand (6607, i) * (float) height;
        const float rnd = cb::spaceui::hashRand (7703, i);

        float size = 0.8f;
        float alpha = 0.45f;

        if (rnd > 0.94f)      { size = 2.4f; alpha = 0.95f; }
        else if (rnd > 0.78f) { size = 1.5f; alpha = 0.70f; }

        g.setColour (juce::Colours::white.withAlpha (alpha * (0.5f + 0.5f * cb::spaceui::hashRand (8803, i))));
        g.fillEllipse (x, y, size, size);
    }
}

//==============================================================================
void CurveBoxAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const float scale = uiScale();

    //--------------------------------------------------------------------------
    const int headerHeight = juce::jmax (36, (int) std::round (46.0f * scale));
    auto header = bounds.removeFromTop (headerHeight).reduced (10, 6);

    logoLabel.setBounds (header.removeFromLeft ((int) std::round (120.0f * scale)));
    header.removeFromLeft (4);

    const int buttonWidth = juce::jmax (20, (int) std::round (26.0f * scale));
    prevPresetButton.setBounds (header.removeFromLeft (buttonWidth));
    header.removeFromLeft (3);
    presetBox.setBounds (header.removeFromLeft (juce::jmin (header.getWidth() / 2,
                                                           (int) std::round (250.0f * scale))));
    header.removeFromLeft (3);
    nextPresetButton.setBounds (header.removeFromLeft (buttonWidth));

    const int rightWidth = juce::jmax (60, (int) std::round (86.0f * scale));
    shapeButton.setBounds (header.removeFromRight (rightWidth));
    header.removeFromRight (6);
    snapButton.setBounds (header.removeFromRight (juce::jmax (54, (int) std::round (64.0f * scale))));

    //--------------------------------------------------------------------------
    hoverLabel.setBounds (bounds.removeFromBottom (juce::jmax (18, (int) std::round (24.0f * scale)))
                              .reduced (14, 0));

    bounds.reduce (8, 4);

    const int leftWidth  = (int) std::round (256.0f * scale);
    const int rightPanel = (int) std::round (330.0f * scale);

    auto leftArea  = bounds.removeFromLeft (leftWidth);
    auto rightArea = bounds.removeFromRight (rightPanel);
    bounds.removeFromLeft (6);
    bounds.removeFromRight (6);

    // left: solar system + global knobs
    auto solarArea = leftArea.removeFromTop (juce::jmin (leftArea.getHeight(),
                                                        (int) std::round (300.0f * scale)));
    solarSystem.setBounds (solarArea);
    leftArea.removeFromTop (6);
    globalControlArea = leftArea;

    // right: planet + module knobs
    auto planetArea = rightArea.removeFromTop (juce::jmin (rightArea.getHeight(),
                                                          (int) std::round (292.0f * scale)));
    planetView->setBounds (planetArea);
    rightArea.removeFromTop (6);

    // enabled toggle above the knobs
    enableButton.setBounds (rightArea.removeFromTop (juce::jmax (18, (int) std::round (22.0f * scale))));
    rightArea.removeFromTop (2);
    moduleControlArea = rightArea;

    curveEditor->setBounds (bounds);

    layoutControls();

    rebuildStarfield();
}

//==============================================================================
void CurveBoxAudioProcessorEditor::timerCallback()
{
    // slow background animation (parallax / twinkle overlay)
    backgroundPhase += 0.04;
    updateHoverText();
    repaint();
}

//==============================================================================
juce::String CurveBoxAudioProcessorEditor::describeParam (const juce::String& paramId) const
{
    if (auto* param = processor.apvts.getParameter (paramId))
        return param->getName (48) + ":  " + param->getText (param->getValue(), 24);

    return {};
}

void CurveBoxAudioProcessorEditor::updateHoverText()
{
    const auto mouse = juce::Desktop::getMousePosition();

    for (auto& control : controls)
    {
        auto* widget = control->widget();

        if (widget != nullptr && widget->isVisible() && widget->getScreenBounds().contains (mouse))
        {
            hoverLabel.setText (describeParam (control->paramId), juce::dontSendNotification);
            return;
        }
    }

    if (curveEditor != nullptr && curveEditor->getScreenBounds().contains (mouse))
    {
        hoverLabel.setText ("Curve: click to add a point, drag to move, double-click to delete, "
                            "alt/right-drag a segment to change its curvature",
                            juce::dontSendNotification);
        return;
    }

    if (solarSystem.getScreenBounds().contains (mouse))
    {
        hoverLabel.setText ("Solar system: click a body to select the shaper, "
                            "alt/right-click to enable or bypass it",
                            juce::dontSendNotification);
        return;
    }

    if (planetView != nullptr && planetView->getScreenBounds().contains (mouse))
    {
        hoverLabel.setText (cb::moduleName (currentModule) + " celestial body reacts to every knob of the module",
                            juce::dontSendNotification);
        return;
    }

    hoverLabel.setText ("Hover a control to see its name and value", juce::dontSendNotification);
}

//==============================================================================
void CurveBoxAudioProcessorEditor::refreshPresetUI()
{
    presetBox.setSelectedId (processor.getCurrentPresetIndex() + 1, juce::dontSendNotification);
}

//==============================================================================
void CurveBoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (starfield.isNull() || starfield.getWidth() != getWidth() || starfield.getHeight() != getHeight())
        rebuildStarfield();

    g.drawImageAt (starfield, 0, 0);

    // twinkling layer
    for (int i = 0; i < 26; ++i)
    {
        const float x = cb::spaceui::hashRand (5501, i * 7) * (float) getWidth();
        const float y = cb::spaceui::hashRand (6607, i * 7) * (float) getHeight();
        const float twinkle = 0.35f + 0.65f * (0.5f + 0.5f * std::sin ((float) backgroundPhase * 0.7f + (float) i));
        const float size = 1.0f + 1.6f * cb::spaceui::hashRand (9901, i);

        cb::spaceui::drawGlow (g, { x, y }, size * 3.2f, juce::Colour (0xff9fd0ff), 0.10f * twinkle);
        g.setColour (juce::Colours::white.withAlpha (0.65f * twinkle));
        g.fillEllipse (x, y, size, size);
    }

    // galaxy brightness driven by the Output parameter
    const float outputDb = processor.getParamValue (cb::ids::output);
    const float norm = juce::jlimit (0.0f, 1.0f, (outputDb + 24.0f) / 36.0f);
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float haloRadius = (float) juce::jmax (getWidth(), getHeight()) * (0.30f + 0.35f * norm);

    juce::ColourGradient halo (juce::Colour (0xff8fb8ff).withAlpha (0.05f + 0.13f * norm),
                               centre.x, centre.y,
                               juce::Colour (0xff8fb8ff).withAlpha (0.0f),
                               centre.x + haloRadius, centre.y, true);
    g.setGradientFill (halo);
    g.fillAll();

    // header / footer separators
    const int headerHeight = juce::jmax (36, (int) std::round (46.0f * uiScale()));
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine (headerHeight, 0.0f, (float) getWidth());
    g.drawHorizontalLine (getHeight() - 26, 0.0f, (float) getWidth());

    // soft glow under the logo
    cb::spaceui::drawGlow (g, { 40.0f, (float) headerHeight * 0.5f }, 60.0f, juce::Colour (0xff5fd8ff), 0.10f);
}
