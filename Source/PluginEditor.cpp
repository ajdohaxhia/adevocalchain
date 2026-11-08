#include "PluginEditor.h"

#include <cmath>

namespace vr
{

namespace
{
const char* scaleName (int index)
{
    static const char* names[] = { "CHROMATIC", "MAJOR", "MINOR", "CUSTOM" };
    return names[juce::jlimit (0, 3, index)];
}

int countBits (int value)
{
    int count = 0;

    while (value != 0)
    {
        count += value & 1;
        value >>= 1;
    }

    return count;
}
} // namespace

void ControlGrid::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (VocalRackLookAndFeel::card());
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (VocalRackLookAndFeel::line());
    g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

    if (title.isNotEmpty())
    {
        auto titleArea = getLocalBounds().reduced (18);
        titleArea.setHeight (32);
        g.setColour (VocalRackLookAndFeel::text());
        g.setFont (VocalRackLookAndFeel::font (20.0f, true));
        g.drawText (title, titleArea, juce::Justification::centredLeft);

        g.setColour (VocalRackLookAndFeel::line().withAlpha (0.7f));
        g.fillRect (18, 40, getWidth() - 36, 1);
    }
}

void ControlGrid::paintOverChildren (juce::Graphics& g)
{
    if (! moduleStrip || headerControls.size() < 2)
        return;

    const int cellWidth = (getWidth() - 36) / (int) headerControls.size();
    g.setColour (VocalRackLookAndFeel::dimText().withAlpha (0.7f));
    g.setFont (VocalRackLookAndFeel::font (12.0f));

    for (size_t i = 1; i < headerControls.size(); ++i)
        g.drawText (">", 18 + (int) i * cellWidth - 8, 30, 16, 18,
                    juce::Justification::centred);
}

VocalRackEditor::VocalRackEditor (VocalRackAudioProcessor& processorRef)
    : AudioProcessorEditor (&processorRef),
      processor (processorRef),
      globalBypass (processorRef.getAPVTS(), "global.bypass", "GLOBAL"),
      meters (processorRef.getEngine()),
      pitchPanel (processorRef.getEngine()),
      keyboard (processorRef.getAPVTS()),
      browser (processorRef, [this]
      {
          stateDirty = true;

          if (browser.isVisible())
              browser.refresh();
      })
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
    setResizable (true, true);
    setResizeLimits (960, 640, 1600, 1100);
    setSize (1120, 760);

    configureTopBar();
    configureStatusBar();
    configureQuickView();
    configureDetailTabs();

    addAndMakeVisible (noticeLabel);
    noticeLabel.setText ("96 factory presets · Keep Song Key is on for factory browsing.",
                         juce::dontSendNotification);
    noticeLabel.setFont (VocalRackLookAndFeel::font (10.0f));
    noticeLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    noticeLabel.setJustificationType (juce::Justification::centredLeft);
    noticeLabel.setMinimumHorizontalScale (0.85f);

    qualityHintLabel.setFont (VocalRackLookAndFeel::font (9.0f));
    qualityHintLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    qualityHintLabel.setJustificationType (juce::Justification::centredLeft);
    qualityHintLabel.setText ("QUALITY", juce::dontSendNotification);
    qualityHintLabel.setTooltip ("Track and Mix use different internal resources and latency. Stop the transport before switching if the host cannot re-align live.");


    addAndMakeVisible (browser);
    browser.setVisible (false);
    browser.toFront (false);

    processor.onStateChanged = [this] { stateDirty = true; };
    processor.onNotice = [this] (const juce::String& message) { showNotice (message); };

    setView (false);
    refreshFromProcessor();
    startTimerHz (20);
}

VocalRackEditor::~VocalRackEditor()
{
    stopTimer();
    processor.onStateChanged = nullptr;
    processor.onNotice = nullptr;
    setLookAndFeel (nullptr);
}

