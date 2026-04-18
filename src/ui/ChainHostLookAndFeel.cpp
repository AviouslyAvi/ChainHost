#include "ChainHostLookAndFeel.h"
#include "Colors.h"

ChainHostLookAndFeel::ChainHostLookAndFeel()
{
    using G = gin::GinLookAndFeel;
    setColour (G::backgroundColourId, Colors::bg);
    setColour (G::groupColourId, Colors::surfaceRaised);
    setColour (G::lightGroupColourId, Colors::surfaceHover);
    setColour (G::accentColourId, Colors::accent);
    setColour (G::grey30ColourId, Colors::textDim);
    setColour (G::grey45ColourId, Colors::textMid);
    setColour (G::grey60ColourId, Colors::textMid);
    setColour (G::grey90ColourId, Colors::text);

    setColour (juce::Slider::rotarySliderFillColourId, Colors::accent);
    setColour (juce::Slider::trackColourId, Colors::border);
    setColour (juce::Slider::thumbColourId, Colors::accent);

    setColour (juce::Label::textColourId, Colors::text);
    setColour (juce::TextButton::textColourOnId, Colors::accent);
    setColour (juce::TextButton::textColourOffId, Colors::textMid);

    setColour (juce::PopupMenu::backgroundColourId, Colors::surface);
    setColour (juce::PopupMenu::textColourId, Colors::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colors::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Colors::text);
}

void ChainHostLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                             int x, int y, int width, int height,
                                             float sliderPos,
                                             float startAngle, float endAngle,
                                             juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                                (float) width, (float) height);
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const float stroke = 2.5f;
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);

    // Knob body — soft white disc with hairline border, gives the knob
    // physical presence against light panels
    const float bodyR = radius - stroke * 0.5f;
    g.setColour (Colors::surfaceRaised);
    g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (Colors::borderSubtle);
    g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 0.5f);

    // Track
    juce::Path track;
    track.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colors::border);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc
    if (valueAngle - startAngle > 0.001f)
    {
        juce::Path fill;
        fill.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, valueAngle, true);
        const auto arcColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
        g.setColour (arcColour);
        g.strokePath (fill, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // Indicator tick — crisp full-stroke line near the rim
    const float innerR = radius * 0.55f;
    const float outerR = radius - 2.0f;
    const float sinA = std::sin (valueAngle);
    const float cosA = -std::cos (valueAngle);
    const float x1 = cx + sinA * innerR;
    const float y1 = cy + cosA * innerR;
    const float x2 = cx + sinA * outerR;
    const float y2 = cy + cosA * outerR;
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.drawLine (x1, y1, x2, y2, stroke);
}
