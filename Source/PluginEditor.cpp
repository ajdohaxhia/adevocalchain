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

    qualityHintLabel.setFont (VocalRackLookAndFeel::font (7.5f, true));
    qualityHintLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    qualityHintLabel.setJustificationType (juce::Justification::centredRight);
    qualityHintLabel.setText ("STOP TRANSPORT TO CHANGE QUALITY", juce::dontSendNotification);
    qualityHintLabel.setTooltip ("Track and Mix use different internal resources and latency. Stop the transport before switching if the host cannot re-align live.");
    addAndMakeVisible (qualityHintLabel);

    addAndMakeVisible (quickButton);
    addAndMakeVisible (detailButton);
    quickButton.setComponentID ("QuickButton");
    detailButton.setComponentID ("DetailButton");
    quickButton.setClickingTogglesState (false);
    detailButton.setClickingTogglesState (false);
    quickButton.onClick = [this] { setView (false); };
    detailButton.onClick = [this] { setView (true); };

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
    logoLabel.setFont (VocalRackLookAndFeel::font (25.0f, true));
    logoLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    logoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (logoLabel);

    productLabel.setText ("VOCAL RACK", juce::dontSendNotification);
    productLabel.setFont (VocalRackLookAndFeel::font (9.5f, true));
    productLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::accent());
    productLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (productLabel);

    presetNameLabel.setFont (VocalRackLookAndFeel::font (15.0f, true));
    presetNameLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    presetNameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetNameLabel);

    presetMetaLabel.setFont (VocalRackLookAndFeel::font (8.5f, true));
    presetMetaLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    presetMetaLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetMetaLabel);

    latencyLabel.setFont (VocalRackLookAndFeel::font (9.0f));
    latencyLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    latencyLabel.setJustificationType (juce::Justification::centredRight);
    latencyLabel.setTooltip ("Plugin-reported latency. Track is pinned at or below 15 ms at 48 kHz; Mix adds the spectral path.");
    addAndMakeVisible (latencyLabel);

    spectralLabel.setFont (VocalRackLookAndFeel::font (8.5f, true));
    spectralLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::danger());
    spectralLabel.setJustificationType (juce::Justification::centredLeft);
    spectralLabel.setVisible (false);
    addAndMakeVisible (spectralLabel);

    for (auto* button : { &previousButton, &nextButton, &browseButton, &abAButton,
                          &abBButton, &copyABButton })
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
}

void VocalRackEditor::configureStatusBar()
{
    addAndMakeVisible (meters);
    addAndMakeVisible (pitchPanel);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (keyLabel);

    keyLabel.setFont (VocalRackLookAndFeel::font (8.5f, true));
    keyLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    keyLabel.setJustificationType (juce::Justification::centredLeft);

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

void VocalRackEditor::addQuickKnob (const char* parameterId)
{
    auto* knob = new Knob (processor.getAPVTS(), parameterId);
    quickView.addGridControl (knob);
}

void VocalRackEditor::addQuickPower (const char* parameterId, const char* label)
{
    auto* power = new PowerButton (processor.getAPVTS(), parameterId, label);
    quickView.addHeaderControl (power);
}

void VocalRackEditor::addKnob (ControlGrid& grid, const char* parameterId)
{
    auto* knob = new Knob (processor.getAPVTS(), parameterId);
    grid.addGridControl (knob);
}

void VocalRackEditor::addPower (ControlGrid& grid, const char* parameterId, const char* label)
{
    auto* power = new PowerButton (processor.getAPVTS(), parameterId, label);
    grid.addHeaderControl (power);
}

void VocalRackEditor::addToggle (ControlGrid& grid, const char* parameterId, const char* label)
{
    auto* toggle = new juce::ToggleButton (label);
    toggle->setTitle (label);
    toggle->setWantsKeyboardFocus (true);
    toggle->setTooltip (juce::String (label) + " for this module.");
    grid.addHeaderControl (toggle);
    toggleAttachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.getAPVTS(), parameterId, *toggle));
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

    addQuickPower ("exp.bypass", "EXP");
    addQuickPower ("dn.bypass", "NOISE");
    addQuickPower ("pitch.bypass", "PITCH");
    addQuickPower ("comp.bypass", "COMP");
    addQuickPower ("ds.bypass", "DE-ESS");
    addQuickPower ("sat.bypass", "COLOR");
    addQuickPower ("dly.bypass", "DELAY");
    addQuickPower ("rev.bypass", "VERB");
    addQuickPower ("lim.bypass", "LIMIT");

    addQuickKnob ("global.input");
    addQuickKnob ("dn.amount");
    addQuickKnob ("pitch.amount");
    addQuickKnob ("pitch.retune");
    addQuickKnob ("comp.threshold");
    addQuickKnob ("ds.range");
    addQuickKnob ("teq.presGain");
    addQuickKnob ("sat.drive");
    addQuickKnob ("dly.level");
    addQuickKnob ("rev.level");
    addQuickKnob ("global.mix");
    addQuickKnob ("global.output");

    addAndMakeVisible (quickView);
}