void VocalRackEditor::configureTopBar()
{
    logoLabel.setText ("ademusic", juce::dontSendNotification);
    logoLabel.setFont (VocalRackLookAndFeel::font (27.0f, true));
    logoLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    productLabel.setText ("VOCAL RACK", juce::dontSendNotification);
    productLabel.setFont (VocalRackLookAndFeel::font (17.0f, true));
    productLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    productLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (productLabel);

    presetNameLabel.setFont (VocalRackLookAndFeel::font (17.0f, true));
    presetNameLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    presetNameLabel.setJustificationType (juce::Justification::centredLeft);
    presetNameLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (presetNameLabel);

    presetMetaLabel.setFont (VocalRackLookAndFeel::font (8.5f));
    presetMetaLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    presetMetaLabel.setJustificationType (juce::Justification::centredLeft);
    presetMetaLabel.setMinimumHorizontalScale (0.65f);
    addAndMakeVisible (presetMetaLabel);

    latencyLabel.setFont (VocalRackLookAndFeel::font (10.0f));
    latencyLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    latencyLabel.setJustificationType (juce::Justification::centredLeft);
    latencyLabel.setTooltip ("Plugin-reported latency. Track is pinned at or below 15 ms at 48 kHz; Mix adds the spectral path.");
    addAndMakeVisible (latencyLabel);

    sampleRateLabel.setFont (VocalRackLookAndFeel::font (10.0f));
    sampleRateLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    sampleRateLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sampleRateLabel);

    ceilingLabel.setFont (VocalRackLookAndFeel::font (10.0f));
    ceilingLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    ceilingLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (ceilingLabel);

    spectralLabel.setFont (VocalRackLookAndFeel::font (8.5f, true));
    spectralLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::danger());
    spectralLabel.setJustificationType (juce::Justification::centredLeft);
    spectralLabel.setVisible (false);
    addAndMakeVisible (spectralLabel);

    for (auto* button : { &previousButton, &nextButton, &browseButton, &abAButton,
                          &abBButton, &copyABButton, &matchABButton })
    {
        addAndMakeVisible (*button);
        button->setWantsKeyboardFocus (true);
    }

    previousButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    browseButton.setTooltip ("Open the preset browser");
    abAButton.setTooltip ("Recall A/B slot A");
    abBButton.setTooltip ("Recall A/B slot B");
    copyABButton.setTooltip ("Copy the active A/B slot to the other slot");
    matchABButton.setClickingTogglesState (true);
    matchABButton.setTooltip ("Loudness Match: match A/B comparison level without changing stored state or output trim.");
    browseButton.setButtonText ("Browse");

    previousButton.onClick = [this] { loadRelativePreset (-1); };
    nextButton.onClick = [this] { loadRelativePreset (1); };
    browseButton.setComponentID ("BrowseButton");
    browseButton.onClick = [this] { toggleBrowser(); };
    abAButton.onClick = [this] { processor.switchAB (0); stateDirty = true; };
    abBButton.onClick = [this] { processor.switchAB (1); stateDirty = true; };
    copyABButton.onClick = [this]
    {
        processor.copyAB (processor.getActiveAB(), 1 - processor.getActiveAB());
        stateDirty = true;
    };
    matchABButton.onClick = [this]
    {
        processor.setLoudnessMatch (matchABButton.getToggleState());
        stateDirty = true;
    };

    keepKeyButton.setWantsKeyboardFocus (true);
    keepKeyButton.setTooltip ("Factory browsing keeps the current tonic, scale and note mask.");
    keepKeyButton.onClick = [this]
    {
        processor.setKeepSongKey (keepKeyButton.getToggleState());
        stateDirty = true;
    };
    addAndMakeVisible (keepKeyButton);

    qualityBox.addItem ("TRACK", 1);
    qualityBox.addItem ("MIX", 2);
    qualityBox.setTitle ("Quality configuration");
    qualityBox.setTooltip ("Track: low-latency tuning and expander. Mix: full spectral cleanup and higher latency. Stop transport before switching if the host does not re-align live.");
    qualityBox.setWantsKeyboardFocus (true);
    qualityBox.onChange = [this]
    {
        const auto mode = qualityBox.getSelectedId() == 2 ? QualityMode::mix : QualityMode::track;
        processor.requestQuality (mode);
        stateDirty = true;
        showNotice (mode == QualityMode::track
                        ? "Track configuration active. Reported latency is pinned at or below 15 ms at 48 kHz."
                        : "Mix configuration active. Full spectral cleanup is available; latency is higher.");
    };
    addAndMakeVisible (qualityBox);

    globalBypass.setTooltip ("Global bypass. When off, returns the latency-aligned original input.");
    addAndMakeVisible (globalBypass);

    qualityHintLabel.setFont (VocalRackLookAndFeel::font (9.0f));
    qualityHintLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    qualityHintLabel.setJustificationType (juce::Justification::centredLeft);
    qualityHintLabel.setText ("QUALITY", juce::dontSendNotification);
    qualityHintLabel.setTooltip ("Stop transport before switching quality if the host cannot re-align live.");
    qualityHintLabel.setVisible (true);
    addAndMakeVisible (qualityHintLabel);

    quickButton.setButtonText ("QUICK");
    detailButton.setButtonText ("DETAIL");
    addAndMakeVisible (quickButton);
    addAndMakeVisible (detailButton);
    quickButton.setComponentID ("QuickButton");
    detailButton.setComponentID ("DetailButton");
    quickButton.setClickingTogglesState (false);
    detailButton.setClickingTogglesState (false);
    quickButton.onClick = [this] { setView (false); };
    detailButton.onClick = [this] { setView (true); };
}

