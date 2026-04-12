#pragma once
// ArcKnob — rotary arc knob with modulation halo ring
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Colors.h"

class ArcKnob : public juce::Component, public juce::DragAndDropTarget
{
public:
    ArcKnob (const juce::String& label = {}, juce::Colour arcCol = Colors::accent);

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
    void setModDepth (float d) { modDepth = d; repaint(); }
    float getModDepth() const { return modDepth; }
    bool isDraggingHalo() const { return draggingModDepth; }

    juce::Slider& getSlider() { return slider; }
    std::function<void()> onValueChange;

    // Called when user drags the halo ring to change mod depth: (newDepth)
    std::function<void (float)> onModDepthChanged;

    // Macro drag-drop target
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails&) override { macroDragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { macroDragHover = false; repaint(); }

    // Called when a macro is dropped on this knob: (macroIndex)
    std::function<void (int)> onMacroDropped;
    // Called when an LFO is dropped on this knob: (lfoIndex)
    std::function<void (int)> onLfoDropped;
    // Called on right-click to build a context menu
    std::function<void (juce::PopupMenu&)> onBuildContextMenu;
    // Called when a context menu item is selected
    std::function<void (int)> onContextMenuResult;

private:
    juce::Slider slider;
    juce::String label, suffix = "%";
    juce::Colour arcColour;
    float defaultValue = 0.0f;
    float modDepth = 0.0f;  // 0..1 normalised modulation depth for blue halo
    bool showPercentage = true;
    bool macroDragHover = false;

    // Halo ring drag state
    bool draggingModDepth = false;
    float modDragStartDepth = 0.0f;
    float modDragStartY = 0.0f;

    // Halo geometry helpers
    void getArcGeometry (float& cx, float& cy, float& radius,
                         float& startAngle, float& arcRange) const;
    bool hitTestHalo (float mx, float my) const;

    void mouseDown (const juce::MouseEvent&) override;

    // Transparent overlay that sits on top of the slider to catch halo clicks
    struct HaloOverlay : public juce::Component
    {
        ArcKnob& owner;
        HaloOverlay (ArcKnob& o) : owner (o) { setInterceptsMouseClicks (true, false); }
        bool hitTest (int x, int y) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
    };
    HaloOverlay haloOverlay { *this };
};
