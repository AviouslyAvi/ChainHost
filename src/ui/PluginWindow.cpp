#include "PluginWindow.h"

#if JUCE_MAC
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
    centreWithSize (editor->getWidth(), editor->getHeight());
    setVisible (true);
    toFront (true);

#if JUCE_MAC
    if (parentComp != nullptr)
        if (auto* peer = getPeer())
            if (auto* parentPeer = parentComp->getPeer())
                attachChildWindowToParent (peer->getNativeHandle(), parentPeer->getNativeHandle());
#else
    juce::ignoreUnused (parentComp);
    setAlwaysOnTop (true);
#endif
}

void PluginWindow::closeButtonPressed()
{
    clearContentComponent();
    setVisible (false);
    if (closeCallback) closeCallback (nodeId);
}
