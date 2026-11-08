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
    void setTitle (const juce::String& newTitle) { title = newTitle; repaint(); }
    void setModuleStrip (bool shouldBeStrip) { moduleStrip = shouldBeStrip; repaint(); }

    void setTopComponent (juce::Component* component, int preferredHeight = 150)
    {
        topComponent = component;
        topHeight = juce::jmax (60, preferredHeight);

        if (topComponent != nullptr)
            addAndMakeVisible (topComponent);

        resized();
    }

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

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
        auto area = getLocalBounds().reduced (18);

        if (title.isNotEmpty())
        {
            area.removeFromTop (42);
        }

        if (! headerControls.empty())
        {
            auto header = area.removeFromTop (moduleStrip ? 42 : 38);

            if (moduleStrip)
            {
                const int width = header.getWidth() / juce::jmax (1, (int) headerControls.size());

                for (size_t i = 0; i < headerControls.size(); ++i)
                {
                    auto cell = header.removeFromLeft (juce::jmin (width, header.getWidth()));
                    headerControls[i]->setBounds (cell.reduced (8, 3));
                }
            }
            else
            {
                const int count = juce::jmax (1, (int) headerControls.size());
                const int itemWidth = juce::jmin (count <= 2 ? 136 : 128,
                                                  (header.getWidth() - (count - 1) * 8) / count);
                const int total = count * itemWidth + (count - 1) * 8;
                int x = juce::jmax (header.getX(), header.getRight() - total);

                for (auto* control : headerControls)
                {
                    control->setBounds (x, header.getY() + 2, itemWidth, header.getHeight() - 5);
                    x += itemWidth + 8;
                }
            }

            area.removeFromTop (6);
        }

        if (topComponent != nullptr)
        {
            const int graphHeight = juce::jlimit (90, topHeight,
                                                  juce::jmax (90, area.getHeight() * 32 / 100));
            topComponent->setBounds (area.removeFromTop (graphHeight));
            area.removeFromTop (10);
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
            const int height = juce::jmin (juce::jmax (82, cellHeight - 8),
                                           rows == 1 ? 300 : 190);
            juce::Rectangle<int> cell (area.getX() + column * cellWidth,
                                       area.getY() + row * cellHeight + 4,
                                       juce::jmax (40, cellWidth - 4),
                                       height);
            gridControls[i]->setBounds (cell);
        }
    }

private:
    int columns = 7;
    juce::String title;
    bool moduleStrip = false;
    std::vector<juce::Component*> headerControls;
    std::vector<juce::Component*> gridControls;
    juce::Component* topComponent = nullptr;
    int topHeight = 150;
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

    void addQuickKnob (const char* parameterId, const char* displayName = nullptr);
    void addQuickPower (const char* parameterId, const char* label);
    void addKnob (ControlGrid& grid, const char* parameterId, const char* subLabel = nullptr);
    void addPower (ControlGrid& grid, const char* parameterId, const char* label);
    juce::ToggleButton* addToggle (ControlGrid& grid, const char* parameterId, const char* label);
    void addCombo (ControlGrid& grid, const char* parameterId, const char* label);

    VocalRackAudioProcessor& processor;
    VocalRackLookAndFeel lookAndFeel;

    // Top bar
    juce::Label logoLabel;
    juce::Label productLabel;
    juce::Label presetNameLabel;
    juce::Label presetMetaLabel;
    juce::Label latencyLabel;
    juce::Label sampleRateLabel;
    juce::Label ceilingLabel;
    juce::TextButton previousButton { "<" };
    juce::TextButton nextButton { ">" };
    juce::TextButton browseButton { "BROWSE" };
    juce::TextButton abAButton { "A" };
    juce::TextButton abBButton { "B" };
    juce::TextButton copyABButton { "COPY" };
    juce::TextButton matchABButton { "M" };
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
    juce::Label maskLabel;
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

    juce::ToggleButton* formantToggle = nullptr;
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
