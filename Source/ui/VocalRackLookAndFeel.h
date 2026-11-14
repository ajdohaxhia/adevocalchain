#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vr
{

class VocalRackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VocalRackLookAndFeel();

    // Neutral graphite channel-strip palette with a restrained blue accent.
    static juce::Colour background() noexcept { return juce::Colour (0xff1b1d21); }
    static juce::Colour panel() noexcept { return juce::Colour (0xff23262b); }
    static juce::Colour panelLight() noexcept { return juce::Colour (0xff2e323a); }
    static juce::Colour card() noexcept { return juce::Colour (0xff202329); }
    static juce::Colour line() noexcept { return juce::Colour (0xff393e46); }
    static juce::Colour text() noexcept { return juce::Colour (0xffe9ebee); }
    static juce::Colour dimText() noexcept { return juce::Colour (0xff888e98); }
    static juce::Colour accent() noexcept { return juce::Colour (0xff4d9be6); }
    static juce::Colour accentSoft() noexcept { return juce::Colour (0xff2f5d86); }
    static juce::Colour danger() noexcept { return juce::Colour (0xffff6f61); }

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
