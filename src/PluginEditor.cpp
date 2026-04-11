#include "PluginEditor.h"

#ifndef CHAINHOST_VERSION
#define CHAINHOST_VERSION "0.0.0"
#endif

//==============================================================================
// ChainHostLookAndFeel — override Gin's Copper palette with our warm amber
//==============================================================================

ChainHostLookAndFeel::ChainHostLookAndFeel()
{
    using G = gin::GinLookAndFeel;
    setColour (G::backgroundColourId, Colors::bg);
    setColour (G::groupColourId, Colors::surfaceRaised);
    setColour (G::lightGroupColourId, Colors::surfaceHover);
    setColour (G::accentColourId, Colors::accent);
    setColour (G::grey30ColourId, Colors::textDim);
    setColour (G::grey45ColourId, Colors::textMid);
    setColour (G::grey60ColourId, Colors::textMid);
    setColour (G::grey90ColourId, Colors::text);

    setColour (juce::Slider::rotarySliderFillColourId, Colors::accent);
    setColour (juce::Slider::trackColourId, Colors::border);
    setColour (juce::Slider::thumbColourId, Colors::accent);

    setColour (juce::Label::textColourId, Colors::text);
    setColour (juce::TextButton::textColourOnId, Colors::accent);
    setColour (juce::TextButton::textColourOffId, Colors::textMid);

    setColour (juce::PopupMenu::backgroundColourId, Colors::surface);
    setColour (juce::PopupMenu::textColourId, Colors::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colors::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Colors::text);
}

//==============================================================================
// FabKnob — wraps juce::Slider with Gin's CopperLookAndFeel rendering
//==============================================================================

FabKnob::FabKnob (const juce::String& lbl, juce::Colour arcCol)
    : label (lbl), arcColour (arcCol)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 1.0, 0.001);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, arcColour);
    slider.setColour (juce::Slider::trackColourId, Colors::border);
    slider.onValueChange = [this]() { repaint(); if (onValueChange) onValueChange(); };
    addAndMakeVisible (slider);
}

void FabKnob::resized()
{
    auto b = getLocalBounds();
    int labelH = label.isEmpty() ? 0 : 14;
    int valueH = showPercentage ? 12 : 0;
    auto sliderBounds = b.withTrimmedBottom (labelH + valueH);
    // Keep slider square
    int sz = juce::jmin (sliderBounds.getWidth(), sliderBounds.getHeight());
    slider.setBounds (sliderBounds.withSizeKeepingCentre (sz, sz));
}

