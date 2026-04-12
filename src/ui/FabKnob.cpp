#include "FabKnob.h"

FabKnob::FabKnob (const juce::String& lbl, juce::Colour arcCol)
    : label (lbl), arcColour (arcCol)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 1.0, 0.001);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, arcColour);
    slider.setColour (juce::Slider::trackColourId, Colors::border);
    slider.onValueChange = [this]() { repaint(); if (onValueChange) onValueChange(); };
    addAndMakeVisible (slider);
}

void FabKnob::resized()
{
    auto b = getLocalBounds();
    int labelH = label.isEmpty() ? 0 : 14;
    int valueH = showPercentage ? 12 : 0;
    auto sliderBounds = b.withTrimmedBottom (labelH + valueH);
    int sz = juce::jmin (sliderBounds.getWidth(), sliderBounds.getHeight());
    slider.setBounds (sliderBounds.withSizeKeepingCentre (sz, sz));
}

void FabKnob::paint (juce::Graphics& g)
{
    if (showPercentage)
    {
        float norm = (float) slider.getValue();
        if (slider.getMaximum() > 1.1)
        {
            g.setColour (Colors::text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            auto valStr = juce::String (slider.getValue(), 2) + suffix;
            g.drawText (valStr, 0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
        else
        {
            g.setColour (Colors::text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (juce::String (juce::roundToInt (norm * 100.0f)) + suffix,
                        0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
    }

    if (label.isNotEmpty())
    {
        g.setColour (Colors::textDim);
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        g.drawText (label, 0, getHeight() - 12, getWidth(), 12, juce::Justification::centred);
    }

    if (macroDragHover)
    {
        g.setColour (Colors::accent.withAlpha (0.2f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (Colors::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1), 4.0f, 1.5f);
    }
}

void FabKnob::setValue (float v, bool notify)
{
    slider.setValue (v, notify ? juce::sendNotificationAsync : juce::dontSendNotification);
}

float FabKnob::getValue() const { return (float) slider.getValue(); }

void FabKnob::setRange (float mn, float mx)
{
    slider.setRange (mn, mx, (mx - mn) < 2.0f ? 0.001 : 0.01);
}

void FabKnob::setArcColour (juce::Colour c)
{
    arcColour = c;
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    repaint();
}

bool FabKnob::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("macro:");
}

void FabKnob::itemDropped (const SourceDetails& details)
{
    macroDragHover = false; repaint();
    auto desc = details.description.toString();
    if (desc.startsWith ("macro:") && onMacroDropped)
    {
        int macroIdx = desc.fromFirstOccurrenceOf ("macro:", false, false).getIntValue();
        onMacroDropped (macroIdx);
    }
}
