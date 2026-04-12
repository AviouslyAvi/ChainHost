#pragma once
// FabKnob — arc knob using juce::Slider + CopperLookAndFeel + value overlay
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Colors.h"

class FabKnob : public juce::Component, public juce::DragAndDropTarget
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

    // Macro drag-drop target
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails&) override { macroDragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { macroDragHover = false; repaint(); }

    // Called when a macro is dropped on this knob: (macroIndex)
    std::function<void (int)> onMacroDropped;

private:
    juce::Slider slider;
    juce::String label, suffix = "%";
    juce::Colour arcColour;
    float defaultValue = 0.0f;
    bool showPercentage = true;
    bool macroDragHover = false;
};