void FabKnob::paint (juce::Graphics& g)
{
    // Value text below slider
    if (showPercentage)
    {
        float norm = (float) slider.getValue();
        if (slider.getMaximum() > 1.1)
        {
            g.setColour (Colors::text.withAlpha (0.75f));
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            auto valStr = juce::String (slider.getValue(), 2) + suffix;
            g.drawText (valStr, 0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
        else
        {
            g.setColour (Colors::text.withAlpha (0.75f));
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (juce::String (juce::roundToInt (norm * 100.0f)) + suffix,
                        0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
    }

    if (label.isNotEmpty())
    {
        g.setColour (Colors::textDim.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (7.5f).withStyle ("Bold")));
        g.drawText (label.toUpperCase(), 0, getHeight() - 11, getWidth(), 11, juce::Justification::centred);
    }

    // Drop hover — LFO assignment glow
    if (dropHover)
    {
        g.setColour (Colors::lfoBlue.withAlpha (0.12f));
        g.fillRect (getLocalBounds());
        g.setColour (Colors::lfoBlue.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 2);
        // "LFO" label overlay
        g.setColour (Colors::lfoBlue.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
        g.drawText ("LFO", getLocalBounds().removeFromTop (16), juce::Justification::centred);
    }
}

void FabKnob::setValue (float v, bool notify)
{
    slider.setValue (v, notify ? juce::sendNotificationAsync : juce::dontSendNotification);
    if (! notify) repaint();  // still update visuals when skipping callbacks
}

float FabKnob::getValue() const { return (float) slider.getValue(); }

void FabKnob::setRange (float mn, float mx)
{
    slider.setRange (mn, mx, (mx - mn) < 2.0f ? 0.001 : 0.01);
}

void FabKnob::setArcColour (juce::Colour c)
{
    arcColour = c;
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    repaint();
}

bool FabKnob::isInterestedInDragSource (const SourceDetails&)
{
    return onDrop != nullptr;
}

void FabKnob::itemDropped (const SourceDetails& details)
{
    dropHover = false;
    repaint();
    if (onDrop) onDrop (details.description);
}

//==============================================================================
// PluginWindow
//==============================================================================

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

//==============================================================================
// PluginSlotComponent — with melatonin blur shadows
//==============================================================================

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
        // Update glow
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

    // Slot background with subtle top-to-bottom gradient
    auto bgTop = isBypassed ? Colors::slotBg : Colors::surfaceRaised;
    auto bgBot = isBypassed ? Colors::slotBg.darker (0.1f) : Colors::surfaceRaised.darker (0.15f);
    g.setGradientFill (juce::ColourGradient (bgTop, 0, bounds.getY(), bgBot, 0, bounds.getBottom(), false));
    g.fillRect (bounds);

    // Border — brighter when active
    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder.brighter (0.1f));
    g.drawRect (bounds, 1.0f);

    // Left accent bar — glowing when active
    if (! isBypassed)
    {
        g.setColour (Colors::accent);
        g.fillRect (bounds.getX(), bounds.getY(), 3.0f, bounds.getHeight());
        // Soft glow bleed from accent bar
        g.setGradientFill (juce::ColourGradient (
            Colors::accent.withAlpha (0.08f), bounds.getX() + 3, bounds.getCentreY(),
            Colors::accent.withAlpha (0.0f), bounds.getX() + 20, bounds.getCentreY(), false));
        g.fillRect (bounds.getX() + 3, bounds.getY(), 17.0f, bounds.getHeight());
    }

    // Bordered square around ON button
    auto btnBounds = bypassButton.getBounds().toFloat().expanded (2);
    g.setColour (isBypassed ? Colors::borderSubtle : Colors::bypass.withAlpha (0.3f));
    g.drawRect (btnBounds, 1.0f);
    if (! isBypassed)
    {
        g.setColour (Colors::bypass.withAlpha (0.06f));
        g.fillRect (btnBounds);
    }

    // Drop hover
    if (dragHover)
    {
        g.setColour (Colors::accent.withAlpha (0.08f));
        g.fillRect (bounds);
        g.setColour (Colors::accent.withAlpha (0.5f));
        g.drawRect (bounds, 2.0f);
    }

    // Drag grip dots
    g.setColour (Colors::textDim.withAlpha (0.18f));
    for (int dy = 0; dy < 3; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            g.fillRect (bounds.getX() + 5 + dx * 4, bounds.getY() + 26 + dy * 4, 2.0f, 2.0f);
}

void PluginSlotComponent::resized()
{
    auto b = getLocalBounds().reduced (2);
    bypassButton.setBounds (b.getX() + 4, b.getY() + 4, 26, 14);
    nameLabel.setBounds (b.getX() + 34, b.getY() + 3, b.getWidth() - 54, 16);
    removeButton.setBounds (b.getRight() - 18, b.getY() + 2, 16, 16);
    dryWetKnob.setBounds (b.getRight() - 48, b.getY() + 20, 44, 44);
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

//==============================================================================
// PresetBrowser
//==============================================================================

PresetBrowser::PresetBrowser (ChainHostProcessor& p) : proc (p)
{
    listBox.setColour (juce::ListBox::backgroundColourId, Colors::bgDeep);
    listBox.setColour (juce::ListBox::outlineColourId, Colors::border);
    listBox.setRowHeight (22);
    addAndMakeVisible (listBox);

    nameEditor.setColour (juce::TextEditor::backgroundColourId, Colors::surfaceRaised);
    nameEditor.setColour (juce::TextEditor::textColourId, Colors::text);
    nameEditor.setColour (juce::TextEditor::outlineColourId, Colors::border);
    nameEditor.setTextToShowWhenEmpty ("Preset name...", Colors::textDim);
    nameEditor.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (nameEditor);

    saveButton.setColour (juce::TextButton::buttonColourId, Colors::accent.withAlpha (0.5f));
    saveButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    saveButton.onClick = [this]() {
        auto name = nameEditor.getText().trim();
        if (name.isEmpty()) return;
        auto file = ChainHostProcessor::getPresetsDirectory().getChildFile (name + ".chainhost");
        juce::MemoryBlock data;
        proc.getStateInformation (data);
        file.replaceWithData (data.getData(), data.getSize());
        nameEditor.clear();
        refresh();
    };
    addAndMakeVisible (saveButton);

    deleteButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.4f));
    deleteButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    deleteButton.onClick = [this]() {
        if (selectedRow >= 0 && selectedRow < presetFiles.size()) {
            presetFiles[selectedRow].deleteFile();
            selectedRow = -1;
            refresh();
        }
    };
    addAndMakeVisible (deleteButton);
    refresh();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bgDeep);
    g.setColour (Colors::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("PRESET BROWSER", 10, 4, 120, 14, juce::Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (18);
    auto bottom = r.removeFromBottom (28);
    nameEditor.setBounds (bottom.removeFromLeft (bottom.getWidth() - 110));
    bottom.removeFromLeft (4);
    saveButton.setBounds (bottom.removeFromLeft (50));
    bottom.removeFromLeft (4);
    deleteButton.setBounds (bottom);
    r.removeFromBottom (4);
    listBox.setBounds (r);
}

void PresetBrowser::refresh()
{
    presetNames.clear(); presetFiles.clear();
    for (auto& f : ChainHostProcessor::getPresetsDirectory().findChildFiles (juce::File::findFiles, false, "*.chainhost"))
    {
        presetFiles.add (f);
        presetNames.add (f.getFileNameWithoutExtension());
    }
    listBox.updateContent(); listBox.repaint();
}

int PresetBrowser::getNumRows() { return presetNames.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (selected) { g.setColour (Colors::accent.withAlpha (0.15f)); g.fillRect (0, 0, w, h); }
    g.setColour (selected ? Colors::accent : Colors::text);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (presetNames[row], 8, 0, w - 16, h, juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    selectedRow = row;
    if (row >= 0 && row < presetFiles.size())
    {
        juce::MemoryBlock data;
        if (presetFiles[row].loadFileAsData (data))
        {
            proc.setStateInformation (data.getData(), (int)data.getSize());
            if (onPresetLoaded) onPresetLoaded();
        }
    }
}

//==============================================================================
// MappingPanel
//==============================================================================

MappingPanel::MappingPanel (ChainHostProcessor& p) : proc (p)
{
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        macroSelectButtons[i].setButtonText (juce::String (i + 1));
        macroSelectButtons[i].setColour (juce::TextButton::textColourOffId, Colors::text);
        macroSelectButtons[i].onClick = [this, i]() { setSelectedMacro (i); };
        addAndMakeVisible (macroSelectButtons[i]);
    }
    mapButton.setColour (juce::TextButton::buttonColourId, Colors::accent.withAlpha (0.4f));
    mapButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    mapButton.onClick = [this]() { showParameterPicker(); };
    addAndMakeVisible (mapButton);

    learnButton.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
    learnButton.setColour (juce::TextButton::textColourOffId, Colors::learn);
    learnButton.onClick = [this]() { isLearning() ? stopLearn() : startLearn (selectedMacro); };
    addAndMakeVisible (learnButton);
    setSelectedMacro (0);
}

void MappingPanel::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bgDeep);

    // Section label with accent dot
    g.setColour (Colors::accent.withAlpha (0.5f));
    g.fillRect (6, 10, 3, 3);
    g.setColour (Colors::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (8.0f).withStyle ("Bold")));
    g.drawText ("MACRO MAPPING", 12, 6, 100, 12, juce::Justification::centredLeft);

    if (isLearning())
    {
        // Pulsing learn indicator
        g.setColour (Colors::learn.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText ("Move a parameter on any loaded plugin...", 10, 50, getWidth() - 20, 16, juce::Justification::centredLeft);
        // Animated border flash (subtle)
        g.setColour (Colors::learn.withAlpha (0.15f));
        g.drawRect (getLocalBounds().reduced (2), 1);
    }
    else if (mappingRows.isEmpty())
    {
        g.setColour (Colors::textDim.withAlpha (0.3f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("No mappings. Click + MAP or LEARN to assign.", 10, 50, getWidth() - 20, 16, juce::Justification::centredLeft);
    }
}

void MappingPanel::resized()
{
    int x = 110;
    for (int i = 0; i < MacroManager::numMacros; ++i) { macroSelectButtons[i].setBounds (x, 4, 24, 16); x += 27; }
    mapButton.setBounds (x + 6, 3, 50, 18);
    learnButton.setBounds (x + 62, 3, 54, 18);
    int y = 68;
    for (auto* row : mappingRows) {
        row->label.setBounds (12, y, 210, 16);
        row->minKnob.setBounds (230, y - 8, 44, 36);
        row->maxKnob.setBounds (290, y - 8, 44, 36);
        row->removeButton.setBounds (344, y, 16, 16);
        y += 38;
    }
}

void MappingPanel::setSelectedMacro (int i)
{
    selectedMacro = juce::jlimit (0, MacroManager::numMacros - 1, i);
    for (int j = 0; j < MacroManager::numMacros; ++j)
        macroSelectButtons[j].setColour (juce::TextButton::buttonColourId,
            j == selectedMacro ? Colors::accent.withAlpha (0.55f) : Colors::surfaceRaised);
    refresh();
}

void MappingPanel::refresh()
{
    mappingRows.clear();
    for (auto& m : proc.getMacroManager().getMappings (selectedMacro))
    {
        auto* row = mappingRows.add (new MappingRow());
        row->slotUid = m.slotUid; row->paramIndex = m.paramIndex;
        juce::String pName = "?", plugName = "?";
        auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
        if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
            plugName = node->getProcessor()->getName();
            auto& params = node->getProcessor()->getParameters();
            if (m.paramIndex < params.size()) pName = params[m.paramIndex]->getName (28);
        }
        row->label.setText (plugName + "  >  " + pName, juce::dontSendNotification);
        row->label.setColour (juce::Label::textColourId, Colors::text);
        row->label.setFont (juce::Font (juce::FontOptions (9.5f)));
        addAndMakeVisible (row->label);

        row->minKnob.setValue (m.minValue, false); row->minKnob.setShowPercentage (true);
        row->maxKnob.setValue (m.maxValue, false); row->maxKnob.setShowPercentage (true);
        row->minKnob.onValueChange = row->maxKnob.onValueChange =
            [this, uid = m.slotUid, pi = m.paramIndex, &r = *row]() {
                proc.getMacroManager().removeMapping (selectedMacro, uid, pi);
                proc.getMacroManager().addMapping (selectedMacro, uid, pi, r.minKnob.getValue(), r.maxKnob.getValue());
                if (onMappingChanged) onMappingChanged();
            };
        addAndMakeVisible (row->minKnob); addAndMakeVisible (row->maxKnob);

        row->removeButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        row->removeButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        row->removeButton.onClick = [this, uid = m.slotUid, pi = m.paramIndex]() {
            proc.getMacroManager().removeMapping (selectedMacro, uid, pi);
            refresh(); if (onMappingChanged) onMappingChanged();
        };
        addAndMakeVisible (row->removeButton);
    }
    resized(); repaint();
}

void MappingPanel::startLearn (int mi) {
    learningMacroIndex = mi;
    learnButton.setButtonText ("...");
    learnButton.setColour (juce::TextButton::buttonColourId, Colors::learn.withAlpha (0.5f));
    takeParamSnapshot(); repaint();
}
void MappingPanel::stopLearn() {
    learningMacroIndex = -1;
    learnButton.setButtonText ("LEARN");
    learnButton.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
    learnSnapshot.clear(); repaint();
}

void MappingPanel::takeParamSnapshot() {
    learnSnapshot.clear();
    auto& cg = proc.getChainGraph();
    for (int ci = 0; ci < cg.getNumChains(); ++ci)
        for (auto& slot : cg.getChain (ci).slots)
            if (auto* node = proc.getGraph().getNodeForId (slot.nodeId)) {
                auto& params = node->getProcessor()->getParameters();
                for (int pi = 0; pi < params.size(); ++pi)
                    learnSnapshot.push_back ({ slot.uid, pi, params[pi]->getValue() });
            }
}

void MappingPanel::checkLearn() {
    if (learningMacroIndex < 0) return;

    // Build a set of parameters currently modulated by LFOs or macros so we can ignore them
    auto isModulated = [&] (const juce::String& slotUid, int paramIndex) -> bool
    {
        // Check LFO targets
        auto& lfoEngine = proc.getLfoEngine();
        for (int li = 0; li < LfoEngine::numLfos; ++li)
        {
            auto& lfo = lfoEngine.getLfo (li);
            if (! lfo.enabled) continue;
            for (auto& t : lfo.targets)
            {
                if (t.type == LfoTarget::Parameter && t.slotUid == slotUid && t.paramIndex == paramIndex)
                    return true;
                // If LFO targets a macro, check that macro's mappings too
                if (t.type == LfoTarget::Macro)
                {
                    for (auto& m : proc.getMacroManager().getMappings (t.macroIndex))
                        if (m.slotUid == slotUid && m.paramIndex == paramIndex)
                            return true;
                }
            }
        }
        // Check all macro mappings (macros may be automated by host or other sources)
        for (int mi = 0; mi < MacroManager::numMacros; ++mi)
            for (auto& m : proc.getMacroManager().getMappings (mi))
                if (m.slotUid == slotUid && m.paramIndex == paramIndex)
                    return true;
        return false;
    };

    float bestDelta = 0; juce::String bestUid; int bestParam = -1;
    for (auto& snap : learnSnapshot)
    {
        auto nodeId = proc.getChainGraph().getNodeIdForUid (snap.slotUid);
        if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
            if (isModulated (snap.slotUid, snap.paramIndex)) continue;
            auto& params = node->getProcessor()->getParameters();
            if (snap.paramIndex < params.size()) {
                float d = std::abs (params[snap.paramIndex]->getValue() - snap.value);
                if (d > bestDelta) { bestDelta = d; bestUid = snap.slotUid; bestParam = snap.paramIndex; }
            }
        }
    }
    if (bestDelta > 0.05f && bestParam >= 0) {
        proc.getMacroManager().addMapping (learningMacroIndex, bestUid, bestParam, 0.0f, 1.0f);
        setSelectedMacro (learningMacroIndex); stopLearn(); refresh();
        if (onMappingChanged) onMappingChanged();
    }
}

void MappingPanel::showParameterPicker() {
    juce::PopupMenu menu; auto& cg = proc.getChainGraph();
    int itemId = 1;
    struct PI { juce::String uid; int p; };
    std::vector<PI> lookup;
    for (int ci = 0; ci < cg.getNumChains(); ++ci)
        for (auto& slot : cg.getChain (ci).slots)
            if (auto* node = proc.getGraph().getNodeForId (slot.nodeId)) {
                auto& params = node->getProcessor()->getParameters();
                if (params.isEmpty()) continue;
                juce::PopupMenu sub;
                for (int pi = 0; pi < params.size(); ++pi) { lookup.push_back ({slot.uid, pi}); sub.addItem (itemId++, params[pi]->getName (40)); }
                auto lbl = node->getProcessor()->getName();
                if (cg.getNumChains() > 1) lbl = "Chain " + juce::String (ci + 1) + " > " + lbl;
                menu.addSubMenu (lbl, sub);
            }
    if (itemId == 1) menu.addItem (-1, "(No plugins)", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mapButton),
        [this, lookup] (int r) { if (r > 0 && r <= (int)lookup.size()) {
            proc.getMacroManager().addMapping (selectedMacro, lookup[(size_t)(r-1)].uid, lookup[(size_t)(r-1)].p, 0.0f, 1.0f);
            refresh(); if (onMappingChanged) onMappingChanged(); }});
}