void VocalRackEditor::configureStatusBar()
{
    addAndMakeVisible (meters);
    addAndMakeVisible (pitchPanel);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (keyLabel);

    keyLabel.setFont (VocalRackLookAndFeel::font (9.0f, true));
    keyLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    keyLabel.setJustificationType (juce::Justification::centredLeft);
    keyLabel.setText ("KEY", juce::dontSendNotification);

    maskLabel.setFont (VocalRackLookAndFeel::font (9.5f, true));
    maskLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    maskLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (maskLabel);

    tonicBox.setTitle ("Tonic");
    tonicBox.setTooltip ("Song tonic used for target-note selection.");
    tonicBox.setWantsKeyboardFocus (true);
    tonicBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    addAndMakeVisible (tonicBox);
    tonicAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getAPVTS(), "pitch.tonic", tonicBox);

    scaleBox.setTitle ("Scale");
    scaleBox.setTooltip ("Chromatic, major or minor scale, or a custom note mask.");
    scaleBox.setWantsKeyboardFocus (true);
    scaleBox.addItemList ({ "CHROMATIC", "MAJOR", "MINOR", "CUSTOM" }, 1);
    addAndMakeVisible (scaleBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getAPVTS(), "pitch.scale", scaleBox);

    keyboard.onToggle = [this] (int pitchClass) { toggleMaskNote (pitchClass); };
}

void VocalRackEditor::addQuickKnob (const char* parameterId, const char* displayName)
{
    auto* knob = new Knob (processor.getAPVTS(), parameterId,
                           displayName != nullptr ? juce::String (displayName) : juce::String());
    quickView.addGridControl (knob);
}

void VocalRackEditor::addQuickPower (const char* parameterId, const char* label)
{
    auto* power = new PowerButton (processor.getAPVTS(), parameterId, label);
    quickView.addHeaderControl (power);
}

void VocalRackEditor::addKnob (ControlGrid& grid, const char* parameterId, const char* subLabel)
{
    auto* knob = new Knob (processor.getAPVTS(), parameterId);

    if (subLabel != nullptr)
        knob->setSubLabel (subLabel);

    grid.addGridControl (knob);
}

void VocalRackEditor::addPower (ControlGrid& grid, const char* parameterId, const char* label)
{
    auto* power = new PowerButton (processor.getAPVTS(), parameterId, label);
    grid.addHeaderControl (power);
}

juce::ToggleButton* VocalRackEditor::addToggle (ControlGrid& grid, const char* parameterId, const char* label)
{
    auto* toggle = new juce::ToggleButton (label);
    toggle->setTitle (label);
    toggle->setWantsKeyboardFocus (true);

    if (const auto* spec = findParamSpec (parameterId))
        toggle->setTooltip (juce::String (label) + " — " + spec->description);
    else
        toggle->setTooltip (juce::String (label));
    grid.addHeaderControl (toggle);
    toggleAttachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.getAPVTS(), parameterId, *toggle));
    return toggle;
}

void VocalRackEditor::addCombo (ControlGrid& grid, const char* parameterId, const char* label)
{
    const auto* spec = findParamSpec (parameterId);

    if (spec == nullptr)
        return;

    auto* combo = new juce::ComboBox();
    combo->setTitle (label);
    combo->setTooltip (juce::String (label) + " — " + spec->description);
    combo->setWantsKeyboardFocus (true);
    combo->addItemList (juce::StringArray::fromTokens (juce::String (spec->choices), "|", ""), 1);
    grid.addHeaderControl (combo);
    detailComboAttachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.getAPVTS(), parameterId, *combo));
}

