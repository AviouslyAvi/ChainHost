#pragma once
// PluginWindow — floating DocumentWindow for hosted plugin editors
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Colors.h"

class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (const juce::String& name, juce::AudioProcessorGraph::NodeID nid,
                  juce::AudioProcessorEditor* editor,
                  std::function<void (juce::AudioProcessorGraph::NodeID)> onClose);
    void closeButtonPressed() override;
    juce::AudioProcessorGraph::NodeID getNodeId() const { return nodeId; }
private:
    juce::AudioProcessorGraph::NodeID nodeId;
    std::function<void (juce::AudioProcessorGraph::NodeID)> closeCallback;
};
