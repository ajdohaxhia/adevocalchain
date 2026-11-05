#include "VocalRackLookAndFeel.h"

#include <cmath>

namespace vr
{

juce::Font VocalRackLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Helvetica Neue", height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

VocalRackLookAndFeel::VocalRackLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text());
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, accent());
    setColour (juce::Slider::rotarySliderOutlineColourId, panelLight());
    setColour (juce::Label::textColourId, text());
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, panelLight());
    setColour (juce::TextButton::buttonOnColourId, accent());
    setColour (juce::TextButton::textColourOffId, text());
    setColour (juce::TextButton::textColourOnId, background());
    setColour (juce::ToggleButton::textColourId, text());
    setColour (juce::ToggleButton::tickColourId, accent());
    setColour (juce::ComboBox::backgroundColourId, panelLight());
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3f47));
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::arrowColourId, dimText());
    setColour (juce::ComboBox::buttonColourId, panelLight());
    setColour (juce::PopupMenu::backgroundColourId, panel());
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentSoft());
    setColour (juce::PopupMenu::highlightedTextColourId, text());
    setColour (juce::ListBox::backgroundColourId, panel());
    setColour (juce::ListBox::outlineColourId, juce::Colour (0xff343941));
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff14161a));
    setColour (juce::TextEditor::textColourId, text());
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a4048));
    setColour (juce::TextEditor::focusedOutlineColourId, accent());
    setColour (juce::TextEditor::highlightColourId, accentSoft());
    setColour (juce::TextEditor::highlightedTextColourId, text());
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4a5059));
}

void VocalRackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f;
    const auto centre = bounds.getCentre();
    const float start = juce::jmax (rotaryStartAngle, -2.6f);
    const float end = juce::jmin (rotaryEndAngle, 2.6f);
    const float angle = start + juce::jlimit (0.0f, 1.0f, sliderPos) * (end - start);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff343941));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (std::abs (angle - start) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, angle, true);
        g.setColour (slider.isEnabled() ? accent() : dimText());
        g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const float knobRadius = radius * 0.63f;
    g.setColour (juce::Colour (0xff262a31));
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (juce::Colour (0xff3d434c));
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    const juce::Point<float> pointer (centre.x + std::sin (angle) * radius * 0.78f,
                                      centre.y - std::cos (angle) * radius * 0.78f);
    g.setColour (accent());
    g.drawLine (centre.x, centre.y, pointer.x, pointer.y, 2.2f);
    g.fillEllipse (centre.x - 2.2f, centre.y - 2.2f, 4.4f, 4.4f);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (accent().withAlpha (0.85f));
        g.drawEllipse (centre.x - radius - 3.0f, centre.y - radius - 3.0f,
                       (radius + 3.0f) * 2.0f, (radius + 3.0f) * 2.0f, 1.4f);
    }
}

void VocalRackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 5.0f;
    auto colour = button.getToggleState() ? accent() : backgroundColour;

    if (isDown)
        colour = colour.brighter (0.08f);
    else if (isHighlighted)
        colour = colour.brighter (0.04f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, corner);

    if (! button.getToggleState())
    {
        g.setColour (juce::Colour (0xff3a4048));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (accent().withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.2f);
    }
}

void VocalRackLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float pillWidth = 34.0f;
    auto pill = bounds.removeFromLeft (pillWidth).withSizeKeepingCentre (pillWidth, 20.0f);

    g.setColour (button.getToggleState() ? accentSoft() : juce::Colour (0xff2c3037));
    g.fillRoundedRectangle (pill, 10.0f);
    g.setColour (juce::Colour (0xff454b54));
    g.drawRoundedRectangle (pill, 10.0f, 1.0f);

    auto thumb = pill.reduced (2.0f).withWidth (16.0f);

    if (button.getToggleState())
        thumb.setX (pill.getRight() - 18.0f);

    g.setColour (button.getToggleState() ? accent() : dimText());
    g.fillEllipse (thumb);

    g.setColour (text());
    auto textArea = bounds.reduced (8.0f, 0.0f);
    g.setFont (font (12.0f, button.getToggleState()));
    g.drawFittedText (button.getButtonText(), textArea.toNearestInt(),
                      juce::Justification::centredLeft, 1);

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (accent().withAlpha (0.8f));
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f), 4.0f, 1.2f);
    }

    juce::ignoreUnused (isHighlighted, isDown);
}

void VocalRackLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent() : juce::Colour (0xff3a4048));
    g.drawRoundedRectangle (bounds, 4.0f, box.hasKeyboardFocus (true) ? 1.4f : 1.0f);

    const float arrowSize = 5.0f;
    const auto centre = juce::Point<float> ((float) width - 15.0f, (float) height * 0.5f);
    juce::Path arrow;
    arrow.startNewSubPath (centre.x - arrowSize, centre.y - arrowSize * 0.5f);
    arrow.lineTo (centre.x, centre.y + arrowSize * 0.5f);
    arrow.lineTo (centre.x + arrowSize, centre.y - arrowSize * 0.5f);
    g.setColour (dimText());
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void VocalRackLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (9, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (font (12.0f));
    label.setJustificationType (juce::Justification::centredLeft);
}

void VocalRackLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                  juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (editor.hasKeyboardFocus (true) ? accent() : juce::Colour (0xff3a4048));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

juce::Font VocalRackLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

juce::Font VocalRackLookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    if (button.getWidth() < 32)
        return font (9.0f, true);

    return font (juce::jmin (13.0f, (float) buttonHeight * 0.52f));
}

juce::Font VocalRackLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (12.0f);
}

juce::Font VocalRackLookAndFeel::getPopupMenuFont()
{
    return font (13.0f);
}

} // namespace vr
