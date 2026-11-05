#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vr
{

class VocalRackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VocalRackLookAndFeel();

    static juce::Colour background() noexcept { return juce::Colour (0xff101210); }
    static juce::Colour panel() noexcept { return juce::Colour (0xff171a17); }
    static juce::Colour panelLight() noexcept { return juce::Colour (0xff202420); }
    static juce::Colour card() noexcept { return juce::Colour (0xff1b1f1b); }
    static juce::Colour line() noexcept { return juce::Colour (0xff303630); }
    static juce::Colour text() noexcept { return juce::Colour (0xffeef1e8); }
    static juce::Colour dimText() noexcept { return juce::Colour (0xff939a91); }
    static juce::Colour accent() noexcept { return juce::Colour (0xffc8f45a); }
    static juce::Colour accentSoft() noexcept { return juce::Colour (0xff728f2d); }
    static juce::Colour danger() noexcept { return juce::Colour (0xffff7a66); }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawTabButton (juce::TabBarButton&, juce::Graphics&,
                        bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;

    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static juce::Font font (float height, bool bold = false);
};

} // namespace vr