//==============================================================================
// LfoPanel — Serum-style tabbed LFO display
//==============================================================================

LfoPanel::LfoPanel (ChainHostProcessor& p) : proc (p)
{
    for (int i = 0; i < LfoEngine::numLfos; ++i)
    {
        lfoTabs[i].setButtonText ("LFO " + juce::String (i + 1));
        lfoTabs[i].setColour (juce::TextButton::textColourOffId, Colors::text);
        lfoTabs[i].onClick = [this, i]() { setActiveLfo (i); };
        addAndMakeVisible (lfoTabs[i]);
    }

    enableButton.setClickingTogglesState (true);
    enableButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    enableButton.setColour (juce::TextButton::textColourOnId, Colors::text);
    enableButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.enabled = enableButton.getToggleState();
        enableButton.setButtonText (lfo.enabled ? "ON" : "OFF");
        enableButton.setColour (juce::TextButton::buttonColourId, lfo.enabled ? Colors::lfoBlue.withAlpha (0.5f) : Colors::surfaceRaised);
        enableButton.setColour (juce::TextButton::buttonOnColourId, Colors::lfoBlue.withAlpha (0.5f));
    };
    addAndMakeVisible (enableButton);

    for (int s = 0; s < LfoEngine::NumShapes; ++s)
    {
        shapeButtons[s].setButtonText (LfoEngine::shapeName ((LfoEngine::Shape) s));
        shapeButtons[s].setColour (juce::TextButton::textColourOffId, Colors::text);
        shapeButtons[s].onClick = [this, s]() {
            proc.getLfoEngine().getLfo (activeLfo).shape = (LfoEngine::Shape) s;
            for (int j = 0; j < LfoEngine::NumShapes; ++j)
                shapeButtons[j].setColour (juce::TextButton::buttonColourId,
                    j == s ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);
        };
        addAndMakeVisible (shapeButtons[s]);
    }

    rateKnob.setRange (LfoEngine::kMinRate, LfoEngine::kMaxRate);
    rateKnob.setDefaultValue (1.0f);
    rateKnob.setSuffix (" Hz");
    rateKnob.setShowPercentage (false);
    rateKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).rate = rateKnob.getValue(); };
    addAndMakeVisible (rateKnob);

    depthKnob.setDefaultValue (0.5f);
    depthKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).depth = depthKnob.getValue(); };
    addAndMakeVisible (depthKnob);

    // BPM/Hz toggle — prominent mode switcher
    syncToggle.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
    syncToggle.setColour (juce::TextButton::textColourOffId, Colors::lfoBlue);
    syncToggle.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.syncToHost = !lfo.syncToHost;
        syncToggle.setButtonText (lfo.syncToHost ? "SYNC" : "FREE");
        syncToggle.setColour (juce::TextButton::buttonColourId,
            lfo.syncToHost ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);
        rateKnob.setVisible (!lfo.syncToHost);
        divButton.setVisible (lfo.syncToHost);
    };
    addAndMakeVisible (syncToggle);

    // Note division button (visible in BPM mode)
    divButton.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.25f));
    divButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    divButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        juce::PopupMenu menu;
        for (int d = 0; d < LfoEngine::numDivisions; ++d)
            menu.addItem (d + 1, LfoEngine::divisionName (LfoEngine::divisions[d]),
                          true, LfoEngine::divisions[d] == lfo.noteDivision);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (divButton),
            [this] (int r) {
                if (r > 0) {
                    auto& l = proc.getLfoEngine().getLfo (activeLfo);
                    l.noteDivision = LfoEngine::divisions[r - 1];
                    divButton.setButtonText (LfoEngine::divisionName (l.noteDivision));
                }
            });
    };
    divButton.setVisible (false);
    addAndMakeVisible (divButton);

    addTargetButton.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.3f));
    addTargetButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    addTargetButton.onClick = [this]() { showTargetMenu (activeLfo); };
    addAndMakeVisible (addTargetButton);

    dragHandle.activeLfoPtr = &activeLfo;
    addAndMakeVisible (dragHandle);

    refresh();
}

