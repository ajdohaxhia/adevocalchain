#include "RackComponents.h"

#include "PluginProcessor.h"
#include "VocalRackLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace vr
{

namespace
{
constexpr float kMinDb = -60.0f;

juce::String shortUnit (const ParamSpec& spec)
{
    return juce::String (spec.unit);
}
} // namespace

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId)
{
    spec = findParamSpec (parameterId);
    jassert (spec != nullptr);

    if (spec == nullptr)
        return;

    nameLabel.setText (juce::String (spec->name).toUpperCase(), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    nameLabel.setFont (VocalRackLookAndFeel::font (10.5f, true));
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    slider.setRange (spec->minValue, spec->maxValue);
    slider.setNumDecimalPlacesToDisplay (spec->maxValue - spec->minValue <= 4.0f ? 2
                                      : spec->maxValue - spec->minValue <= 40.0f ? 1
                                      : 0);
    slider.setDoubleClickReturnValue (true, spec->defaultValue);
    slider.setName (spec->name);
    slider.setTitle (spec->name);
    slider.setDescription (spec->description);
    slider.setTooltip (juce::String (spec->name) + " — " + spec->description);
    slider.setWantsKeyboardFocus (true);
    slider.setScrollWheelEnabled (true);
    slider.setColour (juce::Slider::textBoxTextColourId, VocalRackLookAndFeel::dimText());
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterId, slider);

    const int decimals = (spec->maxValue - spec->minValue) <= 4.0f ? 2
                       : (spec->maxValue - spec->minValue) <= 60.0f ? 1
                       : 0;
    const juce::String suffix = shortUnit (*spec).isEmpty() ? juce::String()
                                                           : " " + shortUnit (*spec);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.textFromValueFunction = [decimals, suffix] (double value)
    {
        auto text = juce::String (value, decimals);
        return suffix.isNotEmpty() ? text + suffix : text;
    };
    const float minimumValue = spec->minValue;
    const float maximumValue = spec->maxValue;
    slider.valueFromTextFunction = [minimumValue, maximumValue] (const juce::String& text)
    {
        return (double) juce::jlimit (minimumValue, maximumValue, text.getFloatValue());
    };
    slider.updateText();

    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds (area.removeFromTop (14));
    area.removeFromTop (2);
    slider.setBounds (area);
}

void Knob::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

PowerButton::PowerButton (juce::AudioProcessorValueTreeState& state,
                          const juce::String& parameterId,
                          const juce::String& shortLabel)
    : Button (shortLabel), label (shortLabel)
{
    parameter = state.getParameter (parameterId);
    jassert (parameter != nullptr);
    setTooltip ("Toggle " + shortLabel);
    setWantsKeyboardFocus (true);
    setTitle (shortLabel);
    lastState = parameter != nullptr && parameter->getValue() >= 0.5f;
    startTimerHz (12);
}

void PowerButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = parameter != nullptr && parameter->getValue() < 0.5f;
    auto fill = on ? VocalRackLookAndFeel::accentSoft().withAlpha (0.32f)
                   : VocalRackLookAndFeel::panel();

    if (isButtonDown)
        fill = fill.brighter (0.08f);
    else if (isMouseOver)
        fill = fill.brighter (0.04f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (on ? VocalRackLookAndFeel::accent() : juce::Colour (0xff3a4048));
    g.drawRoundedRectangle (bounds, 4.0f, on ? 1.2f : 1.0f);

    g.setColour (on ? VocalRackLookAndFeel::accent() : VocalRackLookAndFeel::dimText());
    g.fillEllipse (bounds.getX() + 6.0f, bounds.getCentreY() - 2.5f, 5.0f, 5.0f);

    g.setColour (on ? VocalRackLookAndFeel::text() : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.5f, on));
    g.drawFittedText (label, bounds.toNearestInt().withTrimmedLeft (16).withTrimmedRight (4),
                      juce::Justification::centredLeft, 1);

    if (hasKeyboardFocus (true))
    {
        g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }
}

void PowerButton::clicked()
{
    if (parameter == nullptr)
        return;

    const bool currentlyOn = parameter->getValue() < 0.5f;
    parameter->setValueNotifyingHost (currentlyOn ? 1.0f : 0.0f);
    repaint();
}

