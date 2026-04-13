#include "PluginSlotComponent.h"
#include "../PluginProcessor.h"

PluginSlotComponent::PluginSlotComponent (ChainHostProcessor& p, juce::AudioProcessorGraph::NodeID nid, int ci, int si)
    : proc (p), nodeId (nid), chainIndex (ci), slotIndex (si), dryWetKnob ("D/W", Colors::wet)
{
    bool active = !proc.getChainGraph().isSlotBypassed (nodeId);

    // Orange power button (Ableton-style)
    powerButton.setToggleState (active, juce::dontSendNotification);
    powerButton.setClickingTogglesState (true);
    powerButton.setButtonText (active ? "ON" : "OFF");
    auto powerOn = Colors::accent;   // orange
    powerButton.setColour (juce::TextButton::buttonColourId, active ? powerOn.withAlpha (0.7f) : Colors::surfaceRaised);
    powerButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    powerButton.setColour (juce::TextButton::buttonOnColourId, powerOn.withAlpha (0.7f));
    powerButton.setColour (juce::TextButton::textColourOnId, Colors::text);
    powerButton.onClick = [this]() {
        bool nowActive = powerButton.getToggleState();
        proc.getChainGraph().setSlotBypassed (proc.getGraph(), nodeId, !nowActive);
        powerButton.setButtonText (nowActive ? "ON" : "OFF");
        powerButton.setColour (juce::TextButton::buttonColourId, nowActive ? Colors::accent.withAlpha (0.7f) : Colors::surfaceRaised);
        accentGlow = nowActive
            ? melatonin::DropShadow { { Colors::accent.withAlpha (0.25f), 10, { 0, 0 }, 2 } }
            : melatonin::DropShadow {};
        repaint();
    };
    addAndMakeVisible (powerButton);

    if (active)
        accentGlow = melatonin::DropShadow { { Colors::accent.withAlpha (0.25f), 10, { 0, 0 }, 2 } };

    juce::String pluginName = "?";
    if (auto* node = proc.getGraph().getNodeForId (nodeId))
        pluginName = node->getProcessor()->getName();
    nameLabel.setText (pluginName, juce::dontSendNotification);
    nameLabel.setColour (juce::Label::textColourId, Colors::text);
    nameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (nameLabel);

    // Wrench button — toggles plugin editor window
    wrenchButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    wrenchButton.setColour (juce::TextButton::textColourOffId, Colors::textDim);
    wrenchButton.onClick = [this]() { if (onOpenEditor) onOpenEditor (nodeId); };
    addAndMakeVisible (wrenchButton);

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

void PluginSlotComponent::setCollapsed (bool c)
{
    if (collapsed == c) return;
    collapsed = c;
    powerButton.setVisible (!collapsed);
    wrenchButton.setVisible (!collapsed);
    nameLabel.setVisible (!collapsed);
    dryWetKnob.setVisible (!collapsed);
    removeButton.setVisible (!collapsed);
    resized();
    repaint();
}

juce::Rectangle<int> PluginSlotComponent::getHeaderBounds() const
{
    return getLocalBounds().reduced (2).removeFromTop (headerHeight);
}

void PluginSlotComponent::paint (juce::Graphics& g)
{
    bool isBypassed = proc.getChainGraph().isSlotBypassed (nodeId);
    auto bounds = getLocalBounds().toFloat().reduced (2);

    if (collapsed)
        paintCollapsed (g, bounds, isBypassed);
    else
        paintFull (g, bounds, isBypassed);

    if (dragHover)
    {
        g.setColour (Colors::accent.withAlpha (0.1f));
        g.fillRect (bounds);
        g.setColour (Colors::accent.withAlpha (0.6f));
        g.drawRect (bounds, 2.0f);
    }
}

void PluginSlotComponent::paintFull (juce::Graphics& g, juce::Rectangle<float> bounds, bool isBypassed)
{
    // Card background
    g.setColour (isBypassed ? Colors::slotBg : Colors::surfaceRaised);
    g.fillRect (bounds);

    // Header bar
    auto header = bounds.removeFromTop ((float) headerHeight);
    g.setColour (isBypassed ? Colors::slotBg.brighter (0.05f) : Colors::surface);
    g.fillRect (header);

    // Header bottom line
    g.setColour (Colors::border.withAlpha (0.4f));
    g.drawLine (header.getX(), header.getBottom(), header.getRight(), header.getBottom(), 1.0f);

    // Card border
    auto fullBounds = getLocalBounds().toFloat().reduced (2);
    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder);
    g.drawRect (fullBounds, 1.0f);

    // Active accent stripe on left
    if (!isBypassed)
    {
        g.setColour (Colors::accent);
        g.fillRect (fullBounds.getX(), fullBounds.getY(), 3.0f, fullBounds.getHeight());
    }
}