void LfoPanel::LfoDragHandle::paint (juce::Graphics& g)
{
    g.setColour (Colors::lfoBlue.withAlpha (0.4f));
    g.fillRect (getLocalBounds());
    g.setColour (Colors::lfoBlue.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
    g.setColour (Colors::text);
    g.setFont (juce::Font (juce::FontOptions (8.0f)));
    g.drawText (juce::CharPointer_UTF8 ("DRAG \xe2\x86\x92 MACRO"), getLocalBounds(), juce::Justification::centred);
}

void LfoPanel::LfoDragHandle::mouseDown (const juce::MouseEvent&) {}

void LfoPanel::LfoDragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 4 && activeLfoPtr != nullptr)
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
            dc->startDragging (juce::var ("lfo:" + juce::String (*activeLfoPtr)), this);
}

void LfoPanel::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bgDeep);

    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);

    // ── Waveform display ─────────────────────────────────────────
    auto wfBounds = juce::Rectangle<int> (10, 28, getWidth() / 2 - 140, 56);

    // Dark inset background with subtle vignette
    {
        g.setColour (juce::Colour (0xff08080f));
        g.fillRect (wfBounds);
        // Top-edge inner shadow
        g.setGradientFill (juce::ColourGradient (
            juce::Colour (0x18000000), 0, (float) wfBounds.getY(),
            juce::Colours::transparentBlack, 0, (float) wfBounds.getY() + 6, false));
        g.fillRect (wfBounds);
    }

    // Grid lines — fine crosshatch
    g.setColour (juce::Colour (0x0affffff));
    float midY = (float) wfBounds.getCentreY();
    g.drawLine ((float) wfBounds.getX(), midY, (float) wfBounds.getRight(), midY, 0.5f);
    // Quarter + eighth grid
    for (int q = 1; q <= 7; ++q)
    {
        float qx = wfBounds.getX() + (wfBounds.getWidth() * q) / 8.0f;
        g.setColour (juce::Colour ((q % 2 == 0) ? 0x0affffff : 0x05ffffff));
        g.drawLine (qx, (float) wfBounds.getY(), qx, (float) wfBounds.getBottom(), 0.5f);
    }

    // Waveform rendering — two-pass: fill then stroke
    {
        juce::Path wave;
        for (int x = 0; x <= wfBounds.getWidth(); ++x)
        {
            float phase = (float) x / (float) wfBounds.getWidth();
            float val = LfoEngine::waveformAt (lfo.shape, phase, 0.5f);
            float py = wfBounds.getY() + (1.0f - val) * wfBounds.getHeight();
            if (x == 0) wave.startNewSubPath ((float) (wfBounds.getX() + x), py);
            else        wave.lineTo ((float) (wfBounds.getX() + x), py);
        }

        // Filled area under curve
        juce::Path fillPath (wave);
        fillPath.lineTo ((float) wfBounds.getRight(), (float) wfBounds.getBottom());
        fillPath.lineTo ((float) wfBounds.getX(), (float) wfBounds.getBottom());
        fillPath.closeSubPath();

        auto fillCol = lfo.enabled ? Colors::lfoBlue : Colors::textDim;
        g.setGradientFill (juce::ColourGradient (
            fillCol.withAlpha (0.28f), 0, (float) wfBounds.getY(),
            fillCol.withAlpha (0.0f),  0, (float) wfBounds.getBottom(), false));
        g.fillPath (fillPath);

        // Bright waveform stroke
        g.setColour (lfo.enabled ? Colors::lfoBlue.withAlpha (0.9f) : Colors::textDim.withAlpha (0.4f));
        g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

        // Secondary glow stroke (wider, dimmer)
        if (lfo.enabled)
        {
            g.setColour (Colors::lfoBlue.withAlpha (0.12f));
            g.strokePath (wave, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
        }
    }

    // Phase cursor (playhead)
    if (lfo.enabled)
    {
        float px = wfBounds.getX() + lfo.phase * wfBounds.getWidth();
        float val = LfoEngine::waveformAt (lfo.shape, lfo.phase, lfo.lastRandom);
        float py = wfBounds.getY() + (1.0f - val) * wfBounds.getHeight();

        // Vertical scanline
        g.setColour (Colors::lfoBlue.withAlpha (0.2f));
        g.drawLine (px, (float) wfBounds.getY(), px, (float) wfBounds.getBottom(), 1.0f);

        // Outer glow ring
        g.setColour (Colors::lfoBlue.withAlpha (0.15f));
        g.fillEllipse (px - 8.0f, py - 8.0f, 16.0f, 16.0f);
        // Inner dot
        g.setColour (Colors::lfoBlue);
        g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);
        // Bright center
        g.setColour (Colors::text.withAlpha (0.7f));
        g.fillEllipse (px - 1.2f, py - 1.2f, 2.4f, 2.4f);
    }

    // Border — subtle inset
    g.setColour (Colors::border.withAlpha (0.4f));
    g.drawRect (wfBounds.toFloat(), 1.0f);

    // ── Targets header ───────────────────────────────────────────
    int targetsX = getWidth() / 2 + 10;
    g.setColour (Colors::lfoBlue.withAlpha (0.5f));
    g.fillRect (targetsX - 1, 30, 3, 3);
    g.setColour (Colors::textDim.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (8.0f).withStyle ("Bold")));
    g.drawText ("TARGETS", targetsX + 6, 28, 60, 12, juce::Justification::centredLeft);

    if (lfo.targets.empty())
    {
        g.setColour (Colors::textDim.withAlpha (0.25f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("No targets. Use + TARGET or drag onto a macro.", targetsX, 44, getWidth() / 2 - 30, 16, juce::Justification::centredLeft);
    }
}

void LfoPanel::resized()
{
    // Tab buttons at top
    int tabW = 52;
    for (int i = 0; i < LfoEngine::numLfos; ++i)
        lfoTabs[i].setBounds (10 + i * (tabW + 4), 4, tabW, 18);

    // Enable button + shape buttons in same row as tabs
    enableButton.setBounds (130, 4, 36, 18);
    int sx = 174;
    for (int s = 0; s < LfoEngine::NumShapes; ++s)
    {
        shapeButtons[s].setBounds (sx, 4, 32, 18);
        sx += 35;
    }

    // Knobs next to waveform
    int knobX = getWidth() / 2 - 130;
    syncToggle.setBounds (knobX - 44, 30, 40, 18);
    rateKnob.setBounds (knobX, 26, 56, 60);
    divButton.setBounds (knobX + 8, 40, 44, 20);
    depthKnob.setBounds (knobX + 62, 26, 56, 60);

    // Drag handle — visible in top row after shape buttons
    dragHandle.setBounds (sx + 6, 4, 80, 18);

    // Targets on right side
    int targetsX = getWidth() / 2 + 10;
    addTargetButton.setBounds (targetsX + 90, 26, 72, 18);

    int ty = 44;
    for (auto* tr : targetRows)
    {
        tr->label.setBounds (targetsX, ty, getWidth() / 2 - 50, 14);
        // Range knobs below the label
        int knobY = ty + 15;
        int knobSz = 36;
        tr->minKnob.setBounds (targetsX, knobY, knobSz + 8, knobSz + 14);
        tr->centerKnob.setBounds (targetsX + knobSz + 14, knobY, knobSz + 8, knobSz + 14);
        tr->maxKnob.setBounds (targetsX + (knobSz + 14) * 2, knobY, knobSz + 8, knobSz + 14);
        tr->removeButton.setBounds (getWidth() - 30, ty + 2, 16, 16);
        ty += 68;
    }
}

void LfoPanel::refresh()
{
    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);

    // Update tab colors
    for (int i = 0; i < LfoEngine::numLfos; ++i)
        lfoTabs[i].setColour (juce::TextButton::buttonColourId,
            i == activeLfo ? Colors::lfoBlue.withAlpha (0.45f) : Colors::surfaceRaised);

    // Update enable button
    enableButton.setToggleState (lfo.enabled, juce::dontSendNotification);
    enableButton.setButtonText (lfo.enabled ? "ON" : "OFF");
    enableButton.setColour (juce::TextButton::buttonColourId, lfo.enabled ? Colors::lfoBlue.withAlpha (0.5f) : Colors::surfaceRaised);
    enableButton.setColour (juce::TextButton::buttonOnColourId, Colors::lfoBlue.withAlpha (0.5f));

    // Update shape buttons
    for (int s = 0; s < LfoEngine::NumShapes; ++s)
        shapeButtons[s].setColour (juce::TextButton::buttonColourId,
            (int) lfo.shape == s ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

    // Update knobs
    rateKnob.setValue (lfo.rate, false);
    depthKnob.setValue (lfo.depth, false);

    // Update sync mode
    syncToggle.setButtonText (lfo.syncToHost ? "SYNC" : "FREE");
    syncToggle.setColour (juce::TextButton::buttonColourId,
        lfo.syncToHost ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);
    rateKnob.setVisible (!lfo.syncToHost);
    divButton.setVisible (lfo.syncToHost);
    divButton.setButtonText (LfoEngine::divisionName (lfo.noteDivision));

    refreshTargetRows();
    resized();
}

void LfoPanel::setActiveLfo (int index)
{
    activeLfo = juce::jlimit (0, LfoEngine::numLfos - 1, index);
    refresh();
}

void LfoPanel::refreshTargetRows()
{
    targetRows.clear();
    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
    for (int ti = 0; ti < (int) lfo.targets.size(); ++ti)
    {
        auto* tr = targetRows.add (new TargetRow());
        auto& t = lfo.targets[(size_t) ti];
        juce::String lbl;
        if (t.type == LfoTarget::Macro)
            lbl = "Macro " + juce::String (t.macroIndex + 1);
        else 
        {
            auto nodeId = proc.getChainGraph().getNodeIdForUid (t.slotUid);
            if (auto* node = proc.getGraph().getNodeForId (nodeId))
            {
                auto& params = node->getProcessor()->getParameters();
                lbl = node->getProcessor()->getName() + " > "
                    + (t.paramIndex < params.size() ? params[t.paramIndex]->getName (20) : juce::String ("?"));
            }
        }
        tr->label.setText (lbl, juce::dontSendNotification);
        tr->label.setColour (juce::Label::textColourId, Colors::text);
        tr->label.setFont (juce::Font (juce::FontOptions (9.0f)));
        addAndMakeVisible (tr->label);

        // Range knobs: Min, Max, Start (center)
        tr->minKnob.setValue (t.minValue, false);
        tr->minKnob.setShowPercentage (true);
        tr->minKnob.onValueChange = [this, ti]() {
            auto& tgt = proc.getLfoEngine().getLfo (activeLfo).targets[(size_t) ti];
            tgt.minValue = targetRows[ti]->minKnob.getValue();
        };
        addAndMakeVisible (tr->minKnob);

        tr->maxKnob.setValue (t.maxValue, false);
        tr->maxKnob.setShowPercentage (true);
        tr->maxKnob.onValueChange = [this, ti]() {
            auto& tgt = proc.getLfoEngine().getLfo (activeLfo).targets[(size_t) ti];
            tgt.maxValue = targetRows[ti]->maxKnob.getValue();
        };
        addAndMakeVisible (tr->maxKnob);

        tr->centerKnob.setValue (t.center, false);
        tr->centerKnob.setShowPercentage (true);
        tr->centerKnob.onValueChange = [this, ti]() {
            auto& tgt = proc.getLfoEngine().getLfo (activeLfo).targets[(size_t) ti];
            tgt.center = targetRows[ti]->centerKnob.getValue();
        };
        addAndMakeVisible (tr->centerKnob);

        tr->removeButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        tr->removeButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        tr->removeButton.onClick = [this, ti]() {
            proc.getLfoEngine().removeTarget (activeLfo, ti);
            refresh();
        };
        addAndMakeVisible (tr->removeButton);
    }
}

void LfoPanel::showTargetMenu (int lfoIndex)
{
    juce::PopupMenu menu;
    int itemId = 1;
    juce::PopupMenu macroSub;
    for (int i = 0; i < MacroManager::numMacros; ++i)
        macroSub.addItem (itemId++, "Macro " + juce::String (i + 1));
    menu.addSubMenu ("Macros", macroSub);

    struct PI { LfoTarget t; };
    std::vector<PI> lookup;
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        LfoTarget t;
        t.type = LfoTarget::Macro;
        t.macroIndex = i;
        lookup.push_back ({ t });
    }

    auto& cg = proc.getChainGraph();
    for (int ci = 0; ci < cg.getNumChains(); ++ci)
    {
        for (auto& slot : cg.getChain (ci).slots)
        {
            if (auto* node = proc.getGraph().getNodeForId (slot.nodeId))
            {
                auto& params = node->getProcessor()->getParameters();
                if (params.isEmpty()) continue;
                juce::PopupMenu sub;
                for (int pi = 0; pi < params.size(); ++pi)
                {
                    LfoTarget t;
                    t.type = LfoTarget::Parameter;
                    t.slotUid = slot.uid;
                    t.paramIndex = pi;
                    lookup.push_back ({ t });
                    sub.addItem (itemId++, params[pi]->getName (40));
                }
                menu.addSubMenu (node->getProcessor()->getName(), sub);
            }
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addTargetButton),
        [this, lfoIndex, lookup] (int r) {
            if (r > 0 && r <= (int) lookup.size())
            {
                proc.getLfoEngine().addTarget (lfoIndex, lookup[(size_t) (r - 1)].t);
                refresh();
            }
        });
}