void VocalRackEditor::configureQuickView()
{
    quickView.setComponentID ("QuickView");
    quickView.setColumns (6);
    quickView.setModuleStrip (true);

    addQuickPower ("exp.bypass", "Cleanup");
    addQuickPower ("dn.bypass", "Noise");
    addQuickPower ("pitch.bypass", "Pitch");
    addQuickPower ("comp.bypass", "Comp");
    addQuickPower ("ds.bypass", "De-ess");
    addQuickPower ("sat.bypass", "Color");
    addQuickPower ("dly.bypass", "Delay");
    addQuickPower ("rev.bypass", "Space");
    addQuickPower ("lim.bypass", "Limit");

    addQuickKnob ("global.input", "Input");
    addQuickKnob ("dn.amount", "Noise");
    addQuickKnob ("pitch.amount", "Tune");
    addQuickKnob ("pitch.retune", "Retune");
    addQuickKnob ("comp.threshold", "Comp");
    addQuickKnob ("ds.range", "De-ess");
    addQuickKnob ("teq.presGain", "Presence");
    addQuickKnob ("sat.drive", "Drive");
    addQuickKnob ("dly.level", "Delay");
    addQuickKnob ("rev.level", "Reverb");
    addQuickKnob ("global.mix", "Mix");
    addQuickKnob ("global.output", "Output");

    addAndMakeVisible (quickView);
}