void PowerButton::timerCallback()
{
    if (parameter == nullptr)
        return;

    const bool state = parameter->getValue() >= 0.5f;

    if (state != lastState)
    {
        lastState = state;
        repaint();
    }
}

MeterPanel::MeterPanel (Engine& engineIn) : engine (engineIn)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void MeterPanel::timerCallback()
{
    const auto follow = [] (float& value, float target, float up, float down)
    {
        const float coefficient = target > value ? up : down;
        value += coefficient * (target - value);
    };

    follow (inputDb, engine.getInputRmsDb(), 0.55f, 0.18f);
    follow (outputDb, engine.getOutputPeakDb(), 0.7f, 0.12f);
    follow (compGr, engine.getCompressorGainReductionDb(), 0.7f, 0.2f);
    follow (limiterGr, engine.getLimiterGainReductionDb(), 0.8f, 0.2f);

    if (outputDb >= peakHoldDb)
    {
        peakHoldDb = outputDb;
        peakHoldTicks = 30;
    }
    else if (--peakHoldTicks <= 0)
    {
        peakHoldDb = juce::jmax (outputDb, peakHoldDb - 0.5f);
    }

    repaint();
}

float MeterPanel::meterPosition (float db) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db - kMinDb) / -kMinDb);
}

void MeterPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (8, 6);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.5f, true));
    g.drawText ("LEVELS", area.removeFromTop (14), juce::Justification::centredLeft);

    auto drawMeter = [&g] (juce::Rectangle<int> row, const juce::String& label, float db,
                           float peakDb, juce::Colour colour, bool hold)
    {
        auto labelArea = row.removeFromLeft (30);
        auto valueArea = row.removeFromRight (46);
        auto bar = row.reduced (2, 5);

        g.setColour (VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (9.5f, true));
        g.drawText (label, labelArea, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff2b2f36));
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);

        const float position = meterPosition (db);
        auto fill = bar.toFloat().withWidth (bar.getWidth() * position);
        g.setColour (colour);
        g.fillRoundedRectangle (fill, 2.0f);

        if (hold && peakDb > kMinDb)
        {
            const int x = bar.getX() + (int) (bar.getWidth() * meterPosition (peakDb));
            g.setColour (VocalRackLookAndFeel::text().withAlpha (0.75f));
            g.fillRect (x, bar.getY(), 1, bar.getHeight());
        }

        g.setColour (db <= kMinDb + 0.5f ? VocalRackLookAndFeel::dimText()
                                         : VocalRackLookAndFeel::text());
        g.setFont (VocalRackLookAndFeel::font (10.0f));
        g.drawText (db <= kMinDb + 0.5f ? "-inf" : juce::String (db, 1) + " dB",
                    valueArea, juce::Justification::centredRight);
    };

    auto inputRow = area.removeFromTop (24);
    drawMeter (inputRow, "IN", inputDb, -60.0f, VocalRackLookAndFeel::accent().withAlpha (0.7f), false);

    auto outputRow = area.removeFromTop (24);
    drawMeter (outputRow, "OUT", outputDb, peakHoldDb, VocalRackLookAndFeel::accent(), true);

    area.removeFromTop (4);
    auto grRow = area.removeFromTop (18);
    auto drawGr = [&g] (juce::Rectangle<int> row, const juce::String& label, float reduction,
                        juce::Colour colour)
    {
        auto labelArea = row.removeFromLeft (30);
        auto valueArea = row.removeFromRight (46);
        auto bar = row.reduced (2, 4);

        g.setColour (VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (9.0f, true));
        g.drawText (label, labelArea, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff2b2f36));
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);

        const float amount = juce::jlimit (0.0f, 1.0f, -reduction / 24.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (bar.toFloat().withWidth (bar.getWidth() * amount), 2.0f);

        g.setColour (reduction < -0.05f ? VocalRackLookAndFeel::text()
                                        : VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (9.0f));
        g.drawText (juce::String (reduction, 1) + " dB", valueArea,
                    juce::Justification::centredRight);
    };

    drawGr (grRow, "GR", compGr, juce::Colour (0xffffb15c));
    drawGr (area.removeFromTop (18), "LIM", limiterGr, VocalRackLookAndFeel::danger());
}