void VocalRackEditor::configureDetailTabs()
{
    detailTabs.setTabBarDepth (30);
    detailTabs.setOutline (0);
    detailTabs.setColour (juce::TabbedButtonBar::tabTextColourId, VocalRackLookAndFeel::dimText());
    detailTabs.setColour (juce::TabbedButtonBar::frontTextColourId, VocalRackLookAndFeel::text());
    detailTabs.setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colour (0xff2a2e35));
    detailTabs.setColour (juce::TabbedButtonBar::frontOutlineColourId, VocalRackLookAndFeel::accent());
    detailTabs.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    detailTabs.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

    auto makeGrid = [this] (const juce::String& name, int columns)
    {
        auto* grid = new ControlGrid();
        grid->setColumns (columns);
        detailTabs.addTab (name, VocalRackLookAndFeel::panel(), grid, true);
        return grid;
    };

    auto* cleanup = makeGrid ("CLEANUP", 7);
    addPower (*cleanup, "exp.bypass", "EXP");
    addPower (*cleanup, "dn.bypass", "NOISE");
    addKnob (*cleanup, "exp.threshold");
    addKnob (*cleanup, "exp.range");
    addKnob (*cleanup, "exp.attack");
    addKnob (*cleanup, "exp.hold");
    addKnob (*cleanup, "exp.release");
    addKnob (*cleanup, "dn.amount");
    addKnob (*cleanup, "dn.maxReduction");

    auto* pitch = makeGrid ("PITCH", 5);
    addPower (*pitch, "pitch.bypass", "PITCH");
    addCombo (*pitch, "pitch.range", "RANGE");
    addKnob (*pitch, "pitch.amount");
    addKnob (*pitch, "pitch.retune");
    addKnob (*pitch, "pitch.humanize");
    addKnob (*pitch, "pitch.a4");

    auto* eq = makeGrid ("EQ", 7);
    addPower (*eq, "ceq.bypass", "CLEAN EQ");
    addPower (*eq, "teq.bypass", "TONE EQ");
    addKnob (*eq, "ceq.hpf");
    addKnob (*eq, "ceq.cut1Freq");
    addKnob (*eq, "ceq.cut1Gain");
    addKnob (*eq, "ceq.cut1Q");
    addKnob (*eq, "ceq.cut2Freq");
    addKnob (*eq, "ceq.cut2Gain");
    addKnob (*eq, "ceq.cut2Q");
    addKnob (*eq, "teq.lowFreq");
    addKnob (*eq, "teq.lowGain");
    addKnob (*eq, "teq.presFreq");
    addKnob (*eq, "teq.presGain");
    addKnob (*eq, "teq.presQ");
    addKnob (*eq, "teq.highFreq");
    addKnob (*eq, "teq.highGain");

    auto* dynamics = makeGrid ("DYNAMICS", 7);
    addPower (*dynamics, "comp.bypass", "COMP");
    addPower (*dynamics, "ds.bypass", "DE-ESS");
    addToggle (*dynamics, "ds.audition", "AUDITION");
    addKnob (*dynamics, "comp.threshold");
    addKnob (*dynamics, "comp.ratio");
    addKnob (*dynamics, "comp.attack");
    addKnob (*dynamics, "comp.release");
    addKnob (*dynamics, "comp.knee");
    addKnob (*dynamics, "comp.makeup");
    addKnob (*dynamics, "comp.mix");
    addKnob (*dynamics, "comp.detHPF");
    addKnob (*dynamics, "ds.freq");
    addKnob (*dynamics, "ds.threshold");
    addKnob (*dynamics, "ds.range");
    addKnob (*dynamics, "ds.release");

    auto* color = makeGrid ("COLOR", 4);
    addPower (*color, "sat.bypass", "SAT");
    addCombo (*color, "sat.mode", "MODE");
    addKnob (*color, "sat.drive");
    addKnob (*color, "sat.blend");
    addKnob (*color, "sat.output");

    auto* delay = makeGrid ("DELAY", 7);
    addPower (*delay, "dly.bypass", "DELAY");
    addToggle (*delay, "dly.sync", "SYNC");
    addToggle (*delay, "dly.pingpong", "PING-PONG");
    addCombo (*delay, "dly.division", "DIVISION");
    addCombo (*delay, "dly.feel", "FEEL");
    addKnob (*delay, "dly.ms");
    addKnob (*delay, "dly.feedback");
    addKnob (*delay, "dly.level");
    addKnob (*delay, "dly.lowCut");
    addKnob (*delay, "dly.highCut");
    addKnob (*delay, "dly.duck");
    addKnob (*delay, "dly.manualBpm");

    auto* reverb = makeGrid ("REVERB", 7);
    addPower (*reverb, "rev.bypass", "REVERB");
    addCombo (*reverb, "rev.type", "TYPE");
    addKnob (*reverb, "rev.predelay");
    addKnob (*reverb, "rev.decay");
    addKnob (*reverb, "rev.damping");
    addKnob (*reverb, "rev.lowCut");
    addKnob (*reverb, "rev.highCut");
    addKnob (*reverb, "rev.level");
    addKnob (*reverb, "rev.duck");

    auto* limiter = makeGrid ("LIMITER", 4);
    addPower (*limiter, "lim.bypass", "LIMITER");
    addKnob (*limiter, "lim.ceiling");
    addKnob (*limiter, "lim.release");
    addKnob (*limiter, "global.mix");
    addKnob (*limiter, "global.output");

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
    quickButton.setColour (juce::TextButton::buttonColourId,
                           ! detailMode ? VocalRackLookAndFeel::accentSoft()
                                        : VocalRackLookAndFeel::panel());
    detailButton.setColour (juce::TextButton::buttonColourId,
                            detailMode ? VocalRackLookAndFeel::accentSoft()
                                       : VocalRackLookAndFeel::panel());
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

    presetNameLabel.setText (processor.getPresetName()
                                 + (processor.isEdited() ? "  *" : ""),
                             juce::dontSendNotification);

    const bool track = processor.getQuality() == QualityMode::track;
    const bool spectralOff = track
                             && raw ("dn.bypass") < 0.5f
                             && raw ("dn.amount") > 0.05f;

    juce::String meta = processor.getPresetBank();
    meta << (processor.isFactoryPreset() ? "  ·  FACTORY" : "  ·  USER")
         << (track ? "  ·  TRACK" : "  ·  MIX");

    presetMetaLabel.setText (meta, juce::dontSendNotification);
    presetMetaLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    presetMetaLabel.setTooltip (spectralOff
                                    ? "This preset uses spectral cleanup, which is unavailable in Track. Switch Quality to Mix to hear it."
                                    : "Preset bank and configuration.");
    spectralLabel.setVisible (spectralOff);
    spectralLabel.setText (spectralOff
                               ? "SPECTRAL CLEANUP NEEDS MIX (TRACK ACTIVE)"
                               : juce::String(),
                           juce::dontSendNotification);

    const int latency = processor.getReportedLatencySamples();
    const double sampleRate = processor.getSampleRate() > 1000.0 ? processor.getSampleRate() : 48000.0;
    latencyLabel.setText ("LATENCY  " + juce::String (latency) + " smp  ·  "
                              + juce::String (latency / sampleRate * 1000.0, 2) + " ms",
                          juce::dontSendNotification);

    keepKeyButton.setToggleState (processor.getKeepSongKey(), juce::dontSendNotification);
    abAButton.setToggleState (processor.getActiveAB() == 0, juce::dontSendNotification);
    abBButton.setToggleState (processor.getActiveAB() == 1, juce::dontSendNotification);
    qualityBox.setSelectedId (track ? 1 : 2, juce::dontSendNotification);
    browseButton.setToggleState (browser.isVisible(), juce::dontSendNotification);

    const int scale = (int) std::lround (raw ("pitch.scale"));
    const int tonic = (int) std::lround (raw ("pitch.tonic"));
    int mask = (int) std::lround (raw ("pitch.mask"));

    if (scale != 3)
        mask = maskForScale (scale, tonic);

    keyLabel.setText ("KEY " + pitchClassName (tonic) + "  ·  " + scaleName (scale)
                          + "  ·  " + juce::String (countBits (mask)) + " NOTES",
                      juce::dontSendNotification);
    keyLabel.setTooltip ("Key " + pitchClassName (tonic) + ", " + scaleName (scale)
                             + ", " + juce::String (countBits (mask)) + " enabled notes.");
}

