#pragma once
// ChainHostLookAndFeel — extends Gin's CopperLookAndFeel with our colour scheme
#include <gin_plugin/gin_plugin.h>

class ChainHostLookAndFeel : public gin::CopperLookAndFeel
{
public:
    ChainHostLookAndFeel();

    // V1 Candlelight rotary: thin track + value arc + indicator line. No bevel, no dot.
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};
