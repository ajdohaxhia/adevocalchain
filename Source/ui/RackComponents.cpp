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

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
            const juce::String& displayName)
{
    spec = findParamSpec (parameterId);
    jassert (spec != nullptr);

    if (spec == nullptr)
        return;

    nameLabel.setText (displayName.isNotEmpty() ? displayName : juce::String (spec->name),
                       juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    nameLabel.setFont (VocalRackLookAndFeel::font (12.5f));
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    subLabel.setJustificationType (juce::Justification::centred);
    subLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    subLabel.setFont (VocalRackLookAndFeel::font (10.0f));
    subLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (subLabel);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 22);
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
    slider.setColour (juce::Slider::textBoxTextColourId, VocalRackLookAndFeel::text());
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
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
    nameLabel.setBounds (area.removeFromTop (22));

    if (subLabel.isVisible())
    {
        subLabel.setBounds (area.removeFromBottom (18));
        area.removeFromBottom (2);
    }

    area.removeFromTop (2);
    slider.setBounds (area);
}

void Knob::setSubLabel (const juce::String& text)
{
    subLabel.setText (text, juce::dontSendNotification);
    subLabel.setVisible (text.isNotEmpty());
    resized();
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
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = parameter != nullptr && parameter->getValue() < 0.5f;
    auto fill = on ? VocalRackLookAndFeel::panelLight()
                   : VocalRackLookAndFeel::panel();

    if (isButtonDown)
        fill = fill.brighter (0.07f);
    else if (isMouseOver)
        fill = fill.brighter (0.035f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (on ? VocalRackLookAndFeel::accent().withAlpha (0.75f)
                    : VocalRackLookAndFeel::line());
    g.drawRoundedRectangle (bounds, 7.0f, on ? 1.2f : 1.0f);

    const auto centre = juce::Point<float> (bounds.getX() + 15.0f, bounds.getCentreY());
    juce::Path power;
    power.addCentredArc (centre.x, centre.y, 5.0f, 5.0f, 0.0f,
                         juce::MathConstants<float>::pi * 0.75f,
                         juce::MathConstants<float>::pi * 2.25f, true);
    g.setColour (on ? VocalRackLookAndFeel::accent() : VocalRackLookAndFeel::dimText());
    g.strokePath (power, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.drawLine (centre.x, centre.y - 7.0f, centre.x, centre.y + 1.0f, 1.5f);

    g.setColour (on ? VocalRackLookAndFeel::text() : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (11.0f, on));
    g.drawFittedText (label, bounds.toNearestInt().withTrimmedLeft (28).withTrimmedRight (7),
                      juce::Justification::centredLeft, 1);

    if (hasKeyboardFocus (true))
    {
        g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 7.0f, 1.0f);
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
    auto area = getLocalBounds().reduced (14, 10);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.0f, true));
    g.drawText ("LEVELS", area.removeFromTop (16), juce::Justification::centredLeft);

    auto drawMeter = [&g] (juce::Rectangle<int> row, const juce::String& label, float db,
                           float peakDb, juce::Colour colour, bool hold)
    {
        auto labelArea = row.removeFromLeft (34);
        auto valueArea = row.removeFromRight (52);
        auto bar = row.reduced (2, 6);

        g.setColour (VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (10.0f, true));
        g.drawText (label, labelArea, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff2b302b));
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);

        const float position = meterPosition (db);
        auto fill = bar.toFloat().withWidth (bar.getWidth() * position);
        g.setColour (colour);
        g.fillRoundedRectangle (fill, 2.0f);

        if (hold && peakDb > kMinDb)
        {
            const int x = bar.getX() + (int) (bar.getWidth() * meterPosition (peakDb));
            g.setColour (VocalRackLookAndFeel::text().withAlpha (0.7f));
            g.fillRect (x, bar.getY(), 1, bar.getHeight());
        }

        g.setColour (db <= kMinDb + 0.5f ? VocalRackLookAndFeel::dimText()
                                         : VocalRackLookAndFeel::text());
        g.setFont (VocalRackLookAndFeel::font (10.0f));
        g.drawText (db <= kMinDb + 0.5f ? "-inf" : juce::String (db, 1) + " dB",
                    valueArea, juce::Justification::centredRight);
    };

    drawMeter (area.removeFromTop (27), "IN", inputDb, -60.0f,
               VocalRackLookAndFeel::accent().withAlpha (0.68f), false);
    drawMeter (area.removeFromTop (27), "OUT", outputDb, peakHoldDb,
               VocalRackLookAndFeel::accent(), true);

    area.removeFromTop (6);

    auto drawGr = [&g] (juce::Rectangle<int> row, const juce::String& label, float reduction,
                        juce::Colour colour)
    {
        auto labelArea = row.removeFromLeft (34);
        auto valueArea = row.removeFromRight (52);
        auto bar = row.reduced (2, 4);

        g.setColour (VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (9.0f, true));
        g.drawText (label, labelArea, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff2b302b));
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

    drawGr (area.removeFromTop (19), "GR", compGr, juce::Colour (0xffffb15c));
    drawGr (area.removeFromTop (19), "LIM", limiterGr, VocalRackLookAndFeel::danger());
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
    auto area = getLocalBounds().reduced (14, 10);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (9.0f, true));
    g.drawText ("PITCH", area.removeFromTop (14), juce::Justification::centredLeft);

    if (! status.active)
    {
        g.setColour (VocalRackLookAndFeel::dimText());
        g.setFont (VocalRackLookAndFeel::font (15.0f));
        g.drawText ("BYPASSED", area, juce::Justification::centredLeft);
        return;
    }

    const auto detectedName = status.voiced && status.detectedMidi > 0.0f
                                  ? midiNoteName ((int) std::lround (status.detectedMidi))
                                  : "--";
    const auto targetName = status.targetMidi >= 0.0f
                                ? midiNoteName ((int) std::lround (status.targetMidi))
                                : "--";
    const auto hz = status.detectedMidi > 0.0f
                        ? 440.0f * std::pow (2.0f, (status.detectedMidi - 69.0f) / 12.0f)
                        : 0.0f;

    auto main = area.removeFromTop (48);
    auto noteArea = main.removeFromLeft (82);
    g.setColour (status.voiced ? VocalRackLookAndFeel::text()
                               : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (38.0f, true));
    g.drawText (detectedName, noteArea, juce::Justification::centredLeft);

    auto hzArea = main.removeFromLeft (86);
    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (11.5f));
    g.drawText (hz > 0.0f ? juce::String (hz, 1) + " Hz" : "-- Hz",
                hzArea, juce::Justification::centredLeft);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (13.0f));
    g.drawText ("-> " + targetName, main, juce::Justification::centredRight);

    auto centsRow = area.removeFromTop (22);
    g.setColour (status.voiced ? VocalRackLookAndFeel::text()
                               : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (15.0f));
    g.drawText (status.voiced ? juce::String (status.cents, 1) + " cents"
                              : "listening...",
                centsRow.removeFromLeft (135), juce::Justification::centredLeft);

    g.setColour (status.voiced ? VocalRackLookAndFeel::accent()
                               : VocalRackLookAndFeel::dimText());
    g.fillEllipse ((float) centsRow.getX() + 2.0f, (float) centsRow.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setFont (VocalRackLookAndFeel::font (9.0f, true));
    g.setColour (VocalRackLookAndFeel::dimText());
    g.drawText (status.voiced ? "VOICED" : "UNVOICED", centsRow.withTrimmedLeft (16),
                juce::Justification::centredLeft);

    auto confidenceRow = area.removeFromTop (14);
    constexpr int confidenceTextWidth = 70;
    auto confidenceBar = confidenceRow.withTrimmedRight (confidenceTextWidth);
    g.setColour (juce::Colour (0xff2b302b));
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

void BankListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                      bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.setColour (VocalRackLookAndFeel::accent());
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 2.0f, (float) width, (float) height - 4.0f), 6.0f);
    }

    g.setColour (rowIsSelected ? VocalRackLookAndFeel::background()
                               : VocalRackLookAndFeel::text());
    g.setFont (VocalRackLookAndFeel::font (12.5f, rowIsSelected));
    g.drawFittedText (getRowName ? getRowName (row) : juce::String(),
                      juce::Rectangle<int> (12, 0, width - 28, height),
                      juce::Justification::centredLeft, 1);

    if (! rowIsSelected)
    {
        g.setColour (VocalRackLookAndFeel::dimText().withAlpha (0.55f));
        g.setFont (VocalRackLookAndFeel::font (12.0f));
        g.drawText (">", juce::Rectangle<int> (width - 22, 0, 14, height),
                    juce::Justification::centredRight);
    }
}

