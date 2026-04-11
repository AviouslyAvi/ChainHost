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
