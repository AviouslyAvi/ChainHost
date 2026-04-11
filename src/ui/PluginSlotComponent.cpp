#include "PluginSlotComponent.h"
#include "../PluginProcessor.h"

PluginSlotComponent::PluginSlotComponent (ChainHostProcessor& p, juce::AudioProcessorGraph::NodeID nid, int ci, int si)
    : proc (p), nodeId (nid), chainIndex (ci), slotIndex (si), dryWetKnob ("D/W", Colors::wet)
{
    bool active = !proc.getChainGraph().isSlotBypassed (nodeId);
    bypassButton.setToggleState (active, juce::dontSendNotification);
    bypassButton.setClickingTogglesState (true);
    bypassButton.setButtonText (active ? "ON" : "OFF");
    bypassButton.setColour (juce::TextButton::buttonColourId, active ? Colors::bypass.withAlpha (0.6f) : Colors::surfaceRaised);
    bypassButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, Colors::bypass.withAlpha (0.6f));
    bypassButton.setColour (juce::TextButton::textColourOnId, Colors::text);
    bypassButton.onClick = [this]() {
        bool nowActive = bypassButton.getToggleState();
        proc.getChainGraph().setSlotBypassed (proc.getGraph(), nodeId, !nowActive);
        bypassButton.setButtonText (nowActive ? "ON" : "OFF");
        bypassButton.setColour (juce::TextButton::buttonColourId, nowActive ? Colors::bypass.withAlpha (0.6f) : Colors::surfaceRaised);
        accentGlow = nowActive
            ? melatonin::DropShadow { { Colors::accent.withAlpha (0.25f), 10, { 0, 0 }, 2 } }
            : melatonin::DropShadow {};
        repaint();
    };
    addAndMakeVisible (bypassButton);

    if (active)
        accentGlow = melatonin::DropShadow { { Colors::accent.withAlpha (0.25f), 10, { 0, 0 }, 2 } };

    juce::String pluginName = "?";
    if (auto* node = proc.getGraph().getNodeForId (nodeId))
        pluginName = node->getProcessor()->getName();
    nameLabel.setText (pluginName, juce::dontSendNotification);
    nameLabel.setColour (juce::Label::textColourId, Colors::text);
    nameLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (nameLabel);

    dryWetKnob.setValue (proc.getChainGraph().getSlotDryWet (nodeId), false);
    dryWetKnob.setDefaultValue (1.0f);
    dryWetKnob.onValueChange = [this]() {
        proc.getChainGraph().setSlotDryWet (proc.getGraph(), nodeId, dryWetKnob.getValue());
    };
    addAndMakeVisible (dryWetKnob);

    removeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeButton.setColour (juce::TextButton::textColourOffId, Colors::remove.withAlpha (0.7f));
    removeButton.onClick = [this]() { if (onRemove) onRemove (nodeId); };
    addAndMakeVisible (removeButton);
}

void PluginSlotComponent::paint (juce::Graphics& g)
{
    bool isBypassed = proc.getChainGraph().isSlotBypassed (nodeId);
    auto bounds = getLocalBounds().toFloat().reduced (2);

    g.setColour (isBypassed ? Colors::slotBg : Colors::surfaceRaised);
    g.fillRect (bounds);

    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder);
    g.drawRect (bounds, 1.0f);

    if (!isBypassed)
    {
        g.setColour (Colors::accent);
        g.fillRect (bounds.getX(), bounds.getY(), 3.0f, bounds.getHeight());
    }

    if (dragHover)
    {
        g.setColour (Colors::accent.withAlpha (0.1f));
        g.fillRect (bounds);
        g.setColour (Colors::accent.withAlpha (0.6f));
        g.drawRect (bounds, 2.0f);
    }

    g.setColour (Colors::textDim.withAlpha (0.2f));
    for (int dy = 0; dy < 3; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            g.fillRect (bounds.getX() + 5 + dx * 4, bounds.getY() + 26 + dy * 4, 2.0f, 2.0f);
}

void PluginSlotComponent::resized()
{
    auto b = getLocalBounds().reduced (4);
    bypassButton.setBounds (b.getX() + 6, b.getY() + 4, 28, 16);
    nameLabel.setBounds (b.getX() + 38, b.getY() + 4, b.getWidth() - 60, 18);
    removeButton.setBounds (b.getRight() - 22, b.getY() + 3, 20, 18);
    dryWetKnob.setBounds (b.getRight() - 56, b.getY() + 22, 50, 54);
}

void PluginSlotComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < 22 && e.y > 24)
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
            dc->startDragging (juce::var (juce::String (chainIndex) + ":" + juce::String (slotIndex)), this);
}

void PluginSlotComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onOpenEditor) onOpenEditor (nodeId);
}

void PluginSlotComponent::itemDropped (const SourceDetails& details)
{
    dragHover = false; repaint();
    auto parts = juce::StringArray::fromTokens (details.description.toString(), ":", "");
    if (parts.size() == 2 && onMove)
        onMove (parts[0].getIntValue(), parts[1].getIntValue(), chainIndex, slotIndex);
}