void VocalRackEditor::paint (juce::Graphics& g)
{
    g.fillAll (VocalRackLookAndFeel::background());

    g.setColour (VocalRackLookAndFeel::panel());
    g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 6.0f, (float) getWidth() - 16.0f, 68.0f), 6.0f);
    g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 80.0f, (float) getWidth() - 16.0f, 142.0f), 6.0f);

    g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.9f));
    g.fillRect (0, 0, 3, getHeight());

    g.setColour (juce::Colour (0xff2b2f36));
    g.fillRect (8, 76, getWidth() - 16, 1);

    g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.16f));
    g.fillEllipse ((float) getWidth() - 240.0f, -100.0f, 320.0f, 240.0f);
}

void VocalRackEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (76);

    auto status = area.removeFromTop (146).reduced (12, 6);
    auto footer = area.removeFromBottom (28).reduced (12, 0);
    auto body = area.reduced (12, 0);

    // Top bar
    int x = 12;
    logoLabel.setBounds (x, 6, 150, 32);
    productLabel.setBounds (x + 1, 38, 150, 16);
    x += 158;

    previousButton.setBounds (x, 11, 24, 28);
    x += 26;
    presetNameLabel.setBounds (x, 6, 236, 24);
    presetMetaLabel.setBounds (x, 31, 236, 15);
    x += 240;
    nextButton.setBounds (x, 11, 24, 28);
    x += 28;
    browseButton.setBounds (x, 11, 70, 28);
    x += 78;

    abAButton.setBounds (x, 11, 26, 28);
    x += 28;
    abBButton.setBounds (x, 11, 26, 28);
    x += 28;
    copyABButton.setBounds (x, 11, 54, 28);
    x += 62;

    keepKeyButton.setBounds (x, 12, 108, 26);
    x += 114;
    qualityBox.setBounds (x, 11, 86, 28);
    x += 92;
    globalBypass.setBounds (x, 11, 76, 28);

    latencyLabel.setBounds (getWidth() - 210, 48, 198, 16);
    spectralLabel.setBounds (196, 49, 440, 16);

    // Status strip. Rebalance at narrower widths so the 12-key mask stays readable.
    const int statusWidth = status.getWidth();
    const int meterWidth = statusWidth >= 1050 ? 300 : 250;
    const int pitchWidth = statusWidth >= 1050 ? 330 : 306;
    meters.setBounds (status.removeFromLeft (meterWidth));
    status.removeFromLeft (8);
    pitchPanel.setBounds (status.removeFromLeft (pitchWidth));
    status.removeFromLeft (8);

    auto keyboardArea = status;
    auto header = keyboardArea.removeFromTop (22);
    const int combosWidth = 170;

    if (header.getWidth() > combosWidth + 40)
    {
        keyLabel.setBounds (header.removeFromLeft (header.getWidth() - combosWidth));
        header.removeFromLeft (4);
        tonicBox.setBounds (header.removeFromLeft (64));
        header.removeFromLeft (4);
        scaleBox.setBounds (header);
    }
    else
    {
        keyLabel.setBounds (header);
    }

    keyboardArea.removeFromTop (2);
    keyboard.setBounds (keyboardArea.withHeight (juce::jmin (64, keyboardArea.getHeight())));

    // Footer view toggles and notice
    quickButton.setBounds (footer.removeFromLeft (54));
    footer.removeFromLeft (4);
    detailButton.setBounds (footer.removeFromLeft (58));
    footer.removeFromLeft (10);
    qualityHintLabel.setBounds (footer.removeFromRight (180));
    footer.removeFromRight (8);
    noticeLabel.setBounds (footer);

    quickView.setBounds (body);
    detailTabs.setBounds (body);

    const int browserWidth = juce::jmin (800, getWidth() - 40);
    const int browserHeight = juce::jmin (560, getHeight() - 40);
    juce::Rectangle<int> browserBounds (0, 0, browserWidth, browserHeight);
    browserBounds.setCentre (getLocalBounds().getCentre());
    browser.setBounds (browserBounds);
}

} // namespace vr