void VocalRackEditor::configureDetailTabs()
{
    detailTabs.setTabBarDepth (44);
    detailTabs.setOutline (0);
    detailTabs.setColour (juce::TabbedButtonBar::tabTextColourId, VocalRackLookAndFeel::dimText());
    detailTabs.setColour (juce::TabbedButtonBar::frontTextColourId, VocalRackLookAndFeel::text());
    detailTabs.setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    detailTabs.setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    detailTabs.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    detailTabs.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

    auto makeGrid = [this] (const juce::String& name, const juce::String& title, int columns)
    {
        auto* grid = new ControlGrid();
        grid->setColumns (columns);
        grid->setTitle (title);
        detailTabs.addTab (name, VocalRackLookAndFeel::panel(), grid, true);
        return grid;
    };

    auto* cleanup = makeGrid ("Cleanup", "CLEANUP", 7);
    addPower (*cleanup, "exp.bypass", "Expander");
    addPower (*cleanup, "dn.bypass", "Noise");
    addKnob (*cleanup, "exp.threshold", "Open point");
    addKnob (*cleanup, "exp.range", "Depth");
    addKnob (*cleanup, "exp.attack", "Speed");
    addKnob (*cleanup, "exp.hold", "Pause");
    addKnob (*cleanup, "exp.release", "Recovery");
    addKnob (*cleanup, "dn.amount", "Reduction");
    addKnob (*cleanup, "dn.maxReduction", "Ceiling");

    auto* pitch = makeGrid ("Pitch", "PITCH CORRECTION", 5);
    addPower (*pitch, "pitch.bypass", "Enabled");
    addCombo (*pitch, "pitch.range", "Range");
    formantToggle = addToggle (*pitch, "pitch.formant", "Formant Pres.");
    addKnob (*pitch, "pitch.amount", "Correction amount");
    addKnob (*pitch, "pitch.retune", "Retune time");
    addKnob (*pitch, "pitch.humanize", "Natural movement");
    addKnob (*pitch, "pitch.a4", "A4 reference");
    addKnob (*pitch, "pitch.formantShift", "Formant shift");

    auto* cleanEq = makeGrid ("Clean EQ", "CLEAN EQ", 7);
    addPower (*cleanEq, "ceq.bypass", "Enabled");
    addKnob (*cleanEq, "ceq.hpf");
    addKnob (*cleanEq, "ceq.cut1Freq");
    addKnob (*cleanEq, "ceq.cut1Gain");
    addKnob (*cleanEq, "ceq.cut1Q");
    addKnob (*cleanEq, "ceq.cut2Freq");
    addKnob (*cleanEq, "ceq.cut2Gain");
    addKnob (*cleanEq, "ceq.cut2Q");

    auto* comp = makeGrid ("Comp", "COMPRESSOR", 8);
    addPower (*comp, "comp.bypass", "Enabled");
    addKnob (*comp, "comp.threshold", "Gain reduction");
    addKnob (*comp, "comp.ratio", "Ratio");
    addKnob (*comp, "comp.attack", "Speed");
    addKnob (*comp, "comp.release", "Recovery");
    addKnob (*comp, "comp.knee", "Softness");
    addKnob (*comp, "comp.makeup", "Makeup");
    addKnob (*comp, "comp.mix", "Parallel");
    addKnob (*comp, "comp.detHPF", "Detector");

    auto* deess = makeGrid ("De-ess", "DE-ESSER", 4);
    addPower (*deess, "ds.bypass", "Enabled");
    addToggle (*deess, "ds.audition", "Audition");
    addKnob (*deess, "ds.freq", "Detection band");
    addKnob (*deess, "ds.threshold", "Threshold");
    addKnob (*deess, "ds.range", "Max reduction");
    addKnob (*deess, "ds.release", "Recovery");

    auto* toneEq = makeGrid ("Tone EQ", "TONE EQ", 7);
    addPower (*toneEq, "teq.bypass", "Enabled");
    addKnob (*toneEq, "teq.lowFreq");
    addKnob (*toneEq, "teq.lowGain");
    addKnob (*toneEq, "teq.presFreq");
    addKnob (*toneEq, "teq.presGain");
    addKnob (*toneEq, "teq.presQ");
    addKnob (*toneEq, "teq.highFreq");
    addKnob (*toneEq, "teq.highGain");

    auto* deq = makeGrid ("Dyn EQ", "DYNAMIC EQ", 8);
    addPower (*deq, "deq1.on", "Band 1");
    addPower (*deq, "deq2.on", "Band 2");

    for (const char* id : { "deq1.freq", "deq1.q", "deq1.gain", "deq1.threshold",
                            "deq1.ratio", "deq1.range", "deq1.attack", "deq1.release" })
        addKnob (*deq, id);

    for (const char* id : { "deq2.freq", "deq2.q", "deq2.gain", "deq2.threshold",
                            "deq2.ratio", "deq2.range", "deq2.attack", "deq2.release" })
        addKnob (*deq, id);

    deq->setTopComponent (new DynamicEQGraph (processor.getAPVTS(), processor.getEngine()), 172);

    auto* color = makeGrid ("Color", "COLOR", 3);
    addPower (*color, "sat.bypass", "Enabled");
    addCombo (*color, "sat.mode", "Mode");
    addKnob (*color, "sat.drive", "Drive");
    addKnob (*color, "sat.blend", "Blend");
    addKnob (*color, "sat.output", "Output");

    auto* space = makeGrid ("Space", "SPACE", 7);
    addPower (*space, "dly.bypass", "Delay");
    addPower (*space, "rev.bypass", "Reverb");
    addCombo (*space, "rev.type", "Type");
    addToggle (*space, "dly.sync", "Sync");
    addToggle (*space, "dly.pingpong", "Ping-pong");
    addCombo (*space, "dly.division", "Division");
    addCombo (*space, "dly.feel", "Feel");
    addKnob (*space, "dly.ms");
    addKnob (*space, "dly.feedback");
    addKnob (*space, "dly.level");
    addKnob (*space, "dly.lowCut");
    addKnob (*space, "dly.highCut");
    addKnob (*space, "dly.duck");
    addKnob (*space, "dly.manualBpm");
    addKnob (*space, "rev.predelay");
    addKnob (*space, "rev.decay");
    addKnob (*space, "rev.damping");
    addKnob (*space, "rev.lowCut");
    addKnob (*space, "rev.highCut");
    addKnob (*space, "rev.level");
    addKnob (*space, "rev.duck");

    auto* limiter = makeGrid ("Limit", "LIMITER", 4);
    addPower (*limiter, "lim.bypass", "Enabled");
    addKnob (*limiter, "lim.ceiling", "Ceiling");
    addKnob (*limiter, "lim.release", "Release");
    addKnob (*limiter, "global.mix", "Global mix");
    addKnob (*limiter, "global.output", "Output");

    detailTabs.setCurrentTabIndex (0);
    detailTabs.setVisible (false);
    addAndMakeVisible (detailTabs);
    detailTabs.setComponentID ("DetailTabs");
}