PitchPanel::PitchPanel (Engine& engineIn) : engine (engineIn)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void PitchPanel::timerCallback()
{
    status = engine.getPitchStatus();
    repaint();
}

void PitchPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (8, 6);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.5f, true));
    g.drawText ("PITCH", area.removeFromTop (14), juce::Justification::centredLeft);

    if (! status.active)
    {
        g.setColour (VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (15.0f));
        g.drawText ("BYPASSED", area, juce::Justification::centredLeft);
        return;
    }

    auto row = area.removeFromTop (44);

    const juce::String detected = status.voiced && status.detectedMidi > 0.0f
                                      ? midiNoteName ((int) std::lround (status.detectedMidi))
                                      : "--";
    const juce::String target = status.targetMidi >= 0.0f
                                    ? midiNoteName ((int) std::lround (status.targetMidi))
                                    : "--";

    g.setColour (VocalRackLookAndFeel::text());
    g.setFont (VocalRackLookAndFeel::font (30.0f, true));
    g.drawText (detected, row.removeFromLeft (96), juce::Justification::centredLeft);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (14.0f));
    g.drawText ("->", row.removeFromLeft (30), juce::Justification::centred);

    g.setColour (status.voiced ? VocalRackLookAndFeel::accent()
                               : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (30.0f, true));
    g.drawText (target, row, juce::Justification::centredLeft);

    auto centsRow = area.removeFromTop (20);
    g.setColour (status.voiced ? VocalRackLookAndFeel::text()
                               : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (12.0f));
    g.drawText (status.voiced ? juce::String (status.cents, 1) + " cents"
                              : "listening…",
                centsRow.removeFromLeft (140), juce::Justification::centredLeft);

    g.setColour (status.voiced ? VocalRackLookAndFeel::accent()
                               : VocalRackLookAndFeel::dimText());
    g.fillEllipse ((float) centsRow.getX() + 2.0f, (float) centsRow.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.5f, true));
    g.drawText (status.voiced ? "VOICED" : "UNVOICED", centsRow.withTrimmedLeft (16),
                juce::Justification::centredLeft);

    auto confidenceRow = area.removeFromTop (12);
    constexpr int confidenceTextWidth = 70;
    auto confidenceBar = confidenceRow.withTrimmedRight (confidenceTextWidth);
    g.setColour (juce::Colour (0xff2b2f36));
    g.fillRoundedRectangle (confidenceBar.toFloat(), 3.0f);
    g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.8f));
    g.fillRoundedRectangle (confidenceBar.toFloat().withWidth (
                                confidenceBar.getWidth()
                                    * juce::jlimit (0.0f, 1.0f, status.confidence)),
                            3.0f);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (8.5f));
    g.drawText ("conf " + juce::String (juce::roundToInt (status.confidence * 100.0f)) + "%",
                confidenceRow.removeFromRight (confidenceTextWidth),
                juce::Justification::centredRight);
}

NoteKeyboard::NoteKeyboard (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int pc = 0; pc < 12; ++pc)
    {
        auto button = std::make_unique<juce::TextButton> (names[pc]);
        button->setClickingTogglesState (false);
        button->setWantsKeyboardFocus (true);
        button->setTitle ("Note " + juce::String (names[pc]));
        button->setDescription ("Enable or disable pitch class " + juce::String (names[pc]));
        button->setColour (juce::TextButton::buttonOnColourId, VocalRackLookAndFeel::accent());
        button->setColour (juce::TextButton::textColourOnId, VocalRackLookAndFeel::background());
        button->onClick = [this, pc]
        {
            if (onToggle)
                onToggle (pc);
        };
        addAndMakeVisible (*button);
        keys[(size_t) pc] = std::move (button);
    }

    setTitle ("Note mask");
    setDescription ("12-note pitch-class mask. Editing a key switches to the Custom scale.");
    startTimerHz (12);
}

void NoteKeyboard::resized()
{
    auto area = getLocalBounds();
    const int width = area.getWidth() / 12;

    for (int pc = 0; pc < 12; ++pc)
    {
        auto keyArea = area.removeFromLeft (pc == 11 ? area.getWidth() : width).reduced (1);
        keys[(size_t) pc]->setBounds (keyArea);
    }
}

