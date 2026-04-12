#include "PluginWindow.h"

PluginWindow::PluginWindow (const juce::String& name, juce::AudioProcessorGraph::NodeID nid,
                            juce::AudioProcessorEditor* editor,
                            juce::Component* parentComp,
                            std::function<void (juce::AudioProcessorGraph::NodeID)> onClose)
    : DocumentWindow (name, Colors::surface, DocumentWindow::closeButton),
      nodeId (nid), closeCallback (std::move (onClose))
{
    juce::ignoreUnused (parentComp);
    setContentOwned (editor, true);
    setResizable (false, false);
    setDropShadowEnabled (true);
    setAlwaysOnTop (true);
    centreWithSize (editor->getWidth(), editor->getHeight());
    addToDesktop (juce::ComponentPeer::windowIsTemporary
                | juce::ComponentPeer::windowHasTitleBar
                | juce::ComponentPeer::windowHasCloseButton);
    setVisible (true);
    toFront (true);
}

void PluginWindow::closeButtonPressed()
{
    if (closeCallback) closeCallback (nodeId);
    clearContentComponent();
    setVisible (false);
}