void VocalRackEditor::setView (bool detail)
{
    detailMode = detail;
    quickView.setVisible (! detailMode);
    detailTabs.setVisible (detailMode);
    quickButton.setToggleState (! detailMode, juce::dontSendNotification);
    detailButton.setToggleState (detailMode, juce::dontSendNotification);

    const auto styleViewButton = [this] (juce::TextButton& button, bool active)
    {
        button.setColour (juce::TextButton::buttonColourId,
                          active ? VocalRackLookAndFeel::accent()
                                 : VocalRackLookAndFeel::panelLight());
        button.setColour (juce::TextButton::textColourOffId,
                          active ? VocalRackLookAndFeel::background()
                                 : VocalRackLookAndFeel::text());
        button.repaint();
    };

    styleViewButton (quickButton, ! detailMode);
    styleViewButton (detailButton, detailMode);
    resized();
}

void VocalRackEditor::loadRelativePreset (int delta)
{
    const auto& entries = processor.getPresets().getEntries();

    if (entries.empty())
        return;

    int index = processor.getPresets().indexOfId (processor.getPresetId());

    if (index < 0)
        index = 0;

    index = (index + delta + (int) entries.size()) % (int) entries.size();
    processor.requestPresetLoad (entries[(size_t) index].id, processor.getKeepSongKey());
    stateDirty = true;

    if (browser.isVisible())
        browser.refresh();
}

void VocalRackEditor::toggleBrowser()
{
    if (browser.isVisible())
    {
        browser.setVisible (false);
    }
    else
    {
        browser.showForCurrentPreset();
    }

    browseButton.setToggleState (browser.isVisible(), juce::dontSendNotification);
    matchABButton.setToggleState (processor.isLoudnessMatch(), juce::dontSendNotification);
    browseButton.setColour (juce::TextButton::buttonColourId,
                            browser.isVisible() ? VocalRackLookAndFeel::accentSoft()
                                                : VocalRackLookAndFeel::panelLight());
}

void VocalRackEditor::toggleMaskNote (int pitchClass)
{
    auto& state = processor.getAPVTS();
    const int scale = (int) std::lround (state.getRawParameterValue ("pitch.scale")->load());
    const int tonic = (int) std::lround (state.getRawParameterValue ("pitch.tonic")->load());
    int mask = (int) std::lround (state.getRawParameterValue ("pitch.mask")->load());

    if (scale != 3)
        mask = maskForScale (scale, tonic);

    mask ^= 1 << juce::jlimit (0, 11, pitchClass);
    mask &= 0x0FFF;

    if (scale != 3)
    {
        if (auto* scaleParameter = state.getParameter ("pitch.scale"))
            scaleParameter->setValueNotifyingHost (scaleParameter->convertTo0to1 (3.0f));
    }

    if (auto* maskParameter = state.getParameter ("pitch.mask"))
        maskParameter->setValueNotifyingHost (maskParameter->convertTo0to1 ((float) mask));

    stateDirty = true;
}

void VocalRackEditor::showNotice (const juce::String& message)
{
    lastNotice = message;
    noticeLabel.setText (message, juce::dontSendNotification);
    noticeLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    noticeExpiryMs = juce::Time::getMillisecondCounterHiRes() + 8000.0;
}

void VocalRackEditor::timerCallback()
{
    if (browseButton.getToggleState() != browser.isVisible())
    {
        browseButton.setToggleState (browser.isVisible(), juce::dontSendNotification);
    matchABButton.setToggleState (processor.isLoudnessMatch(), juce::dontSendNotification);
        browseButton.setColour (juce::TextButton::buttonColourId,
                                browser.isVisible() ? VocalRackLookAndFeel::accent()
                                                    : VocalRackLookAndFeel::panelLight());
        browseButton.setColour (juce::TextButton::textColourOffId,
                                browser.isVisible() ? VocalRackLookAndFeel::background()
                                                    : VocalRackLookAndFeel::text());
        browseButton.repaint();
    }

    if (stateDirty)
    {
        refreshFromProcessor();
        stateDirty = false;
    }

    if (noticeExpiryMs > 0.0
        && juce::Time::getMillisecondCounterHiRes() > noticeExpiryMs)
    {
        noticeExpiryMs = 0.0;
        noticeLabel.setText ("96 factory presets · Keep Song Key is on for factory browsing.",
                             juce::dontSendNotification);
        noticeLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    }
}

