#pragma once

#include "Engine.h"
#include "Parameters.h"
#include "Presets.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>

namespace vr
{

class VocalRackAudioProcessor;

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
          const juce::String& displayName = {});

    void resized() override;
    void paint (juce::Graphics&) override;
    void setSubLabel (const juce::String& text);

    juce::Slider& getSlider() noexcept { return slider; }

private:
    const ParamSpec* spec = nullptr;
    juce::Label nameLabel;
    juce::Label subLabel;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

class PowerButton final : public juce::Button,
                          private juce::Timer
{
public:
    PowerButton (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                 const juce::String& shortLabel);

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void clicked() override;

private:
    void timerCallback() override;

    juce::RangedAudioParameter* parameter = nullptr;
    juce::String label;
    bool lastState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PowerButton)
};

class DynamicEQGraph final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    DynamicEQGraph (juce::AudioProcessorValueTreeState& state, Engine& engine);
    ~DynamicEQGraph() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float parameterValue (const juce::String& id) const;
    void setParameterValue (const juce::String& id, float value);
    juce::Point<float> handlePosition (int band) const;
    juce::Point<float> livePosition (int band) const;
    float frequencyAtX (float x) const;
    float xAtFrequency (double frequency) const;
    float dbAtY (float y) const;
    float yAtDb (float db) const;
    int bandAt (juce::Point<float> position) const;
    void updateCurve();
    double magnitudeDb (double frequency, int band, bool live) const;

    juce::AudioProcessorValueTreeState& apvts;
    Engine& engine;
    juce::Path staticCurve;
    juce::Path liveCurve;
    int activeBand = -1;
    float liveGainDb[2] { 0.0f, 0.0f };
    float staticGainDb[2] { 0.0f, 0.0f };
};

class MeterPanel final : public juce::Component,
                         private juce::Timer
{
public:
    explicit MeterPanel (Engine& engine);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float meterPosition (float db) noexcept;

    Engine& engine;
    float inputDb = -60.0f;
    float outputDb = -60.0f;
    float peakHoldDb = -60.0f;
    int peakHoldTicks = 0;
    float compGr = 0.0f;
    float limiterGr = 0.0f;
    float dynamicEqGr = 0.0f;
};

class PitchPanel final : public juce::Component,
                         private juce::Timer
{
public:
    explicit PitchPanel (Engine& engine);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    Engine& engine;
    PitchStatus status;
};

class NoteKeyboard final : public juce::Component,
                           private juce::Timer
{
public:
    explicit NoteKeyboard (juce::AudioProcessorValueTreeState& state);

    std::function<void (int)> onToggle;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    int effectiveMask() const;
    int currentTonic() const;
    int currentScale() const;

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::unique_ptr<juce::TextButton>, 12> keys;
    int lastMask = -1;
    int lastTonic = -1;
};

class BankListModel final : public juce::ListBoxModel
{
public:
    std::function<int()> getRowCount;
    std::function<juce::String (int)> getRowName;
    std::function<int()> getSelectedRow;
    std::function<void (int)> onRowSelected;

    int getNumRows() override { return getRowCount ? getRowCount() : 0; }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
};

class PresetBrowser final : public juce::Component,
                            private juce::ListBoxModel
{
public:
    PresetBrowser (VocalRackAudioProcessor& processor, std::function<void()> changedCallback);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void refresh();
    void showForCurrentPreset();

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;

private:
    void reloadBanks();
    void updateDetails();
    void selectBank (int row);
    void rebuildBankList();
    const PresetEntry* selectedEntry() const;
    void loadSelected();
    void saveAs();
    void duplicateSelected();
    void renameSelected();
    void deleteSelected();
    void importPreset();
    void exportSelected();

    void showPresetSaveDialog();
    void showNameDialog (const juce::String& title, const juce::String& initial,
                         std::function<void (const juce::String&)> callback);
    void showConfirm (const juce::String& title, const juce::String& message,
                      std::function<void()> callback);
    void showError (const juce::String& message);
    void rebuildVisibleList();

    VocalRackAudioProcessor& processor;
    std::function<void()> onChanged;

    juce::Label titleLabel;
    juce::Label countLabel;
    juce::Label bankLabel;
    juce::Label searchLabel;
    juce::TextEditor searchEditor;
    juce::TextButton clearSearchButton { "X" };
    juce::ComboBox bankBox;
    juce::ListBox bankList;
    BankListModel bankModel;
    juce::ListBox listBox;
    juce::Label detailsLabel;

    juce::Label detailBadge;
    juce::Label detailName;
    juce::Label detailBank;
    juce::Label descriptionHeader;
    juce::Label descriptionLabel;
    juce::Label useCaseHeader;
    juce::Label useCaseLabel;
    juce::Label firstHeader;
    juce::Label firstLabel;
    juce::ToggleButton browserKeepKey { "Keep Song Key" };
    juce::TextButton factoryButton { "Factory" };
    juce::TextButton userButton { "User" };
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save As" };
    juce::TextButton duplicateButton { "Duplicate" };
    juce::TextButton renameButton { "Rename" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton importButton { "Import" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton closeButton { "Close" };

    std::vector<int> visibleEntries;
    std::vector<juce::String> banks;
    juce::Rectangle<int> leftPanel;
    juce::Rectangle<int> centerPanel;
    juce::Rectangle<int> rightPanel;
    int selectedBank = 0;
    bool userOnly = false;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace vr