int NoteKeyboard::currentTonic() const
{
    return (int) std::lround (apvts.getRawParameterValue ("pitch.tonic")->load());
}

int NoteKeyboard::currentScale() const
{
    return (int) std::lround (apvts.getRawParameterValue ("pitch.scale")->load());
}

int NoteKeyboard::effectiveMask() const
{
    const int scale = currentScale();
    const int mask = (int) std::lround (apvts.getRawParameterValue ("pitch.mask")->load());
    return scale == 3 ? mask : maskForScale (scale, currentTonic());
}

void NoteKeyboard::timerCallback()
{
    const int mask = effectiveMask();
    const int tonic = currentTonic();

    if (mask == lastMask && tonic == lastTonic)
        return;

    lastMask = mask;
    lastTonic = tonic;

    for (int pc = 0; pc < 12; ++pc)
    {
        const bool enabled = (mask & (1 << pc)) != 0;
        const bool isTonic = pc == tonic;
        auto& button = *keys[(size_t) pc];

        button.setToggleState (enabled, juce::dontSendNotification);
        button.setColour (juce::TextButton::buttonColourId,
                          enabled ? VocalRackLookAndFeel::accent().withAlpha (isTonic ? 1.0f : 0.74f)
                                  : VocalRackLookAndFeel::panel());
        button.setColour (juce::TextButton::textColourOffId,
                          enabled ? VocalRackLookAndFeel::background()
                                  : VocalRackLookAndFeel::dimText());
        button.repaint();
    }
}

void NoteKeyboard::paint (juce::Graphics& g)
{
    g.setColour (VocalRackLookAndFeel::panel().withAlpha (0.6f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

PresetBrowser::PresetBrowser (VocalRackAudioProcessor& processorIn,
                              std::function<void()> changedCallback)
    : processor (processorIn), onChanged (std::move (changedCallback))
{
    setTitle ("Preset browser");
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);

    titleLabel.setText ("PRESET BROWSER", juce::dontSendNotification);
    titleLabel.setFont (VocalRackLookAndFeel::font (14.0f, true));
    titleLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::accent());
    addAndMakeVisible (titleLabel);

    bankLabel.setText ("BANK", juce::dontSendNotification);
    bankLabel.setFont (VocalRackLookAndFeel::font (9.5f, true));
    bankLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    addAndMakeVisible (bankLabel);

    searchLabel.setText ("SEARCH", juce::dontSendNotification);
    searchLabel.setFont (VocalRackLookAndFeel::font (9.5f, true));
    searchLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    addAndMakeVisible (searchLabel);

    searchEditor.setTextToShowWhenEmpty ("Search name, bank, notes", VocalRackLookAndFeel::dimText());
    searchEditor.onTextChange = [this] { rebuildVisibleList(); };
    searchEditor.setTitle ("Search presets");
    searchEditor.setWantsKeyboardFocus (true);
    addAndMakeVisible (searchEditor);

    bankBox.setTextWhenNothingSelected ("All banks");
    bankBox.onChange = [this] { rebuildVisibleList(); };
    bankBox.setTitle ("Preset bank filter");
    addAndMakeVisible (bankBox);

    listBox.setModel (this);
    listBox.setRowHeight (46);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1d21));
    listBox.setTitle ("Presets");
    addAndMakeVisible (listBox);

    detailsLabel.setJustificationType (juce::Justification::topLeft);
    detailsLabel.setMinimumHorizontalScale (1.0f);
    detailsLabel.setFont (VocalRackLookAndFeel::font (12.0f));
    detailsLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    addAndMakeVisible (detailsLabel);

    auto setupButton = [this] (juce::TextButton& button)
    {
        addAndMakeVisible (button);
        button.setWantsKeyboardFocus (true);
    };

    setupButton (loadButton);
    setupButton (saveButton);
    setupButton (duplicateButton);
    setupButton (renameButton);
    setupButton (deleteButton);
    setupButton (importButton);
    setupButton (exportButton);
    setupButton (closeButton);

    loadButton.onClick = [this] { loadSelected(); };
    saveButton.onClick = [this] { saveAs(); };
    duplicateButton.onClick = [this] { duplicateSelected(); };
    renameButton.onClick = [this] { renameSelected(); };
    deleteButton.onClick = [this] { deleteSelected(); };
    importButton.onClick = [this] { importPreset(); };
    exportButton.onClick = [this] { exportSelected(); };
    closeButton.onClick = [this] { setVisible (false); };

    setVisible (false);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::reloadBanks()
{
    const auto current = bankBox.getText();
    bankBox.clear (juce::dontSendNotification);
    bankBox.addItem ("All banks", 1);

    int itemId = 2;

    for (const auto& bank : processor.getPresets().getBanks())
        bankBox.addItem (bank, itemId++);

    if (current.isNotEmpty())
        bankBox.setText (current, juce::dontSendNotification);
    else
        bankBox.setSelectedId (1, juce::dontSendNotification);
}

