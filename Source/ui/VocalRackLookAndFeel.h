#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vr
{

class VocalRackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VocalRackLookAndFeel();

    static juce::Colour background() noexcept { return juce::Colour (0xff17191d); }
    static juce::Colour panel() noexcept { return juce::Colour (0xff202329); }
    static juce::Colour panelLight() noexcept { return juce::Colour (0xff2a2e35); }
    static juce::Colour text() noexcept { return juce::Colour (0xffe9e7df); }
    static juce::Colour dimText() noexcept { return juce::Colour (0xff8f958f); }
    static juce::Colour accent() noexcept { return juce::Colour (0xffd6ff3f); }
    static juce::Colour accentSoft() noexcept { return juce::Colour (0xff6f8f2a); }
    static juce::Colour danger() noexcept { return juce::Colour (0xffff6b5e); }

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

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static juce::Font font (float height, bool bold = false);
};

} // namespace vr