void BankListModel::selectedRowsChanged (int lastRowSelected)
{
    if (onRowSelected)
        onRowSelected (lastRowSelected);
}

PresetBrowser::PresetBrowser (VocalRackAudioProcessor& processorIn,
                              std::function<void()> changedCallback)
    : processor (processorIn), onChanged (std::move (changedCallback))
{
    setTitle ("Preset browser");
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);

    titleLabel.setText ("Presets", juce::dontSendNotification);
    titleLabel.setFont (VocalRackLookAndFeel::font (23.0f, true));
    titleLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    addAndMakeVisible (titleLabel);

    countLabel.setFont (VocalRackLookAndFeel::font (11.0f));
    countLabel.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    addAndMakeVisible (countLabel);

    searchEditor.setTextToShowWhenEmpty ("Search presets...", VocalRackLookAndFeel::dimText());
    searchEditor.setBorder (juce::BorderSize<int> (0, 26, 0, 0));
    searchEditor.onTextChange = [this] { rebuildVisibleList(); };
    searchEditor.setTitle ("Search presets");
    searchEditor.setWantsKeyboardFocus (true);
    addAndMakeVisible (searchEditor);

    clearSearchButton.setTitle ("Clear search");
    clearSearchButton.setTooltip ("Clear the preset search");
    clearSearchButton.onClick = [this] { searchEditor.clear(); rebuildVisibleList(); };
    addAndMakeVisible (clearSearchButton);

    bankLabel.setVisible (false);
    bankBox.setVisible (false);
    searchLabel.setVisible (false);
    addChildComponent (bankLabel);
    addChildComponent (bankBox);
    addChildComponent (searchLabel);

    bankList.setModel (&bankModel);
    bankList.setRowHeight (36);
    bankList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    bankList.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    bankList.setTitle ("Preset banks");
    addAndMakeVisible (bankList);

    bankModel.getRowCount = [this] { return (int) banks.size(); };
    bankModel.getRowName = [this] (int row)
    {
        return row >= 0 && row < (int) banks.size() ? banks[(size_t) row] : juce::String();
    };
    bankModel.getSelectedRow = [this] { return bankList.getSelectedRow(); };
    bankModel.onRowSelected = [this] (int row) { selectBank (row); };

    factoryButton.setClickingTogglesState (false);
    userButton.setClickingTogglesState (false);
    factoryButton.setColour (juce::TextButton::buttonColourId, VocalRackLookAndFeel::accent());
    factoryButton.setColour (juce::TextButton::textColourOffId, VocalRackLookAndFeel::background());
    userButton.setColour (juce::TextButton::buttonColourId, VocalRackLookAndFeel::panelLight());
    userButton.setColour (juce::TextButton::textColourOffId, VocalRackLookAndFeel::text());
    factoryButton.onClick = [this]
    {
        userOnly = false;
        factoryButton.setColour (juce::TextButton::buttonColourId, VocalRackLookAndFeel::accent());
        factoryButton.setColour (juce::TextButton::textColourOffId, VocalRackLookAndFeel::background());
        userButton.setColour (juce::TextButton::buttonColourId, VocalRackLookAndFeel::panelLight());
        userButton.setColour (juce::TextButton::textColourOffId, VocalRackLookAndFeel::text());
        rebuildVisibleList();
    };
    userButton.onClick = [this]
    {
        userOnly = true;
        userButton.setColour (juce::TextButton::buttonColourId, VocalRackLookAndFeel::accent());
        userButton.setColour (juce::TextButton::textColourOffId, VocalRackLookAndFeel::background());
        factoryButton.setColour (juce::TextButton::buttonColourId, VocalRackLookAndFeel::panelLight());
        factoryButton.setColour (juce::TextButton::textColourOffId, VocalRackLookAndFeel::text());
        rebuildVisibleList();
    };
    addAndMakeVisible (factoryButton);
    addAndMakeVisible (userButton);

    listBox.setModel (this);
    listBox.setRowHeight (50);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    listBox.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    listBox.setTitle ("Presets");
    addAndMakeVisible (listBox);

    auto setupHeader = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (VocalRackLookAndFeel::font (8.5f, true));
        label.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    };

    setupHeader (descriptionHeader, "DESCRIPTION");
    setupHeader (useCaseHeader, "USE CASE");
    setupHeader (firstHeader, "FIRST MOVE");

    detailBadge.setFont (VocalRackLookAndFeel::font (8.0f, true));
    detailBadge.setColour (juce::Label::textColourId, VocalRackLookAndFeel::accent());
    detailBadge.setJustificationType (juce::Justification::centred);
    detailBadge.setColour (juce::Label::backgroundColourId, VocalRackLookAndFeel::accentSoft().withAlpha (0.22f));
    addAndMakeVisible (detailBadge);

    detailName.setFont (VocalRackLookAndFeel::font (21.0f, true));
    detailName.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text());
    detailName.setJustificationType (juce::Justification::centredLeft);
    detailName.setMinimumHorizontalScale (0.65f);
    addAndMakeVisible (detailName);

    detailBank.setFont (VocalRackLookAndFeel::font (10.0f));
    detailBank.setColour (juce::Label::textColourId, VocalRackLookAndFeel::dimText());
    detailBank.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (detailBank);

    auto setupBody = [this] (juce::Label& label, float size)
    {
        label.setFont (VocalRackLookAndFeel::font (size));
        label.setColour (juce::Label::textColourId, VocalRackLookAndFeel::text().withAlpha (0.88f));
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (label);
    };

    setupBody (descriptionLabel, 12.0f);
    setupBody (useCaseLabel, 11.5f);
    setupBody (firstLabel, 11.5f);

    browserKeepKey.setButtonText ("Keep Song Key");
    browserKeepKey.setTitle ("Keep Song Key");
    browserKeepKey.setTooltip ("Factory browsing preserves the current tonic, scale and note mask.");
    browserKeepKey.onClick = [this]
    {
        processor.setKeepSongKey (browserKeepKey.getToggleState());
        if (onChanged)
            onChanged();
    };
    addAndMakeVisible (browserKeepKey);

    detailsLabel.setVisible (false);
    addChildComponent (detailsLabel);

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

    loadButton.setButtonText ("Load preset");
    closeButton.setButtonText ("Close");
    closeButton.setTooltip ("Close preset browser");

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
    const auto previous = selectedBank >= 0 && selectedBank < (int) banks.size()
                              ? banks[(size_t) selectedBank] : juce::String ("All Presets");

    banks.clear();
    banks.push_back ("All Presets");

    for (const auto& bank : processor.getPresets().getBanks())
        banks.push_back (bank);

    selectedBank = 0;

    for (int i = 0; i < (int) banks.size(); ++i)
    {
        if (banks[(size_t) i] == previous)
            selectedBank = i;
    }

    bankList.updateContent();
    bankList.selectRow (selectedBank, juce::dontSendNotification);
}

