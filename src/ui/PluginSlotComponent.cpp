#include "PluginSlotComponent.h"
#include "../PluginProcessor.h"

PluginSlotComponent::PluginSlotComponent (ChainHostProcessor& p, juce::AudioProcessorGraph::NodeID nid, int ci, int si)
    : proc (p), nodeId (nid), chainIndex (ci), slotIndex (si), dryWetKnob ("D/W", Colors::wet)
{
    bool active = !proc.getChainGraph().isSlotBypassed (nodeId);

    // V1 Candlelight: circular power dot. The TextButton has empty text and
    // transparent fill — the dot itself is drawn procedurally in paintFull().
    powerButton.setToggleState (active, juce::dontSendNotification);
    powerButton.setClickingTogglesState (true);
    powerButton.setButtonText ({});
    powerButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    powerButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    powerButton.onClick = [this]() {
        bool nowActive = powerButton.getToggleState();
        proc.getChainGraph().setSlotBypassed (proc.getGraph(), nodeId, !nowActive);
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
    constexpr float corner = 4.0f;
    auto fullBounds = getLocalBounds().toFloat().reduced (2);

    // Card background (rounded)
    g.setColour (isBypassed ? Colors::bgDeep : Colors::surfaceRaised);
    g.fillRoundedRectangle (fullBounds, corner);

    // Header bar — clipped to card rounding via full-card path
    {
        juce::Path headerClip;
        headerClip.addRoundedRectangle (fullBounds.getX(), fullBounds.getY(),
                                        fullBounds.getWidth(), (float) headerHeight,
                                        corner, corner, true, true, false, false);
        g.setColour (isBypassed ? Colors::bgDeep.brighter (0.04f) : Colors::surface);
        g.fillPath (headerClip);
    }

    // Header bottom hairline
    g.setColour (Colors::borderSubtle.withAlpha (0.6f));
    g.drawLine (fullBounds.getX() + 1.0f, fullBounds.getY() + headerHeight,
                fullBounds.getRight() - 1.0f, fullBounds.getY() + headerHeight, 1.0f);

    // Card border
    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder);
    g.drawRoundedRectangle (fullBounds, corner, 1.0f);

    // Active accent stripe on left (2px, rounded at top-left/bottom-left)
    if (!isBypassed)
    {
        juce::Path stripe;
        stripe.addRoundedRectangle (fullBounds.getX(), fullBounds.getY(), 2.0f, fullBounds.getHeight(),
                                    corner, corner, true, false, true, false);
        g.setColour (Colors::accent);
        g.fillPath (stripe);
    }

    // Circular power LED — painted over the invisible powerButton
    auto pb = powerButton.getBounds().toFloat();
    const float dotR = 4.0f;
    float dotCx = pb.getCentreX();
    float dotCy = pb.getCentreY();
    if (!isBypassed)
    {
        g.setColour (Colors::accent.withAlpha (0.35f));
        g.fillEllipse (dotCx - dotR - 2.0f, dotCy - dotR - 2.0f, (dotR + 2.0f) * 2.0f, (dotR + 2.0f) * 2.0f);
        g.setColour (Colors::accent);
        g.fillEllipse (dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);
    }
    else
    {
        g.setColour (Colors::textDim);
        g.drawEllipse (dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f, 1.0f);
    }
}

void PluginSlotComponent::paintCollapsed (juce::Graphics& g, juce::Rectangle<float> bounds, bool isBypassed)
{
    constexpr float corner = 4.0f;

    // Slim vertical strip (rounded)
    g.setColour (isBypassed ? Colors::bgDeep : Colors::surfaceRaised);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    // Active accent stripe at top
    if (!isBypassed)
    {
        juce::Path stripe;
        stripe.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), 2.0f,
                                    corner, corner, true, true, false, false);
        g.setColour (Colors::accent);
        g.fillPath (stripe);
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

    // Header layout: [power dot 14] [name ...] [wrench 18] [remove 16]
    powerButton.setBounds (header.getX() + 4, header.getY() + 2, 14, 14);
    removeButton.setBounds (header.getRight() - 16, header.getY(), 16, 18);
    wrenchButton.setBounds (header.getRight() - 34, header.getY(), 18, 18);
    nameLabel.setBounds (header.getX() + 22, header.getY(), header.getWidth() - 58, 18);

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
