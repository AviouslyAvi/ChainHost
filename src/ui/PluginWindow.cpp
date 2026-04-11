#include "PluginWindow.h"

PluginWindow::PluginWindow (const juce::String& name, juce::AudioProcessorGraph::NodeID nid,
                            juce::AudioProcessorEditor* editor,
                            std::function<void (juce::AudioProcessorGraph::NodeID)> onClose)
    : DocumentWindow (name, Colors::surface, DocumentWindow::closeButton),
      nodeId (nid), closeCallback (std::move (onClose))
{
    setContentOwned (editor, true);
    setResizable (false, false);
    setAlwaysOnTop (true);
    setDropShadowEnabled (true);
    centreWithSize (editor->getWidth(), editor->getHeight());
    setVisible (true);
    toFront (true);
}

void PluginWindow::closeButtonPressed()
{
    clearContentComponent();
    setVisible (false);
    if (closeCallback) closeCallback (nodeId);
}
