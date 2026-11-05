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
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    const ParamSpec* spec = nullptr;
    juce::Label nameLabel;
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
    juce::Label bankLabel;
    juce::Label searchLabel;
    juce::TextEditor searchEditor;
    juce::ComboBox bankBox;
    juce::ListBox listBox;
    juce::Label detailsLabel;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save As" };
    juce::TextButton duplicateButton { "Duplicate" };
    juce::TextButton renameButton { "Rename" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton importButton { "Import" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton closeButton { "Close" };

    std::vector<int> visibleEntries;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace vr
