#include "PluginWindow.h"

#if __APPLE__
extern "C" void attachChildWindowToParent (void* childHandle, void* parentHandle);
#endif

PluginWindow::PluginWindow (const juce::String& name, juce::AudioProcessorGraph::NodeID nid,
                            juce::AudioProcessorEditor* editor,
                            juce::Component* parentComp,
                            std::function<void (juce::AudioProcessorGraph::NodeID)> onClose)
    : DocumentWindow (name, Colors::surface, DocumentWindow::closeButton),
      nodeId (nid), closeCallback (std::move (onClose))
{
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

   #if __APPLE__
    if (parentComp != nullptr)
        if (auto* parentPeer = parentComp->getPeer())
            if (auto* childPeer = getPeer())
                attachChildWindowToParent (childPeer->getNativeHandle(),
                                           parentPeer->getNativeHandle());
   #else
    juce::ignoreUnused (parentComp);
   #endif
}

void PluginWindow::closeButtonPressed()
{
    if (closeCallback) closeCallback (nodeId);
    clearContentComponent();
    setVisible (false);
}
