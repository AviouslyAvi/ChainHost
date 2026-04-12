#pragma once
// PluginSlotComponent — single plugin slot card with bypass, dry/wet, drag-and-drop
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <melatonin_blur/melatonin_blur.h>
#include "Colors.h"
#include "ArcKnob.h"

class ChainHostProcessor;

class PluginSlotComponent : public juce::Component, public juce::DragAndDropTarget
{
public:
    PluginSlotComponent (ChainHostProcessor& p, juce::AudioProcessorGraph::NodeID nid, int ci, int si);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override { return true; }
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragHover = false; repaint(); }

    std::function<void (juce::AudioProcessorGraph::NodeID)> onOpenEditor, onRemove;
    std::function<void (int, int, int, int)> onMove;
    juce::AudioProcessorGraph::NodeID getNodeId() const { return nodeId; }

private:
    ChainHostProcessor& proc;
    juce::AudioProcessorGraph::NodeID nodeId;
    int chainIndex, slotIndex;
    bool dragHover = false;
    juce::TextButton bypassButton { "ON" };
    juce::Label nameLabel;
    ArcKnob dryWetKnob;
    juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
    melatonin::DropShadow cardShadow { { juce::Colours::black.withAlpha (0.5f), 8, { 0, 3 } } };
    melatonin::DropShadow accentGlow;
};