//==============================================================================
// ChainHostEditor
//==============================================================================

ChainHostEditor::ChainHostEditor (ChainHostProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBrowser (p), mappingPanel (p), lfoPanel (p)
{
    setLookAndFeel (&laf);

    auto styleBtn = [] (juce::TextButton& b, juce::Colour bg, juce::Colour fg = Colors::text) {
        b.setColour (juce::TextButton::buttonColourId, bg);
        b.setColour (juce::TextButton::textColourOffId, fg);
    };
    styleBtn (scanButton, Colors::surfaceRaised);
    styleBtn (addPluginButton, Colors::accent.withAlpha (0.45f));
    styleBtn (addChainButton, Colors::chainAccent.withAlpha (0.35f));
    styleBtn (presetsToggle, Colors::surfaceRaised);
    styleBtn (tabMappings, Colors::accent.withAlpha (0.45f));
    styleBtn (tabLfo, Colors::surfaceRaised);

    scanButton.onClick = [this]() {
        scanButton.setButtonText ("..."); scanButton.setEnabled (false);
        proc.getScanner().onScanComplete = [this]() { scanButton.setButtonText ("SCAN"); scanButton.setEnabled (true); };
        proc.getScanner().scan();
    };
    addPluginButton.onClick = [this]() { showPluginMenu (0); };
    addChainButton.onClick = [this]() { proc.getChainGraph().addParallelChain (proc.getGraph()); refreshChainView(); };
    presetsToggle.onClick = [this]() {
        presetBrowserOpen = !presetBrowserOpen;
        presetsToggle.setColour (juce::TextButton::buttonColourId, presetBrowserOpen ? Colors::accent.withAlpha (0.45f) : Colors::surfaceRaised);
        presetBrowser.setVisible (presetBrowserOpen);
        if (presetBrowserOpen) presetBrowser.refresh();
        resized();
        repaint();
    };
    tabMappings.onClick = [this]() { setActiveTab (0); };
    tabLfo.onClick = [this]() { setActiveTab (1); };

    for (auto* btn : { &scanButton, &addPluginButton, &addChainButton, &presetsToggle, &tabMappings, &tabLfo })
        addAndMakeVisible (btn);

    presetBrowser.setVisible (false);
    presetBrowser.onPresetLoaded = [this]() {
        // Close all plugin editor windows BEFORE refreshing — the old graph nodes are gone
        pluginWindows.clear();
        windowCloseTimestamps.clear();
        // Close preset browser so chains reposition cleanly
        presetBrowserOpen = false;
        presetsToggle.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
        presetBrowser.setVisible (false);
        refreshChainView();
        mappingPanel.refresh();
        lfoPanel.refresh();
        refreshMacroLabels();
    };
    addAndMakeVisible (presetBrowser);

    juce::Colour macroColours[] = {
        Colors::accent, Colors::accentBright, juce::Colour (0xffcc6644), juce::Colour (0xffdd8855),
        juce::Colour (0xffbb7744), juce::Colour (0xffcc9955), juce::Colour (0xffddaa66), juce::Colour (0xffeebb77)
    };
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        macroKnobs[i].setArcColour (macroColours[i]);
        macroKnobs[i].setDefaultValue (0.0f);
        macroKnobs[i].onValueChange = [this, i]() { proc.getParameters()[i]->setValueNotifyingHost (macroKnobs[i].getValue()); };
        macroKnobs[i].onDrop = [this, i] (const juce::var& desc) {
            auto str = desc.toString();
            if (str.startsWith ("lfo:"))
            {
                int lfoIndex = str.substring (4).getIntValue();
                LfoTarget t;
                t.type = LfoTarget::Macro;
                t.macroIndex = i;
                proc.getLfoEngine().addTarget (lfoIndex, t);
                // Auto-enable the LFO when assigning a target
                proc.getLfoEngine().getLfo (lfoIndex).enabled = true;
                lfoPanel.refresh();
            }
        };
        addAndMakeVisible (macroKnobs[i]);

        macroLabels[i].setText ("MACRO " + juce::String (i + 1), juce::dontSendNotification);
        macroLabels[i].setJustificationType (juce::Justification::centred);
        macroLabels[i].setColour (juce::Label::textColourId, Colors::textDim);
        macroLabels[i].setFont (juce::Font (juce::FontOptions (8.5f)));
        macroLabels[i].setInterceptsMouseClicks (true, false);
        macroLabels[i].addMouseListener (this, false);
        addAndMakeVisible (macroLabels[i]);

        learnButtons[i].setButtonText ("LEARN");
        learnButtons[i].setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
        learnButtons[i].setColour (juce::TextButton::textColourOffId, Colors::textDim);
        learnButtons[i].onClick = [this, i]() {
            if (mappingPanel.isLearning() && mappingPanel.getLearningMacro() == i) mappingPanel.stopLearn();
            else mappingPanel.startLearn (i);
            for (int j = 0; j < MacroManager::numMacros; ++j) {
                bool learning = mappingPanel.isLearning() && mappingPanel.getLearningMacro() == j;
                learnButtons[j].setColour (juce::TextButton::buttonColourId, learning ? Colors::learn.withAlpha (0.5f) : Colors::surfaceRaised);
                learnButtons[j].setColour (juce::TextButton::textColourOffId, learning ? Colors::learn : Colors::textDim);
            }
        };
        addAndMakeVisible (learnButtons[i]);

        clearButtons[i].setButtonText ("CLR");
        clearButtons[i].setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.25f));
        clearButtons[i].setColour (juce::TextButton::textColourOffId, Colors::remove);
        clearButtons[i].onClick = [this, i]() {
            proc.getMacroManager().clearMappings (i);
            refreshMacroLabels(); mappingPanel.refresh();
        };
        addAndMakeVisible (clearButtons[i]);
    }

    mappingPanel.onMappingChanged = [this]() { refreshMacroLabels(); };
    addAndMakeVisible (mappingPanel);
    addAndMakeVisible (lfoPanel);
    setActiveTab (0);

    refreshChainView(); refreshMacroLabels();
    setResizable (true, false);
    setSize (940, 740);
    startTimer (50);
}

