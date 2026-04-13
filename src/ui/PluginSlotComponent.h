#pragma once
// PluginSlotComponent — Ableton-style plugin slot with full/collapsed toggle
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
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override { return true; }
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragHover = false; repaint(); }

    std::function<void (juce::AudioProcessorGraph::NodeID)> onOpenEditor, onRemove;
    std::function<void (int, int, int, int)> onMove;
    std::function<void (int, int, int, int)> onCopy;
    std::function<void()> onLayoutChanged;  // called when collapsed/expanded to trigger parent relayout
    juce::AudioProcessorGraph::NodeID getNodeId() const { return nodeId; }

    bool isCollapsed() const { return collapsed; }
    void setCollapsed (bool c);
    int getDesiredWidth() const { return collapsed ? collapsedWidth : fullWidth; }

    static constexpr int fullWidth = 140;
    static constexpr int collapsedWidth = 28;

private:
    ChainHostProcessor& proc;
    juce::AudioProcessorGraph::NodeID nodeId;
    int chainIndex, slotIndex;
    bool dragHover = false;
    bool collapsed = false;
    juce::Point<int> dragStartPos;

    // Full-mode controls
    juce::TextButton powerButton { "ON" };
    juce::TextButton wrenchButton { juce::CharPointer_UTF8 ("\xf0\x9f\x94\xa7") }; // wrench emoji
    juce::Label nameLabel;
    ArcKnob dryWetKnob;
    juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };

    melatonin::DropShadow cardShadow { { juce::Colours::black.withAlpha (0.5f), 8, { 0, 3 } } };
    melatonin::DropShadow accentGlow;

    static constexpr int headerHeight = 22;
    juce::Rectangle<int> getHeaderBounds() const;

    void paintFull (juce::Graphics& g, juce::Rectangle<float> bounds, bool isBypassed);
    void paintCollapsed (juce::Graphics& g, juce::Rectangle<float> bounds, bool isBypassed);
};
