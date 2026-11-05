#pragma once

#include "PluginProcessor.h"
#include "ui/RackComponents.h"
#include "ui/VocalRackLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace vr
{

/** Simple responsive grid used by both Quick and module editors. */
class ControlGrid final : public juce::Component
{
public:
    void setColumns (int newColumns) noexcept { columns = juce::jmax (1, newColumns); }

    void addHeaderControl (juce::Component* control)
    {
        addAndMakeVisible (control);
        headerControls.push_back (control);

        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    void addGridControl (juce::Component* control)
    {
        addAndMakeVisible (control);
        gridControls.push_back (control);

        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);

        if (! headerControls.empty())
        {
            auto header = area.removeFromTop (30);
            const int width = juce::jmin (130, header.getWidth() / (int) headerControls.size());

            for (size_t i = 0; i < headerControls.size(); ++i)
            {
                auto cell = header.removeFromLeft (juce::jmin (width, header.getWidth()));
                headerControls[i]->setBounds (cell.reduced (2, 2));
            }

            area.removeFromTop (4);
        }

        if (gridControls.empty())
            return;

        const int rows = (int) std::ceil ((double) gridControls.size() / (double) columns);
        const int cellWidth = area.getWidth() / columns;
        const int cellHeight = area.getHeight() / juce::jmax (1, rows);

        for (size_t i = 0; i < gridControls.size(); ++i)
        {
            const int row = (int) i / columns;
            const int column = (int) i % columns;
            const int height = juce::jmin (juce::jmax (60, cellHeight - 8), 126);
            juce::Rectangle<int> cell (area.getX() + column * cellWidth,
                                       area.getY() + row * cellHeight + 4,
                                       juce::jmax (40, cellWidth - 4),
                                       height);
            gridControls[i]->setBounds (cell);
        }
    }

private:
    int columns = 7;
    std::vector<juce::Component*> headerControls;
    std::vector<juce::Component*> gridControls;
};

class VocalRackEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit VocalRackEditor (VocalRackAudioProcessor&);
    ~VocalRackEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureTopBar();
    void configureStatusBar();
    void configureQuickView();
    void configureDetailTabs();
    void refreshFromProcessor();
    void loadRelativePreset (int delta);
    void toggleBrowser();
    void toggleMaskNote (int pitchClass);
    void setView (bool detail);
    void showNotice (const juce::String& message);

    void addQuickKnob (const char* parameterId);
    void addQuickPower (const char* parameterId, const char* label);
    void addKnob (ControlGrid& grid, const char* parameterId);
    void addPower (ControlGrid& grid, const char* parameterId, const char* label);
    void addToggle (ControlGrid& grid, const char* parameterId, const char* label);
    void addCombo (ControlGrid& grid, const char* parameterId, const char* label);

    VocalRackAudioProcessor& processor;
    VocalRackLookAndFeel lookAndFeel;

    // Top bar
    juce::Label logoLabel;
    juce::Label productLabel;
    juce::Label presetNameLabel;
    juce::Label presetMetaLabel;
    juce::Label latencyLabel;
    juce::TextButton previousButton { "<" };
    juce::TextButton nextButton { ">" };
    juce::TextButton browseButton { "BROWSE" };
    juce::TextButton abAButton { "A" };
    juce::TextButton abBButton { "B" };
    juce::TextButton copyABButton { "COPY" };
    juce::ToggleButton keepKeyButton { "Keep Song Key" };
    juce::ComboBox qualityBox;
    PowerButton globalBypass;
    juce::TextButton quickButton { "QUICK" };
    juce::TextButton detailButton { "DETAIL" };

    // Status
    MeterPanel meters;
    PitchPanel pitchPanel;
    NoteKeyboard keyboard;
    juce::Label keyLabel;
    juce::ComboBox tonicBox;
    juce::ComboBox scaleBox;

    // Views
    ControlGrid quickView;
    juce::TabbedComponent detailTabs { juce::TabbedButtonBar::TabsAtTop };
    PresetBrowser browser;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tonicAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> detailComboAttachments;

    juce::Label noticeLabel;
    juce::Label spectralLabel;
    juce::Label qualityHintLabel;

    bool detailMode = false;
    bool stateDirty = true;
    double noticeExpiryMs = 0.0;
    juce::String lastNotice;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalRackEditor)
};

} // namespace vr