ChainHostEditor::~ChainHostEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
    pluginWindows.clear();
}

void ChainHostEditor::timerCallback()
{
    cleanupClosedWindows();
    mappingPanel.checkLearn();
    if (!mappingPanel.isLearning())
        for (int i = 0; i < MacroManager::numMacros; ++i)
            if (learnButtons[i].findColour (juce::TextButton::buttonColourId) != Colors::surfaceRaised) {
                learnButtons[i].setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
                learnButtons[i].setColour (juce::TextButton::textColourOffId, Colors::textDim);
                refreshMacroLabels();
            }

    // Sync macro knob visuals with current values (LFO modulation + host automation)
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        float displayVal = proc.getMacroManager().getLastValue (i);
        if (std::abs (displayVal - macroKnobs[i].getValue()) > 0.001f)
            macroKnobs[i].setValue (displayVal, false);
    }

    if (activeTab == 1) lfoPanel.repaint();
}

void ChainHostEditor::paint (juce::Graphics& g)
{
    // Base fill — deep obsidian
    g.fillAll (Colors::bg);

    // Subtle noise-like texture via fine horizontal scan lines
    g.setColour (juce::Colour (0x03ffffff));
    for (int sy = 0; sy < getHeight(); sy += 2)
        g.fillRect (0, sy, getWidth(), 1);

    // ── Header ──────────────────────────────────────────────────────
    {
        // Gradient header: surface fading into bg
        g.setGradientFill (juce::ColourGradient (
            Colors::surface, 0, 0,
            Colors::surface.withAlpha (0.0f), 0, 56, false));
        g.fillRect (0, 0, getWidth(), 56);

        // Bottom edge: subtle warm glow line
        g.setGradientFill (juce::ColourGradient (
            Colors::accent.withAlpha (0.0f), 0, 48,
            Colors::accent.withAlpha (0.12f), (float) getWidth() * 0.3f, 48, false));
        g.fillRect (0, 47, getWidth(), 2);
        g.setColour (Colors::border.withAlpha (0.5f));
        g.drawLine (0, 48, (float) getWidth(), 48, 1.0f);

        // Title — bold with subtle glow
        g.setColour (Colors::accent);
        g.setFont (juce::Font (juce::FontOptions (21.0f).withStyle ("Bold")));
        g.drawText ("ChainHost", 16, 6, 160, 36, juce::Justification::centredLeft);

        // Version badge
        g.setColour (Colors::textDim.withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        g.drawText ("v" CHAINHOST_VERSION, 140, 22, 42, 10, juce::Justification::centredLeft);
    }

    // ── Chain rows ──────────────────────────────────────────────────
    int macroTop = getHeight() - 230;
    auto& cg = proc.getChainGraph();
    for (int i = 0; i < cg.getNumChains() && i < (int) chainRows.size(); ++i)
    {
        // Derive row position from the actual component bounds (set by resized)
        int rowY = chainRows[i]->muteButton.getBounds().getY() - 12;
        int rowBottom = rowY + 80;

        // Don't paint chain rows that overlap the macro/LFO section
        if (rowBottom > macroTop) break;

        // Alternating row depth
        if (i % 2 == 0)
        {
            g.setColour (Colors::surface.withAlpha (0.15f));
            g.fillRect (0, rowY, getWidth(), 80);
        }

        // Chain mute button indicator
        auto btnRect = chainRows[i]->muteButton.getBounds().toFloat().expanded (3);
        bool active = !cg.isChainMuted (i);

        if (active)
        {
            g.setColour (Colors::chainAccent.withAlpha (0.08f));
            g.fillRect (btnRect.expanded (5));
            g.setColour (Colors::chainAccent.withAlpha (0.12f));
            g.fillRect (btnRect.expanded (2));
        }
        g.setColour (active ? Colors::chainAccent.withAlpha (0.55f) : Colors::border.withAlpha (0.4f));
        g.drawRect (btnRect, 1.0f);

        // Row separator — gradient fade from left
        g.setGradientFill (juce::ColourGradient (
            Colors::border.withAlpha (0.4f), 0, (float) rowBottom,
            Colors::border.withAlpha (0.08f), (float) getWidth(), (float) rowBottom, false));
        g.fillRect (0, rowBottom - 1, getWidth(), 1);
    }

    // ── Macro section ───────────────────────────────────────────────
    int macroAreaW = getWidth() / 2;

    // Top edge with accent glow
    g.setGradientFill (juce::ColourGradient (
        Colors::accent.withAlpha (0.06f), 0, (float) macroTop,
        Colors::accent.withAlpha (0.0f), 0, (float) (macroTop + 16), false));
    g.fillRect (0, macroTop, macroAreaW, 16);
    g.setColour (Colors::border.withAlpha (0.6f));
    g.drawLine (0, (float) macroTop, (float) getWidth(), (float) macroTop, 1.0f);

    // Macro area background — slightly warmer than base
    g.setColour (Colors::surface.withAlpha (0.4f));
    g.fillRect (0, macroTop + 1, macroAreaW, getHeight() - macroTop - 1);

    // Section label
    g.setColour (Colors::accent.withAlpha (0.4f));
    g.setFont (juce::Font (juce::FontOptions (8.0f).withStyle ("Bold")));
    g.drawText ("MACROS", 10, macroTop + 4, 52, 10, juce::Justification::centredLeft);
    // Small accent dot before label
    g.setColour (Colors::accent.withAlpha (0.6f));
    g.fillRect (6, macroTop + 7, 3, 3);

    // Vertical separator with gradient
    g.setGradientFill (juce::ColourGradient (
        Colors::border.withAlpha (0.6f), (float) macroAreaW, (float) macroTop,
        Colors::border.withAlpha (0.15f), (float) macroAreaW, (float) getHeight(), false));
    g.fillRect (macroAreaW, macroTop, 1, getHeight() - macroTop);
}

void ChainHostEditor::resized()
{
    int x = 180;
    scanButton.setBounds (x, 12, 52, 24); x += 58;
    addPluginButton.setBounds (x, 12, 80, 24); x += 86;
    addChainButton.setBounds (x, 12, 68, 24);
    presetsToggle.setBounds (getWidth() - 82, 12, 70, 24);

    int chainTop = 48;
    if (presetBrowserOpen) { presetBrowser.setBounds (0, 48, getWidth(), 140); chainTop = 188; }

    auto& cg = proc.getChainGraph();
    int macroTop = getHeight() - 230;
    int y = chainTop;
    for (int ci = 0; ci < (int)chainRows.size() && ci < cg.getNumChains(); ++ci) {
        auto* row = chainRows[ci];
        bool visible = (y + 80) <= macroTop;
        row->muteButton.setBounds (8, y + 12, 30, 30);
        row->muteButton.setVisible (visible);
        row->volumeKnob.setBounds (42, y + 4, 50, 68);
        row->volumeKnob.setVisible (visible);
        int px = 96;
        for (auto* slot : row->slotComponents) { slot->setBounds (px, y + 4, 130, 72); slot->setVisible (visible); px += 134; }
        row->addToChainButton.setBounds (px + 2, y + 24, 26, 26);
        row->addToChainButton.setVisible (visible);
        row->removeChainButton.setBounds (getWidth() - 28, y + 28, 20, 20);
        row->removeChainButton.setVisible (visible);
        y += 80;
    }

    // Macro knobs: 4x2 grid filling entire bottom-left area
    int macroAreaW = getWidth() / 2;
    int macroH = (getHeight() - macroTop - 18) / 2;  // available height per row
    int macroW = macroAreaW / 4;
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        int col = i % 4;
        int row2 = i / 4;
        int mx = col * macroW;
        int my = macroTop + 18 + row2 * macroH;
        int knobSz = juce::jmin (macroW - 16, macroH - 40);
        macroLabels[i].setBounds (mx, my, macroW, 12);
        macroKnobs[i].setBounds (mx + (macroW - knobSz) / 2, my + 12, knobSz, knobSz);
        int btnY = my + 12 + knobSz + 2;
        int btnAreaW = 44 + 4 + 28;  // LEARN + gap + CLR
        int btnX = mx + (macroW - btnAreaW) / 2;
        learnButtons[i].setBounds (btnX, btnY, 44, 14);
        clearButtons[i].setBounds (btnX + 48, btnY, 28, 14);
    }

    // Tabs + panels fill the right side of the bottom section
    int panelLeft = macroAreaW + 2;
    tabMappings.setBounds (panelLeft + 4, macroTop + 4, 76, 18);
    tabLfo.setBounds (panelLeft + 86, macroTop + 4, 44, 18);
    int panelTop = macroTop + 26;
    mappingPanel.setBounds (panelLeft, panelTop, getWidth() - panelLeft, getHeight() - panelTop);
    lfoPanel.setBounds (panelLeft, panelTop, getWidth() - panelLeft, getHeight() - panelTop);
}