void PresetBrowser::refresh()
{
    reloadBanks();
    rebuildVisibleList();
    browserKeepKey.setToggleState (processor.getKeepSongKey(), juce::dontSendNotification);
    countLabel.setText (juce::String (processor.getPresets().getEntries().size())
                            + " factory presets",
                        juce::dontSendNotification);
}

void PresetBrowser::showForCurrentPreset()
{
    refresh();
    setVisible (true);
    toFront (true);
    searchEditor.grabKeyboardFocus();
}

void PresetBrowser::selectBank (int row)
{
    if (row < 0 || row >= (int) banks.size() || row == selectedBank)
        return;

    selectedBank = row;
    rebuildVisibleList();
}

void PresetBrowser::rebuildVisibleList()
{
    const auto bank = selectedBank == 0 ? juce::String ("All banks")
                                        : banks[(size_t) selectedBank];
    visibleEntries = processor.getPresets().filteredIndices (bank, searchEditor.getText());

    if (userOnly)
    {
        const auto& entries = processor.getPresets().getEntries();
        visibleEntries.erase (std::remove_if (visibleEntries.begin(), visibleEntries.end(),
                                              [&entries] (int index)
                                              {
                                                  return index < 0 || index >= (int) entries.size()
                                                         || entries[(size_t) index].factory;
                                              }),
                              visibleEntries.end());
    }

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

void PresetBrowser::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (VocalRackLookAndFeel::background());
    g.setColour (VocalRackLookAndFeel::background());
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (VocalRackLookAndFeel::accent().withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.2f);

    auto drawPanel = [&g] (juce::Rectangle<int> panel)
    {
        g.setColour (VocalRackLookAndFeel::card());
        g.fillRoundedRectangle (panel.toFloat(), 9.0f);
        g.setColour (VocalRackLookAndFeel::line());
        g.drawRoundedRectangle (panel.toFloat(), 9.0f, 1.0f);
    };

    drawPanel (leftPanel);
    drawPanel (centerPanel);
    drawPanel (rightPanel);

    auto lens = searchEditor.getBounds().toFloat();
    const auto centre = juce::Point<float> (lens.getX() + 14.0f, lens.getCentreY() - 1.0f);
    g.setColour (VocalRackLookAndFeel::dimText());
    g.drawEllipse (centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 1.4f);
    g.drawLine (centre.x + 3.8f, centre.y + 3.8f, centre.x + 8.0f, centre.y + 8.0f, 1.4f);

    auto tableHeader = juce::Rectangle<int> (listBox.getX() + 15, listBox.getY() - 22,
                                             listBox.getWidth() - 30, 20);
    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (8.5f, true));
    g.drawText ("PRESET", tableHeader.removeFromLeft (juce::jmax (120, tableHeader.getWidth() * 3 / 5)),
                juce::Justification::centredLeft);
    g.drawText ("BANK", tableHeader, juce::Justification::centredLeft);

    g.setColour (VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (8.5f, true));
    g.drawText ("BANKS", leftPanel.getX() + 14, leftPanel.getY() + 13, 120, 18,
                juce::Justification::centredLeft);

    g.setColour (VocalRackLookAndFeel::line());
    g.fillRect (rightPanel.getX() + 22, detailName.getBottom() + 12, rightPanel.getWidth() - 44, 1);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced (24);
    auto titleRow = area.removeFromTop (48);
    titleLabel.setBounds (titleRow.removeFromLeft (170));
    titleRow.removeFromLeft (8);
    countLabel.setBounds (titleRow.removeFromLeft (190).reduced (0, 8));

    auto searchArea = titleRow.removeFromRight (390);
    closeButton.setBounds (searchArea.removeFromRight (52).reduced (0, 6));
    searchArea.removeFromRight (8);
    clearSearchButton.setBounds (searchArea.removeFromRight (30).reduced (0, 6));
    searchArea.removeFromRight (4);
    searchEditor.setBounds (searchArea.reduced (0, 4));

    area.removeFromTop (12);
    auto content = area.removeFromTop (area.getHeight() - 12);

    const int leftWidth = juce::jlimit (190, 250, content.getWidth() / 4);
    const int rightWidth = juce::jlimit (300, 380, content.getWidth() / 3);
    leftPanel = content.removeFromLeft (leftWidth);
    content.removeFromLeft (14);
    rightPanel = content.removeFromRight (rightWidth);
    content.removeFromRight (14);
    centerPanel = content;

    auto leftInner = leftPanel.reduced (12);
    leftInner.removeFromTop (24);
    auto bankButtons = leftInner.removeFromBottom (34);
    leftInner.removeFromBottom (8);
    bankList.setBounds (leftInner);
    factoryButton.setBounds (bankButtons.removeFromLeft (bankButtons.getWidth() / 2 - 4));
    bankButtons.removeFromLeft (8);
    userButton.setBounds (bankButtons);

    auto centerInner = centerPanel.reduced (12);
    centerInner.removeFromTop (24);
    listBox.setBounds (centerInner);

    auto rightInner = rightPanel.reduced (22);
    auto detailHead = rightInner.removeFromTop (52);
    detailBadge.setBounds (detailHead.removeFromRight (84).reduced (0, 6));
    detailName.setBounds (detailHead);
    detailBank.setBounds (rightInner.removeFromTop (20));

    rightInner.removeFromTop (12);
    descriptionHeader.setBounds (rightInner.removeFromTop (18));
    descriptionLabel.setBounds (rightInner.removeFromTop (50));
    rightInner.removeFromTop (8);
    useCaseHeader.setBounds (rightInner.removeFromTop (18));
    useCaseLabel.setBounds (rightInner.removeFromTop (38));
    rightInner.removeFromTop (8);
    firstHeader.setBounds (rightInner.removeFromTop (18));
    firstLabel.setBounds (rightInner.removeFromTop (44));

    auto actions = rightInner.removeFromBottom (112);
    browserKeepKey.setBounds (actions.removeFromTop (24));
    actions.removeFromTop (6);

    auto actionRow = actions.removeFromTop (32);
    const int gap = 6;
    const int loadWidth = (actionRow.getWidth() - 2 * gap) / 2;
    loadButton.setBounds (actionRow.removeFromLeft (loadWidth));
    actionRow.removeFromLeft (gap);
    saveButton.setBounds (actionRow.removeFromLeft ((actionRow.getWidth() - gap) / 2));
    actionRow.removeFromLeft (gap);
    exportButton.setBounds (actionRow);

    actions.removeFromTop (6);
    auto secondRow = actions.removeFromTop (30);
    const int smallGap = 4;
    const int smallWidth = (secondRow.getWidth() - 3 * smallGap) / 4;
    duplicateButton.setBounds (secondRow.removeFromLeft (smallWidth));
    secondRow.removeFromLeft (smallGap);
    renameButton.setBounds (secondRow.removeFromLeft (smallWidth));
    secondRow.removeFromLeft (smallGap);
    deleteButton.setBounds (secondRow.removeFromLeft (smallWidth));
    secondRow.removeFromLeft (smallGap);
    importButton.setBounds (secondRow);
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
        g.setColour (VocalRackLookAndFeel::accent());
        g.fillRoundedRectangle (area.toFloat().reduced (5.0f, 3.0f), 7.0f);
    }

    g.setColour (rowIsSelected ? VocalRackLookAndFeel::background()
                               : VocalRackLookAndFeel::text());
    g.setFont (VocalRackLookAndFeel::font (12.5f, rowIsSelected));
    g.drawFittedText (entry.name, area.reduced (15, 3).withTrimmedBottom (14),
                      juce::Justification::centredLeft, 1);

    g.setColour (rowIsSelected ? VocalRackLookAndFeel::background().withAlpha (0.72f)
                               : VocalRackLookAndFeel::dimText());
    g.setFont (VocalRackLookAndFeel::font (10.5f));
    auto bankArea = area.reduced (15, 3).withTrimmedTop (24);
    bankArea.removeFromLeft ((int) (width * 0.58f));
    g.drawFittedText (entry.bank, bankArea, juce::Justification::centredLeft, 1);
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
    browserKeepKey.setToggleState (processor.getKeepSongKey(), juce::dontSendNotification);

    if (entry == nullptr)
    {
        detailName.setText ("No preset selected", juce::dontSendNotification);
        detailBank.setText ({}, juce::dontSendNotification);
        detailBadge.setText ({}, juce::dontSendNotification);
        descriptionLabel.setText ({}, juce::dontSendNotification);
        useCaseLabel.setText ({}, juce::dontSendNotification);
        firstLabel.setText ({}, juce::dontSendNotification);
        countLabel.setText (juce::String (processor.getPresets().getEntries().size())
                                + " factory presets",
                            juce::dontSendNotification);
        return;
    }

    detailBadge.setText (entry->factory ? "FACTORY" : "USER", juce::dontSendNotification);
    detailName.setText (entry->name, juce::dontSendNotification);
    detailName.setTooltip (entry->name);
    detailBank.setText ("Bank: " + entry->bank, juce::dontSendNotification);
    descriptionLabel.setText (entry->description, juce::dontSendNotification);
    useCaseLabel.setText (entry->useCase, juce::dontSendNotification);
    firstLabel.setText (entry->firstAdjustment, juce::dontSendNotification);

    int shown = 0;

    for (const auto& candidate : processor.getPresets().getEntries())
        if (candidate.factory)
            ++shown;

    countLabel.setText (juce::String (shown) + " factory presets", juce::dontSendNotification);
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

} // namespace vr