void PresetBrowser::refresh()
{
    reloadBanks();
    rebuildVisibleList();
    updateDetails();
}

void PresetBrowser::showForCurrentPreset()
{
    refresh();
    setVisible (true);
    toFront (true);
}

void PresetBrowser::rebuildVisibleList()
{
    const auto bank = bankBox.getText().isEmpty() ? juce::String ("All banks") : bankBox.getText();
    visibleEntries = processor.getPresets().filteredIndices (bank, searchEditor.getText());
    listBox.updateContent();

    const auto current = processor.getPresetId();
    const auto it = std::find_if (visibleEntries.begin(), visibleEntries.end(),
                                  [&] (int index)
                                  {
                                      const auto& entries = processor.getPresets().getEntries();
                                      return index >= 0 && index < (int) entries.size()
                                             && entries[(size_t) index].id == current;
                                  });

    if (it != visibleEntries.end())
        listBox.selectRow ((int) std::distance (visibleEntries.begin(), it));
    else
        listBox.deselectAllRows();

    updateDetails();
}

int PresetBrowser::getNumRows()
{
    return (int) visibleEntries.size();
}

void PresetBrowser::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                                      bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) visibleEntries.size())
        return;

    const auto& entries = processor.getPresets().getEntries();
    const int index = visibleEntries[(size_t) rowNumber];

    if (index < 0 || index >= (int) entries.size())
        return;

    const auto& entry = entries[(size_t) index];
    auto area = juce::Rectangle<int> (0, 0, width, height);

    if (rowIsSelected)
    {
        g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.14f));
        g.fillRect (area);
        g.setColour (VocalRackLookAndFeel::accent());
        g.fillRect (area.removeFromLeft (2));
    }

    g.setColour (rowIsSelected ? VocalRackLookAndFeel::text()
                               : VocalRackLookAndFeel::text().withAlpha (0.9f));
    g.setFont (VocalRackLookAndFeel::font (13.0f, rowIsSelected));
    g.drawText (entry.name, area.reduced (12, 4).removeFromTop (20),
                juce::Justification::centredLeft);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.5f));
    g.drawText ((entry.factory ? "FACTORY  ·  " : "USER  ·  ") + entry.bank,
                area.reduced (12, 4).withTrimmedTop (21),
                juce::Justification::centredLeft);
}

void PresetBrowser::selectedRowsChanged (int)
{
    updateDetails();
}

const PresetEntry* PresetBrowser::selectedEntry() const
{
    const int row = listBox.getSelectedRow();

    if (row < 0 || row >= (int) visibleEntries.size())
        return nullptr;

    const auto& entries = processor.getPresets().getEntries();
    const int index = visibleEntries[(size_t) row];

    if (index < 0 || index >= (int) entries.size())
        return nullptr;

    return &entries[(size_t) index];
}

void PresetBrowser::updateDetails()
{
    const auto* entry = selectedEntry();
    const bool userPreset = entry != nullptr && ! entry->factory;

    renameButton.setEnabled (userPreset);
    deleteButton.setEnabled (userPreset);
    duplicateButton.setEnabled (entry != nullptr);
    exportButton.setEnabled (entry != nullptr);
    loadButton.setEnabled (entry != nullptr);

    if (entry == nullptr)
    {
        detailsLabel.setText ("Select a preset to see its description, use case and first move.",
                              juce::dontSendNotification);
        return;
    }

    juce::String text;
    text << entry->description << "\n\n"
         << "USE  " << entry->useCase << "\n\n"
         << "FIRST MOVE  " << entry->firstAdjustment;

    detailsLabel.setText (text, juce::dontSendNotification);
}