void ChainHostEditor::refreshChainView()
{
    chainRows.clear();
    auto& cg = proc.getChainGraph();
    for (int ci = 0; ci < cg.getNumChains(); ++ci) {
        auto* row = chainRows.add (new ChainViewRow());
        auto& chain = cg.getChain (ci);
        row->volumeKnob.setValue (chain.volume, false); row->volumeKnob.setDefaultValue (1.0f);
        row->volumeKnob.onValueChange = [this, ci, row]() { proc.getChainGraph().setChainVolume (proc.getGraph(), ci, row->volumeKnob.getValue()); };
        addAndMakeVisible (row->volumeKnob);

        // Chain number as mute toggle
        bool muted = cg.isChainMuted (ci);
        row->muteButton.setButtonText (juce::String (ci + 1));
        row->muteButton.setClickingTogglesState (true);
        row->muteButton.setToggleState (!muted, juce::dontSendNotification);
        row->muteButton.setColour (juce::TextButton::buttonColourId, muted ? Colors::surfaceRaised : Colors::chainAccent.withAlpha (0.5f));
        row->muteButton.setColour (juce::TextButton::buttonOnColourId, Colors::chainAccent.withAlpha (0.5f));
        row->muteButton.setColour (juce::TextButton::textColourOffId, Colors::textDim);
        row->muteButton.setColour (juce::TextButton::textColourOnId, Colors::chainAccent.brighter (0.4f));
        row->muteButton.onClick = [this, ci, row]() {
            bool nowMuted = !row->muteButton.getToggleState();
            proc.getChainGraph().setChainMuted (proc.getGraph(), ci, nowMuted);
            row->muteButton.setColour (juce::TextButton::buttonColourId, nowMuted ? Colors::surfaceRaised : Colors::chainAccent.withAlpha (0.5f));
        };
        addAndMakeVisible (row->muteButton);

        row->removeChainButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        row->removeChainButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        row->removeChainButton.onClick = [this, ci]() {
            for (auto& s : proc.getChainGraph().getChain (ci).slots) {
                closePluginWindow (s.nodeId);
                proc.getMacroManager().removeMappingsForUid (s.uid);
                proc.getLfoEngine().removeTargetsForUid (s.uid);
            }
            proc.getChainGraph().removeParallelChain (proc.getGraph(), ci);
            refreshChainView(); mappingPanel.refresh(); lfoPanel.refresh(); refreshMacroLabels();
        };
        if (cg.getNumChains() > 1) addAndMakeVisible (row->removeChainButton);

        row->addToChainButton.setColour (juce::TextButton::buttonColourId, Colors::accent.withAlpha (0.3f));
        row->addToChainButton.setColour (juce::TextButton::textColourOffId, Colors::accent);
        row->addToChainButton.onClick = [this, ci]() { showPluginMenu (ci); };
        addAndMakeVisible (row->addToChainButton);

        for (int si = 0; si < (int)chain.slots.size(); ++si) {
            auto& slot = chain.slots[(size_t)si];
            auto* comp = row->slotComponents.add (new PluginSlotComponent (proc, slot.nodeId, ci, si));
            comp->onOpenEditor = [this] (auto nid) { openPluginWindow (nid); };
            comp->onRemove = [this, uid = slot.uid] (auto nid) {
                closePluginWindow (nid); proc.getMacroManager().removeMappingsForUid (uid);
                proc.getLfoEngine().removeTargetsForUid (uid); proc.getChainGraph().removePlugin (proc.getGraph(), nid);
                refreshChainView(); mappingPanel.refresh(); lfoPanel.refresh(); refreshMacroLabels();
            };
            comp->onMove = [this] (int fc, int fs, int tc, int ts) { proc.getChainGraph().movePlugin (proc.getGraph(), fc, fs, tc, ts); refreshChainView(); };
            addAndMakeVisible (comp);
        }
    }
    resized(); repaint();
}