void VocalRackEditor::refreshFromProcessor()
{
    auto& state = processor.getAPVTS();
    auto raw = [&state] (const char* id)
    {
        return state.getRawParameterValue (id)->load();
    };

    const auto fullPresetName = processor.getPresetName();
    presetNameLabel.setText (fullPresetName + (processor.isEdited() ? "  *" : ""),
                             juce::dontSendNotification);
    presetNameLabel.setTooltip (fullPresetName + (processor.isEdited() ? " (edited)" : ""));

    const bool track = processor.getQuality() == QualityMode::track;
    const bool spectralOff = track
                             && raw ("dn.bypass") < 0.5f
                             && raw ("dn.amount") > 0.05f;

    juce::String meta = processor.getPresetBank();
    meta << (processor.isFactoryPreset() ? "  ·  FACTORY" : "  ·  USER")
         << (track ? "  ·  TRACK" : "  ·  MIX");

    presetMetaLabel.setText (meta, juce::dontSendNotification);
    presetMetaLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    presetMetaLabel.setTooltip (meta
                                    + (spectralOff
                                           ? "\nThis preset uses spectral cleanup, which is unavailable in Track. Switch Quality to Mix to hear it."
                                           : "\nPreset bank and configuration."));
    spectralLabel.setVisible (spectralOff);
    spectralLabel.setText (spectralOff
                               ? "SPECTRAL CLEANUP NEEDS MIX (TRACK ACTIVE)"
                               : juce::String(),
                           juce::dontSendNotification);

    const int latency = processor.getReportedLatencySamples();
    const double sampleRate = processor.getSampleRate() > 1000.0 ? processor.getSampleRate() : 48000.0;
    latencyLabel.setText ("Latency  " + juce::String (latency) + " samples  ·  "
                              + juce::String (latency / sampleRate * 1000.0, 2) + " ms",
                          juce::dontSendNotification);

    keepKeyButton.setToggleState (processor.getKeepSongKey(), juce::dontSendNotification);
    abAButton.setToggleState (processor.getActiveAB() == 0, juce::dontSendNotification);
    abBButton.setToggleState (processor.getActiveAB() == 1, juce::dontSendNotification);
    qualityBox.setSelectedId (track ? 1 : 2, juce::dontSendNotification);
    browseButton.setToggleState (browser.isVisible(), juce::dontSendNotification);
    matchABButton.setToggleState (processor.isLoudnessMatch(), juce::dontSendNotification);

    if (formantToggle != nullptr)
    {
        formantToggle->setEnabled (! track);
        formantToggle->setTooltip (track
                                       ? "Formant preservation is available in Mix quality. Switch Quality to Mix to enable it."
                                       : "Preserve the vocal-tract formant envelope while tuning.");
    }

    const int scale = (int) std::lround (raw ("pitch.scale"));
    const int tonic = (int) std::lround (raw ("pitch.tonic"));
    int mask = (int) std::lround (raw ("pitch.mask"));

    if (scale != 3)
        mask = maskForScale (scale, tonic);

    keyLabel.setText ("KEY", juce::dontSendNotification);
    maskLabel.setText ("NOTE MASK  ·  " + pitchClassName (tonic) + " " + scaleName (scale)
                           + "  ·  " + juce::String (countBits (mask)) + " NOTES",
                       juce::dontSendNotification);
    maskLabel.setTooltip ("Key " + pitchClassName (tonic) + ", " + scaleName (scale)
                              + ", " + juce::String (countBits (mask)) + " enabled notes.");

    auto rateText = juce::String (sampleRate / 1000.0, 1);

    if (rateText.endsWith (".0"))
        rateText = rateText.dropLastCharacters (2);

    sampleRateLabel.setText (rateText + " kHz", juce::dontSendNotification);
    ceilingLabel.setText ("Limiter ceiling  " + juce::String (raw ("lim.ceiling"), 1) + " dB",
                          juce::dontSendNotification);
}

void VocalRackEditor::paint (juce::Graphics& g)
{
    g.fillAll (VocalRackLookAndFeel::background());

    auto header = juce::Rectangle<float> (8.0f, 8.0f, (float) getWidth() - 16.0f, 84.0f);
    auto status = juce::Rectangle<float> (8.0f, 98.0f, (float) getWidth() - 16.0f, 144.0f);

    g.setColour (VocalRackLookAndFeel::panel());
    g.fillRoundedRectangle (header, 10.0f);
    g.fillRoundedRectangle (status, 10.0f);

    g.setColour (VocalRackLookAndFeel::line().withAlpha (0.8f));
    g.drawRoundedRectangle (header, 10.0f, 1.0f);
    g.drawRoundedRectangle (status, 10.0f, 1.0f);

    g.setColour (VocalRackLookAndFeel::accent());
    g.fillRoundedRectangle (8.0f, 8.0f, 3.0f, 84.0f, 2.0f);

    g.setColour (VocalRackLookAndFeel::line());
    g.fillRect (20, 50, getWidth() - 40, 1);
    g.fillRect (20, getHeight() - 44, getWidth() - 40, 1);
}