void PresetBrowser::loadSelected()
{
    if (const auto* entry = selectedEntry())
    {
        processor.requestPresetLoad (entry->id, processor.getKeepSongKey());

        if (onChanged)
            onChanged();

        updateDetails();
    }
}

void PresetBrowser::showPresetSaveDialog()
{
    const auto* selected = selectedEntry();
    const auto defaultName = selected != nullptr ? selected->name + " Edit" : "My Vocal";
    const auto defaultBank = selected != nullptr && selected->bank != "All banks"
                                 ? selected->bank : processor.getPresetBank();

    auto* alert = new juce::AlertWindow ("Save user preset",
                                         "Stored atomically in the ademusic user preset folder.",
                                         juce::MessageBoxIconType::InfoIcon, this);
    alert->addTextEditor ("name", defaultName, "Name");
    alert->addTextEditor ("bank", defaultBank.isEmpty() ? "User" : defaultBank, "Bank");
    alert->addTextEditor ("description", selected != nullptr ? selected->description : juce::String(), "Description");
    alert->addTextEditor ("useCase", selected != nullptr ? selected->useCase : juce::String(), "Use case");
    alert->addTextEditor ("firstAdjustment", selected != nullptr ? selected->firstAdjustment : juce::String(),
                          "First adjustment");
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    alert->setSize (460, 360);

    alert->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, alert] (int result)
        {
            if (result == 1)
            {
                juce::String error;

                if (! processor.saveCurrentAsUserPreset (alert->getTextEditorContents ("name"),
                                                         alert->getTextEditorContents ("bank"),
                                                         alert->getTextEditorContents ("description"),
                                                         alert->getTextEditorContents ("useCase"),
                                                         alert->getTextEditorContents ("firstAdjustment"),
                                                         error))
                {
                    showError (error);
                }
                else
                {
                    refresh();

                    if (onChanged)
                        onChanged();
                }
            }

            delete alert;
        }), false);
}

void PresetBrowser::saveAs()
{
    showPresetSaveDialog();
}

void PresetBrowser::showNameDialog (const juce::String& title, const juce::String& initial,
                                    std::function<void (const juce::String&)> callback)
{
    auto* alert = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::QuestionIcon, this);
    alert->addTextEditor ("name", initial, "Name");
    alert->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    alert->setSize (400, 170);

    alert->enterModalState (true, juce::ModalCallbackFunction::create (
        [alert, callback = std::move (callback)] (int result)
        {
            if (result == 1)
                callback (alert->getTextEditorContents ("name"));

            delete alert;
        }), false);
}

void PresetBrowser::duplicateSelected()
{
    if (const auto* entry = selectedEntry())
    {
        const auto id = entry->id;
        showNameDialog ("Duplicate preset", entry->name + " Copy",
                        [this, id] (const juce::String& name)
                        {
                            juce::String error;

                            if (! processor.duplicateUserPreset (id, name, error))
                                showError (error);
                            else
                            {
                                refresh();

                                if (onChanged)
                                    onChanged();
                            }
                        });
    }
}

void PresetBrowser::renameSelected()
{
    if (const auto* entry = selectedEntry())
    {
        const auto id = entry->id;
        showNameDialog ("Rename user preset", entry->name,
                        [this, id] (const juce::String& name)
                        {
                            juce::String error;

                            if (! processor.renameUserPreset (id, name, error))
                                showError (error);
                            else
                            {
                                refresh();

                                if (onChanged)
                                    onChanged();
                            }
                        });
    }
}

