#pragma once
// FabKnob — arc knob using juce::Slider + CopperLookAndFeel + value overlay
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"

class FabKnob : public juce::Component
{
public:
    FabKnob (const juce::String& label = {}, juce::Colour arcCol = Colors::accent);

    void resized() override;
    void paint (juce::Graphics&) override;

    void setValue (float v, bool notify = true);
    float getValue() const;

    void setRange (float mn, float mx);
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setArcColour (juce::Colour c);
    void setSuffix (const juce::String& s) { suffix = s; repaint(); }
    void setDefaultValue (float v) { defaultValue = v; }
    void setShowPercentage (bool b) { showPercentage = b; repaint(); }

    juce::Slider& getSlider() { return slider; }
    std::function<void()> onValueChange;

private:
    juce::Slider slider;
    juce::String label, suffix = "%";
    juce::Colour arcColour;
    float defaultValue = 0.0f;
    bool showPercentage = true;
};