void PluginSlotComponent::paintCollapsed (juce::Graphics& g, juce::Rectangle<float> bounds, bool isBypassed)
{
    // Slim vertical strip
    g.setColour (isBypassed ? Colors::slotBg : Colors::surfaceRaised);
    g.fillRect (bounds);

    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder);
    g.drawRect (bounds, 1.0f);

    // Active accent stripe at top
    if (!isBypassed)
    {
        g.setColour (Colors::accent);
        g.fillRect (bounds.getX(), bounds.getY(), bounds.getWidth(), 3.0f);
    }

    // On/off dot
    float dotX = bounds.getCentreX();
    float dotY = bounds.getY() + 10.0f;
    g.setColour (isBypassed ? Colors::textDim.withAlpha (0.3f) : Colors::accent);
    g.fillEllipse (dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);

    // Rotated plugin name
    juce::String pluginName = "?";
    if (auto* node = proc.getGraph().getNodeForId (nodeId))
        pluginName = node->getProcessor()->getName();

    g.setColour (isBypassed ? Colors::textDim : Colors::text);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));

    // Draw text rotated 90 degrees (bottom to top)
    g.saveState();
    float textX = bounds.getCentreX();
    float textY = bounds.getY() + 22.0f;
    float textH = bounds.getHeight() - 28.0f;
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                       textX, textY + textH * 0.5f));
    g.drawText (pluginName, (int)(textX - textH * 0.5f), (int)(textY + textH * 0.5f - 6),
                (int)textH, 12, juce::Justification::centred, true);
    g.restoreState();
}

void PluginSlotComponent::resized()
{
    if (collapsed) return;

    auto b = getLocalBounds().reduced (4);
    auto header = b.removeFromTop (headerHeight - 2);

    // Header layout: [power 28x16] [name ...] [wrench 18x16] [remove 16x16]
    powerButton.setBounds (header.getX() + 2, header.getY() + 1, 28, 16);
    removeButton.setBounds (header.getRight() - 16, header.getY(), 16, 18);
    wrenchButton.setBounds (header.getRight() - 34, header.getY(), 18, 18);
    nameLabel.setBounds (header.getX() + 34, header.getY(), header.getWidth() - 70, 18);

    // Body: dry/wet knob
    dryWetKnob.setBounds (b.getRight() - 48, b.getY() + headerHeight - 2, 44, 54);
}

void PluginSlotComponent::mouseDown (const juce::MouseEvent& e)
{
    dragStartPos = e.getPosition();
}

void PluginSlotComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (collapsed)
    {
        // In collapsed mode, drag from anywhere
        if (e.getDistanceFromDragStart() > 6)
            if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! dc->isDragAndDropActive())
                {
                    juce::String prefix = e.mods.isAltDown() ? "copy:" : "";
                    dc->startDragging (juce::var (prefix + juce::String (chainIndex) + ":" + juce::String (slotIndex)), this);
                }
        return;
    }

    // Full mode — don't drag from ON button, remove, wrench, or D/W knob
    bool inPower  = powerButton.getBounds().contains (dragStartPos);
    bool inRemove = removeButton.getBounds().contains (dragStartPos);
    bool inWrench = wrenchButton.getBounds().contains (dragStartPos);
    bool inDryWet = dryWetKnob.getBounds().contains (dragStartPos);

    if (inPower || inRemove || inWrench || inDryWet)
        return;

    if (e.getDistanceFromDragStart() > 6)
    {
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! dc->isDragAndDropActive())
            {
                juce::String prefix = e.mods.isAltDown() ? "copy:" : "";
                dc->startDragging (juce::var (prefix + juce::String (chainIndex) + ":" + juce::String (slotIndex)), this);
            }
        }
    }
}

void PluginSlotComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click on header area (or anywhere in collapsed mode) toggles collapse
    if (collapsed)
    {
        setCollapsed (false);
        if (onLayoutChanged) onLayoutChanged();
        return;
    }

    auto header = getHeaderBounds();
    if (header.contains (e.getPosition()))
    {
        setCollapsed (true);
        if (onLayoutChanged) onLayoutChanged();
        return;
    }

    // Double-click on body opens editor (same as wrench)
    if (onOpenEditor) onOpenEditor (nodeId);
}

void PluginSlotComponent::itemDropped (const SourceDetails& details)
{
    dragHover = false; repaint();
    auto desc = details.description.toString();
    bool isCopy = desc.startsWith ("copy:");
    if (isCopy) desc = desc.fromFirstOccurrenceOf ("copy:", false, false);

    auto parts = juce::StringArray::fromTokens (desc, ":", "");
    if (parts.size() == 2)
    {
        int fc = parts[0].getIntValue(), fs = parts[1].getIntValue();
        if (isCopy && onCopy)
            onCopy (fc, fs, chainIndex, slotIndex);
        else if (! isCopy && onMove)
            onMove (fc, fs, chainIndex, slotIndex);
    }
}