void PresetBrowser::showConfirm (const juce::String& title, const juce::String& message,
                                 std::function<void()> callback)
{
    auto* alert = new juce::AlertWindow (title, message, juce::MessageBoxIconType::WarningIcon, this);
    alert->addButton ("Delete", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    alert->setSize (400, 180);

    alert->enterModalState (true, juce::ModalCallbackFunction::create (
        [alert, callback = std::move (callback)] (int result)
        {
            if (result == 1)
                callback();

            delete alert;
        }), false);
}

void PresetBrowser::deleteSelected()
{
    if (const auto* entry = selectedEntry())
    {
        if (entry->factory)
            return;

        const auto id = entry->id;
        showConfirm ("Delete preset", "Delete \"" + entry->name + "\" permanently?",
                     [this, id]
                     {
                         juce::String error;

                         if (! processor.deleteUserPreset (id, error))
                             showError (error);
                         else
                         {
                             refresh();

                             if (onChanged)
                                 onChanged();
                         }
                     });
    }
}

void PresetBrowser::importPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import Vocal Rack preset",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.vocalrack.json");

    juce::Component::SafePointer<PresetBrowser> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& chooser)
                              {
                                  if (safe == nullptr)
                                      return;

                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                  {
                                      juce::String error;

                                      if (! safe->processor.importPresetFile (file, error))
                                          safe->showError (error);
                                      else
                                      {
                                          safe->refresh();

                                          if (safe->onChanged)
                                              safe->onChanged();
                                      }
                                  }
                              });
}

void PresetBrowser::exportSelected()
{
    const auto* entry = selectedEntry();

    if (entry == nullptr)
        return;

    const auto id = entry->id;
    const auto suggested = PresetManager::slugify (entry->name) + ".vocalrack.json";
    fileChooser = std::make_unique<juce::FileChooser> ("Export Vocal Rack preset",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile (suggested),
                                                       "*.vocalrack.json");

    juce::Component::SafePointer<PresetBrowser> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe, id] (const juce::FileChooser& chooser)
                              {
                                  if (safe == nullptr)
                                      return;

                                  auto file = chooser.getResult();

                                  if (file == juce::File())
                                      return;

                                  if (! file.hasFileExtension ("vocalrack.json"))
                                      file = file.withFileExtension ("vocalrack.json");

                                  juce::String error;

                                  if (! safe->processor.exportPresetFile (id, file, error))
                                      safe->showError (error);
                              });
}

void PresetBrowser::showError (const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                            "Preset", message, "OK", this);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (VocalRackLookAndFeel::background().withAlpha (0.97f));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.2f);

    g.setColour (VocalRackLookAndFeel::panel().withAlpha (0.7f));
    g.fillRoundedRectangle (20.0f, 78.0f, (float) getWidth() - 40.0f, 286.0f, 5.0f);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto titleRow = area.removeFromTop (26);
    titleLabel.setBounds (titleRow.removeFromLeft (220));
    closeButton.setBounds (titleRow.removeFromRight (64).reduced (0, 1));

    area.removeFromTop (6);
    auto filterRow = area.removeFromTop (26);
    bankLabel.setBounds (filterRow.removeFromLeft (34));
    bankBox.setBounds (filterRow.removeFromLeft (190));
    filterRow.removeFromLeft (10);
    searchLabel.setBounds (filterRow.removeFromLeft (48));
    searchEditor.setBounds (filterRow.removeFromLeft (260));
    area.removeFromTop (8);

    auto browserArea = area.removeFromTop (286);
    listBox.setBounds (browserArea.removeFromLeft (400));
    browserArea.removeFromLeft (12);
    detailsLabel.setBounds (browserArea.reduced (6));

    area.removeFromTop (10);
    auto buttonsRow1 = area.removeFromTop (28);
    loadButton.setBounds (buttonsRow1.removeFromLeft (72).reduced (1));
    saveButton.setBounds (buttonsRow1.removeFromLeft (78).reduced (1));
    duplicateButton.setBounds (buttonsRow1.removeFromLeft (84).reduced (1));
    renameButton.setBounds (buttonsRow1.removeFromLeft (78).reduced (1));
    deleteButton.setBounds (buttonsRow1.removeFromLeft (70).reduced (1));
    buttonsRow1.removeFromLeft (8);
    importButton.setBounds (buttonsRow1.removeFromLeft (70).reduced (1));
    exportButton.setBounds (buttonsRow1.removeFromLeft (70).reduced (1));
}

} // namespace vr
