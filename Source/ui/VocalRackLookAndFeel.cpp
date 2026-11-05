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
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff383e37));
    setColour (juce::Label::textColourId, text());
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, panelLight());
    setColour (juce::TextButton::buttonOnColourId, accent());
    setColour (juce::TextButton::textColourOffId, text());
    setColour (juce::TextButton::textColourOnId, background());
    setColour (juce::ToggleButton::textColourId, text());
    setColour (juce::ToggleButton::tickColourId, accent());
    setColour (juce::ToggleButton::tickDisabledColourId, dimText());
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c201c));
    setColour (juce::ComboBox::outlineColourId, line());
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::arrowColourId, accent());
    setColour (juce::ComboBox::buttonColourId, juce::Colour (0xff1c201c));
    setColour (juce::PopupMenu::backgroundColourId, card());
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentSoft());
    setColour (juce::PopupMenu::highlightedTextColourId, text());
    setColour (juce::ListBox::backgroundColourId, card());
    setColour (juce::ListBox::outlineColourId, line());
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141714));
    setColour (juce::TextEditor::textColourId, text());
    setColour (juce::TextEditor::outlineColourId, line());
    setColour (juce::TextEditor::focusedOutlineColourId, accent());
    setColour (juce::TextEditor::highlightColourId, accentSoft());
    setColour (juce::TextEditor::highlightedTextColourId, text());
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff495049));
    setColour (juce::ScrollBar::trackColourId, juce::Colour (0xff141714));
}

juce::Slider::SliderLayout VocalRackLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    auto layout = juce::LookAndFeel_V4::getSliderLayout (slider);

    if (slider.getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
    {
        auto local = slider.getLocalBounds();
        layout.textBoxBounds = local.removeFromBottom (juce::jmin (24, local.getHeight() / 4));
        layout.sliderBounds = local;
    }

    return layout;
}

void VocalRackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 9.0f;
    const auto centre = bounds.getCentre();
    const float start = juce::jmax (rotaryStartAngle, -2.45f);
    const float end = juce::jmin (rotaryEndAngle, 2.45f);
    const float angle = start + juce::jlimit (0.0f, 1.0f, sliderPos) * (end - start);

    // Soft outer halo, then the unfilled groove.
    g.setColour (accent().withAlpha (slider.isEnabled() ? 0.035f : 0.015f));
    g.fillEllipse (centre.x - radius - 9.0f, centre.y - radius - 9.0f,
                   (radius + 9.0f) * 2.0f, (radius + 9.0f) * 2.0f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff363c35));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (std::abs (angle - start) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, angle, true);
        g.setColour (slider.isEnabled() ? accent() : dimText());
        g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const float knobRadius = radius * 0.62f;
    g.setColour (juce::Colour (0xff151815));
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (juce::Colour (0xff3a4138));
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius,
                   knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    const float tickRadius = radius * 0.76f;
    const juce::Point<float> pointer (centre.x + std::sin (angle) * tickRadius,
                                      centre.y - std::cos (angle) * tickRadius);
    g.setColour (slider.isEnabled() ? text() : dimText());
    g.drawLine (centre.x + std::sin (angle) * knobRadius * 0.5f,
                centre.y - std::cos (angle) * knobRadius * 0.5f,
                pointer.x, pointer.y, 2.0f);
    g.setColour (accent());
    g.fillEllipse (pointer.x - 2.2f, pointer.y - 2.2f, 4.4f, 4.4f);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (accent().withAlpha (0.8f));
        g.drawEllipse (centre.x - radius - 4.0f, centre.y - radius - 4.0f,
                       (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f, 1.2f);
    }
}

void VocalRackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 8.0f;
    auto colour = button.getToggleState() ? accent() : backgroundColour;

    if (isDown)
        colour = colour.brighter (0.08f);
    else if (isHighlighted)
        colour = colour.brighter (0.04f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, corner);

    if (! button.getToggleState())
    {
        g.setColour (line());
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (accent().withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.4f);
    }
}

void VocalRackLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool isHighlighted, bool isDown)
{
    juce::ignoreUnused (isHighlighted, isDown);

    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float boxSize = juce::jmin (19.0f, bounds.getHeight() - 4.0f);
    auto box = bounds.removeFromLeft (boxSize).withSizeKeepingCentre (boxSize, boxSize);

    g.setColour (button.getToggleState() ? accent() : juce::Colour (0xff161916));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (button.getToggleState() ? accent() : juce::Colour (0xff4a5149));
    g.drawRoundedRectangle (box, 4.0f, 1.0f);

    if (button.getToggleState())
    {
        juce::Path check;
        check.startNewSubPath (box.getX() + box.getWidth() * 0.24f, box.getCentreY());
        check.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.26f);
        check.lineTo (box.getX() + box.getWidth() * 0.78f, box.getY() + box.getHeight() * 0.27f);
        g.setColour (background());
        g.strokePath (check, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    g.setColour (button.getToggleState() ? text() : dimText());
    g.setFont (font (12.0f, false));
    g.drawFittedText (button.getButtonText(),
                      bounds.toNearestInt().withTrimmedLeft (8).withTrimmedRight (2),
                      juce::Justification::centredLeft, 1);

    if (button.hasKeyboardFocus (true))
    {
        g.setColour (accent().withAlpha (0.85f));
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.2f);
    }
}

void VocalRackLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent() : line());
    g.drawRoundedRectangle (bounds, 8.0f, box.hasKeyboardFocus (true) ? 1.4f : 1.0f);

    const float arrowSize = 5.0f;
    const auto centre = juce::Point<float> ((float) width - 16.0f, (float) height * 0.5f);
    juce::Path arrow;
    arrow.startNewSubPath (centre.x - arrowSize, centre.y - arrowSize * 0.4f);
    arrow.lineTo (centre.x, centre.y + arrowSize * 0.6f);
    arrow.lineTo (centre.x + arrowSize, centre.y - arrowSize * 0.4f);
    g.setColour (box.isEnabled() ? accent() : dimText());
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void VocalRackLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (font (12.5f));
    label.setJustificationType (juce::Justification::centredLeft);
}

void VocalRackLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                  juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (editor.hasKeyboardFocus (true) ? accent() : line());
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

int VocalRackLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int)
{
    auto& bar = button.getTabbedButtonBar();
    const int count = juce::jmax (1, bar.getNumTabs());
    return juce::jmax (72, bar.getWidth() / count);
}

void VocalRackLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                          bool isMouseOver, bool isMouseDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    const bool selected = button.getToggleState();

    if (selected)
    {
        g.setColour (accent().withAlpha (0.08f));
        g.fillRoundedRectangle (bounds, 6.0f);
    }
    else if (isMouseOver || isMouseDown)
    {
        g.setColour (panelLight().withAlpha (0.75f));
        g.fillRoundedRectangle (bounds, 6.0f);
    }

    g.setColour (selected ? accent() : dimText());
    g.setFont (font (12.5f, selected));
    auto text = button.getButtonText();
    g.drawFittedText (text, bounds.toNearestInt().withTrimmedLeft (8).withTrimmedRight (14),
                      juce::Justification::centredLeft, 1);

    if (button.getIndex() < button.getTabbedButtonBar().getNumTabs() - 1)
    {
        g.setColour (line());
        g.setFont (font (12.0f));
        g.drawText (">", bounds.toNearestInt().removeFromRight (14),
                    juce::Justification::centredRight);
    }

    if (selected)
    {
        g.setColour (accent());
        g.fillRect (bounds.getX() + 6.0f, bounds.getBottom() - 2.0f,
                    bounds.getWidth() - 12.0f, 2.0f);
    }
}

void VocalRackLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

juce::Font VocalRackLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

juce::Font VocalRackLookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    if (button.getWidth() < 30)
        return font (10.0f, true);

    if (button.getWidth() < 70)
        return font (9.5f, button.getToggleState());

    if (button.getWidth() < 88)
        return font (10.5f, button.getToggleState());

    return font (juce::jmin (12.5f, (float) buttonHeight * 0.46f), button.getToggleState());
}

juce::Font VocalRackLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (12.5f);
}

juce::Font VocalRackLookAndFeel::getPopupMenuFont()
{
    return font (13.0f);
}

} // namespace vr