void VocalRackEditor::resized()
{
    auto area = getLocalBounds();

    // Header, row one: identity, preset navigation, A/B and global power.
    const int headerX = 20;
    const int topY = 12;
    logoLabel.setBounds (headerX, 8, 150, 32);
    productLabel.setBounds (headerX + 166, 13, 150, 28);

    const int rightRail = 108;
    const int globalX = getWidth() - rightRail - 14;
    const int copyX = globalX - 70;
    const int bX = copyX - 40;
    const int aX = bX - 40;
    const int matchX = aX - 40;
    const int presetEnd = matchX - 18;

    globalBypass.setBounds (globalX, topY, 92, 36);
    copyABButton.setBounds (copyX, topY + 2, 62, 32);
    abBButton.setBounds (bX, topY + 2, 32, 32);
    abAButton.setBounds (aX, topY + 2, 32, 32);
    matchABButton.setBounds (matchX, topY + 2, 32, 32);

    const int presetStart = headerX + 300;
    previousButton.setBounds (presetStart, topY, 30, 32);
    browseButton.setBounds (presetEnd - 82, topY + 2, 82, 32);
    nextButton.setBounds (presetEnd - 82 - 38, topY, 30, 32);
    const int nameX = presetStart + 38;
    const int nameWidth = juce::jmax (120, nextButton.getX() - 8 - nameX);
    presetNameLabel.setBounds (nameX, 9, nameWidth, 26);
    presetMetaLabel.setBounds (nameX, 33, nameWidth, 14);

    // Header, row two: view, quality, key, scale, keep.
    quickButton.setBounds (headerX, 58, 66, 28);
    detailButton.setBounds (headerX + 70, 58, 72, 28);
    qualityHintLabel.setBounds (headerX + 166, 63, 58, 18);
    qualityBox.setBounds (headerX + 226, 58, 96, 28);
    keyLabel.setBounds (headerX + 344, 63, 26, 18);
    tonicBox.setBounds (headerX + 372, 58, 62, 28);
    scaleBox.setBounds (headerX + 442, 58, 126, 28);
    keepKeyButton.setBounds (headerX + 588, 60, 146, 24);

    // Status: pitch, note mask, meters.
    auto status = juce::Rectangle<int> (20, 108, getWidth() - 40, 124);
    const int meterWidth = getWidth() >= 1050 ? 250 : 226;
    const int pitchWidth = getWidth() >= 1050 ? 320 : 292;
    pitchPanel.setBounds (status.removeFromLeft (pitchWidth));
    status.removeFromLeft (14);
    meters.setBounds (status.removeFromRight (meterWidth));
    status.removeFromRight (14);

    maskLabel.setBounds (status.removeFromTop (22));
    keyboard.setBounds (status.withHeight (juce::jmin (62, status.getHeight())).reduced (2, 0));

    // Body card. The card itself is drawn by ControlGrid / detail content.
    const int bodyTop = 252;
    const int footerTop = getHeight() - 44;
    quickView.setBounds (8, bodyTop, getWidth() - 16, footerTop - bodyTop - 4);
    detailTabs.setBounds (8, bodyTop, getWidth() - 16, footerTop - bodyTop - 4);

    // Footer status rail.
    auto footer = juce::Rectangle<int> (20, footerTop + 10, getWidth() - 40, 24);
    sampleRateLabel.setBounds (footer.removeFromLeft (92));
    footer.removeFromLeft (12);
    latencyLabel.setBounds (footer.removeFromLeft (250));
    footer.removeFromLeft (12);
    ceilingLabel.setBounds (footer.removeFromLeft (190));
    footer.removeFromLeft (12);
    noticeLabel.setBounds (footer);
    if (getWidth() >= 1050)
        spectralLabel.setBounds (getWidth() - 360, 58, 336, 28);
    else
        spectralLabel.setBounds (20, footerTop - 18, getWidth() - 40, 16);

    spectralLabel.toFront (false);

    browser.setBounds (getLocalBounds());
}


} // namespace vr