void ChainHostEditor::refreshMacroLabels()
{
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        auto& mappings = proc.getMacroManager().getMappings (i);
        if (mappings.empty()) {
            macroLabels[i].setText ("MACRO " + juce::String (i + 1), juce::dontSendNotification);
            macroLabels[i].setColour (juce::Label::textColourId, Colors::textDim);
            clearButtons[i].setVisible (false);
        } else {
            juce::String name;
            for (auto& m : mappings) {
                auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
                if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
                    auto& params = node->getProcessor()->getParameters();
                    if (m.paramIndex < params.size()) {
                        if (name.isNotEmpty()) name += ", ";
                        name += params[m.paramIndex]->getName (20);
                    }
                }
            }
            if (name.isEmpty()) name = "?";
            if (name.length() > 28) name = name.substring (0, 25) + "...";
            macroLabels[i].setText (name, juce::dontSendNotification);
            macroLabels[i].setColour (juce::Label::textColourId, Colors::accent);
            clearButtons[i].setVisible (true);
        }
    }
}

void ChainHostEditor::mouseDown (const juce::MouseEvent& e)
{
    // Right-click on macro labels shows unmap menu
    if (e.mods.isPopupMenu())
    {
        for (int i = 0; i < MacroManager::numMacros; ++i)
        {
            if (e.eventComponent == &macroLabels[i])
            {
                auto& mappings = proc.getMacroManager().getMappings (i);
                if (mappings.empty()) return;

                juce::PopupMenu menu;
                menu.addSectionHeader ("Macro " + juce::String (i + 1) + " Mappings");
                int id = 1;
                for (auto& m : mappings) {
                    juce::String itemName = "?";
                    auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
                    if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
                        auto& params = node->getProcessor()->getParameters();
                        if (m.paramIndex < params.size())
                            itemName = node->getProcessor()->getName() + " > " + params[m.paramIndex]->getName (30);
                    }
                    menu.addItem (id++, "Remove: " + itemName);
                }
                menu.addSeparator();
                menu.addItem (1000, "Clear All Mappings");

                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (macroLabels[i]),
                    [this, i, mappings] (int result) {
                        if (result == 1000) {
                            proc.getMacroManager().clearMappings (i);
                        } else if (result > 0 && result <= (int) mappings.size()) {
                            auto& m = mappings[(size_t) result - 1];
                            proc.getMacroManager().removeMapping (i, m.slotUid, m.paramIndex);
                        }
                        refreshMacroLabels();
                        mappingPanel.refresh();
                    });
                return;
            }
        }
    }
}

void ChainHostEditor::showPluginMenu (int chainIndex)
{
    auto types = proc.getScanner().getPluginList().getTypes();
    if (types.isEmpty()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "No Plugins", "Click SCAN first."); return; }
    juce::PopupMenu menu; std::map<juce::String, juce::PopupMenu> subs; int id = 1;
    for (auto& d : types) { subs[d.manufacturerName.isEmpty() ? "Unknown" : d.manufacturerName].addItem (id++, d.name); }
    for (auto& [mfr, sub] : subs) menu.addSubMenu (mfr, sub);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addPluginButton),
        [this, types, chainIndex] (int r) { if (r > 0 && r <= types.size()) {
            proc.getChainGraph().addPlugin (proc.getGraph(), proc.getScanner(), types[r-1], chainIndex);
            refreshChainView(); mappingPanel.refresh(); lfoPanel.refresh(); }});
}

void ChainHostEditor::openPluginWindow (juce::AudioProcessorGraph::NodeID nodeId)
{
    cleanupClosedWindows();
    auto now = juce::Time::getMillisecondCounter();
    auto it = windowCloseTimestamps.find (nodeId.uid);
    if (it != windowCloseTimestamps.end()) { if ((now - it->second) < kReopenCooldownMs) return; windowCloseTimestamps.erase (it); }
    for (auto* win : pluginWindows) if (win->getNodeId() == nodeId) { win->toFront (true); return; }
    auto* node = proc.getGraph().getNodeForId (nodeId);
    if (!node) return;
    auto* plugProc = node->getProcessor();
    if (!plugProc->hasEditor()) return;
    if (auto* editor = plugProc->createEditor())
        pluginWindows.add (new PluginWindow (plugProc->getName(), nodeId, editor,
            [this] (auto nid) { windowCloseTimestamps[nid.uid] = juce::Time::getMillisecondCounter(); }));
}

void ChainHostEditor::closePluginWindow (juce::AudioProcessorGraph::NodeID nodeId) {
    for (int i = pluginWindows.size(); --i >= 0;)
        if (pluginWindows[i]->getNodeId() == nodeId) { pluginWindows[i]->clearContentComponent(); pluginWindows.remove (i); return; }
}

void ChainHostEditor::cleanupClosedWindows() {
    for (int i = pluginWindows.size(); --i >= 0;) if (!pluginWindows[i]->isVisible()) pluginWindows.remove (i);
    auto now = juce::Time::getMillisecondCounter();
    for (auto it = windowCloseTimestamps.begin(); it != windowCloseTimestamps.end();)
        (now - it->second) > kReopenCooldownMs * 2 ? it = windowCloseTimestamps.erase (it) : ++it;
}

void ChainHostEditor::setActiveTab (int tab) {
    activeTab = tab;
    mappingPanel.setVisible (tab == 0); lfoPanel.setVisible (tab == 1);
    tabMappings.setColour (juce::TextButton::buttonColourId, tab == 0 ? Colors::accent.withAlpha (0.45f) : Colors::surfaceRaised);
    tabLfo.setColour (juce::TextButton::buttonColourId, tab == 1 ? Colors::lfoBlue.withAlpha (0.45f) : Colors::surfaceRaised);
}
