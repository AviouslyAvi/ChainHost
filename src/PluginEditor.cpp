#include "PluginEditor.h"

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
        if (slider.getMaximum() > 1.1)  // non-normalised range (e.g. LFO rate)
        {
            g.setColour (Colors::text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            auto valStr = juce::String (slider.getValue(), 2) + suffix;
            g.drawText (valStr, 0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
        else
        {
            g.setColour (Colors::text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (juce::String (juce::roundToInt (norm * 100.0f)) + suffix,
                        0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
    }

    if (label.isNotEmpty())
    {
        g.setColour (Colors::textDim);
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        g.drawText (label, 0, getHeight() - 12, getWidth(), 12, juce::Justification::centred);
    }
}

void FabKnob::setValue (float v, bool notify)
{
    slider.setValue (v, notify ? juce::sendNotificationAsync : juce::dontSendNotification);
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

    // Flat slot background — Ableton rack style
    g.setColour (isBypassed ? Colors::slotBg : Colors::surfaceRaised);
    g.fillRect (bounds);

    // 1px border
    g.setColour (isBypassed ? Colors::borderSubtle : Colors::slotBorder);
    g.drawRect (bounds, 1.0f);

    // Left accent bar when active
    if (!isBypassed)
    {
        g.setColour (Colors::accent);
        g.fillRect (bounds.getX(), bounds.getY(), 3.0f, bounds.getHeight());
    }

    // Drop hover
    if (dragHover)
    {
        g.setColour (Colors::accent.withAlpha (0.1f));
        g.fillRect (bounds);
        g.setColour (Colors::accent.withAlpha (0.6f));
        g.drawRect (bounds, 2.0f);
    }

    // Drag grip (flat squares instead of dots)
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
    g.setColour (Colors::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("MACRO MAPPING", 10, 6, 100, 12, juce::Justification::centredLeft);
    if (isLearning()) {
        g.setColour (Colors::learn.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("Move a parameter on any loaded plugin...", 10, 50, getWidth() - 20, 16, juce::Justification::centredLeft);
    } else if (mappingRows.isEmpty()) {
        g.setColour (Colors::textDim.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("No mappings. Use + MAP or LEARN.", 10, 50, getWidth() - 20, 16, juce::Justification::centredLeft);
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
        row->nodeId = m.nodeId; row->paramIndex = m.paramIndex;
        juce::String pName = "?", plugName = "?";
        if (auto* node = proc.getGraph().getNodeForId (m.nodeId)) {
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
            [this, nid = m.nodeId, pi = m.paramIndex, &r = *row]() {
                proc.getMacroManager().removeMapping (selectedMacro, nid, pi);
                proc.getMacroManager().addMapping (selectedMacro, nid, pi, r.minKnob.getValue(), r.maxKnob.getValue());
                if (onMappingChanged) onMappingChanged();
            };
        addAndMakeVisible (row->minKnob); addAndMakeVisible (row->maxKnob);

        row->removeButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        row->removeButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        row->removeButton.onClick = [this, nid = m.nodeId, pi = m.paramIndex]() {
            proc.getMacroManager().removeMapping (selectedMacro, nid, pi);
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
                    learnSnapshot.push_back ({ slot.nodeId, pi, params[pi]->getValue() });
            }
}

void MappingPanel::checkLearn() {
    if (learningMacroIndex < 0) return;
    float bestDelta = 0; juce::AudioProcessorGraph::NodeID bestNode {}; int bestParam = -1;
    for (auto& snap : learnSnapshot)
        if (auto* node = proc.getGraph().getNodeForId (snap.nodeId)) {
            auto& params = node->getProcessor()->getParameters();
            if (snap.paramIndex < params.size()) {
                float d = std::abs (params[snap.paramIndex]->getValue() - snap.value);
                if (d > bestDelta) { bestDelta = d; bestNode = snap.nodeId; bestParam = snap.paramIndex; }
            }
        }
    if (bestDelta > 0.05f && bestParam >= 0) {
        proc.getMacroManager().addMapping (learningMacroIndex, bestNode, bestParam, 0.0f, 1.0f);
        setSelectedMacro (learningMacroIndex); stopLearn(); refresh();
        if (onMappingChanged) onMappingChanged();
    }
}

void MappingPanel::showParameterPicker() {
    juce::PopupMenu menu; auto& cg = proc.getChainGraph();
    int itemId = 1;
    struct PI { juce::AudioProcessorGraph::NodeID n; int p; };
    std::vector<PI> lookup;
    for (int ci = 0; ci < cg.getNumChains(); ++ci)
        for (auto& slot : cg.getChain (ci).slots)
            if (auto* node = proc.getGraph().getNodeForId (slot.nodeId)) {
                auto& params = node->getProcessor()->getParameters();
                if (params.isEmpty()) continue;
                juce::PopupMenu sub;
                for (int pi = 0; pi < params.size(); ++pi) { lookup.push_back ({slot.nodeId, pi}); sub.addItem (itemId++, params[pi]->getName (40)); }
                auto lbl = node->getProcessor()->getName();
                if (cg.getNumChains() > 1) lbl = "Chain " + juce::String (ci + 1) + " > " + lbl;
                menu.addSubMenu (lbl, sub);
            }
    if (itemId == 1) menu.addItem (-1, "(No plugins)", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mapButton),
        [this, lookup] (int r) { if (r > 0 && r <= (int)lookup.size()) {
            proc.getMacroManager().addMapping (selectedMacro, lookup[(size_t)(r-1)].n, lookup[(size_t)(r-1)].p, 0.0f, 1.0f);
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

    addTargetButton.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.3f));
    addTargetButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    addTargetButton.onClick = [this]() { showTargetMenu (activeLfo); };
    addAndMakeVisible (addTargetButton);

    refresh();
}

void LfoPanel::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bgDeep);

    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);

    // ── Waveform display ─────────────────────────────────────────
    auto wfBounds = juce::Rectangle<int> (10, 28, getWidth() / 2 - 140, 56);

    // Dark inset background
    {
        g.setColour (juce::Colour (0xff0e0e18));
        g.fillRect (wfBounds);
    }

    // Grid lines (subtle)
    g.setColour (juce::Colour (0x0cffffff));
    float midY = wfBounds.getCentreY();
    g.drawLine ((float) wfBounds.getX(), midY, (float) wfBounds.getRight(), midY, 0.5f);
    for (int q = 1; q <= 3; ++q)
    {
        float qx = wfBounds.getX() + (wfBounds.getWidth() * q) / 4.0f;
        g.drawLine (qx, (float) wfBounds.getY(), qx, (float) wfBounds.getBottom(), 0.5f);
    }

    // Waveform fill (gradient under the curve)
    {
        juce::Path wave;
        for (int x = 0; x <= wfBounds.getWidth(); ++x)
        {
            float phase = (float) x / (float) wfBounds.getWidth();
            float val = LfoEngine::waveformAt (lfo.shape, phase, 0.5f);
            float py = wfBounds.getY() + (1.0f - val) * wfBounds.getHeight();
            if (x == 0) wave.startNewSubPath ((float) (wfBounds.getX() + x), py);
            else wave.lineTo ((float) (wfBounds.getX() + x), py);
        }

        // Filled area under the curve
        juce::Path fillPath (wave);
        fillPath.lineTo ((float) wfBounds.getRight(), (float) wfBounds.getBottom());
        fillPath.lineTo ((float) wfBounds.getX(), (float) wfBounds.getBottom());
        fillPath.closeSubPath();

        auto fillCol = lfo.enabled ? Colors::lfoBlue : Colors::textDim;
        g.setGradientFill (juce::ColourGradient (
            fillCol.withAlpha (0.25f), 0, (float) wfBounds.getY(),
            fillCol.withAlpha (0.02f), 0, (float) wfBounds.getBottom(), false));
        g.fillPath (fillPath);

        // Stroke the waveform line
        g.setColour (lfo.enabled ? Colors::lfoBlue : Colors::textDim.withAlpha (0.5f));
        g.strokePath (wave, juce::PathStrokeType (2.0f));
    }

    // Phase cursor (playhead) with glow
    if (lfo.enabled)
    {
        float px = wfBounds.getX() + lfo.phase * wfBounds.getWidth();
        float val = LfoEngine::waveformAt (lfo.shape, lfo.phase, lfo.lastRandom);
        float py = wfBounds.getY() + (1.0f - val) * wfBounds.getHeight();

        // Vertical line
        g.setColour (Colors::lfoBlue.withAlpha (0.3f));
        g.drawLine (px, (float) wfBounds.getY(), px, (float) wfBounds.getBottom(), 1.0f);

        // Dot at current position
        g.setColour (Colors::lfoBlue);
        g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
        g.setColour (Colors::lfoBlue.withAlpha (0.3f));
        g.fillEllipse (px - 6.0f, py - 6.0f, 12.0f, 12.0f);
    }

    // Border (flat)
    g.setColour (Colors::border.withAlpha (0.5f));
    g.drawRect (wfBounds.toFloat(), 1.0f);

    // ── Targets header ───────────────────────────────────────────
    int targetsX = getWidth() / 2 + 10;
    g.setColour (Colors::textDim.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    g.drawText ("TARGETS", targetsX, 28, 80, 12, juce::Justification::centredLeft);

    if (lfo.targets.empty())
    {
        g.setColour (Colors::textDim.withAlpha (0.3f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("No targets assigned", targetsX, 44, 200, 16, juce::Justification::centredLeft);
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
    rateKnob.setBounds (knobX, 26, 56, 60);
    depthKnob.setBounds (knobX + 62, 26, 56, 60);

    // Targets on right side
    int targetsX = getWidth() / 2 + 10;
    addTargetButton.setBounds (targetsX + 90, 26, 72, 18);

    int ty = 44;
    for (auto* tr : targetRows)
    {
        tr->label.setBounds (targetsX, ty, getWidth() / 2 - 50, 16);
        tr->removeButton.setBounds (getWidth() - 30, ty, 16, 16);
        ty += 19;
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
        else if (auto* node = proc.getGraph().getNodeForId (t.nodeId))
        {
            auto& params = node->getProcessor()->getParameters();
            lbl = node->getProcessor()->getName() + " > "
                + (t.paramIndex < params.size() ? params[t.paramIndex]->getName (24) : juce::String ("?"));
        }
        tr->label.setText (lbl, juce::dontSendNotification);
        tr->label.setColour (juce::Label::textColourId, Colors::text);
        tr->label.setFont (juce::Font (juce::FontOptions (9.5f)));
        addAndMakeVisible (tr->label);
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
                    t.nodeId = slot.nodeId;
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
    }

    mappingPanel.onMappingChanged = [this]() { refreshMacroLabels(); };
    addAndMakeVisible (mappingPanel);
    lfoPanel.setVisible (false);
    addAndMakeVisible (lfoPanel);

    refreshChainView(); refreshMacroLabels();
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
    if (activeTab == 1) lfoPanel.repaint();
}

void ChainHostEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bg);

    // Flat header — Ableton rack style
    {
        g.setColour (Colors::surface);
        g.fillRect (0, 0, getWidth(), 48);
        g.setColour (Colors::border);
        g.drawLine (0, 48, (float)getWidth(), 48, 1.0f);

        g.setColour (Colors::accent);
        g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
        g.drawText ("ChainHost", 16, 8, 160, 32, juce::Justification::centredLeft);
        g.setColour (Colors::textDim.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("v0.2", 138, 20, 30, 12, juce::Justification::centredLeft);
    }

    // Chain rows — flat with 1px separator lines
    int chainTop = 48 + (presetBrowserOpen ? 140 : 0);
    auto& cg = proc.getChainGraph();
    int y = chainTop;
    for (int i = 0; i < cg.getNumChains(); ++i) {
        // Alternating row tint
        if (i % 2 == 0) { g.setColour (Colors::surface.withAlpha (0.12f)); g.fillRect (0, y, getWidth(), 86); }

        // Rectangular chain badge (no rounded corners)
        g.setColour (Colors::chainAccent.withAlpha (0.1f));
        g.fillRect (10, y + 8, 30, 30);
        g.setColour (Colors::chainAccent.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
        g.drawText (juce::String (i + 1), 10, y + 8, 30, 30, juce::Justification::centred);
        g.setColour (Colors::textDim.withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (7.5f)));
        g.drawText ("CHAIN", 10, y + 40, 30, 10, juce::Justification::centred);

        // Row separator
        g.setColour (Colors::border.withAlpha (0.3f));
        g.drawLine (0, (float)(y + 88), (float)getWidth(), (float)(y + 88), 1.0f);
        y += 88;
    }

    // Macro section — flat, bottom-left, 4x2 grid
    int macroTop = getHeight() - 230;
    g.setColour (Colors::border);
    g.drawLine (0, (float)macroTop, (float)getWidth(), (float)macroTop, 1.0f);
    g.setColour (Colors::surface);
    g.fillRect (0, macroTop + 1, 210, 109);
    g.setColour (Colors::textDim.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("MACROS", 8, macroTop + 4, 60, 12, juce::Justification::centredLeft);
    // Vertical separator between macros and mapping panel
    g.setColour (Colors::border.withAlpha (0.4f));
    g.drawLine (210, (float)macroTop, 210, (float)(macroTop + 110), 1.0f);
    g.setColour (Colors::border);
    g.drawLine (0, (float)(macroTop + 110), (float)getWidth(), (float)(macroTop + 110), 1.0f);
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
    int y = chainTop;
    for (int ci = 0; ci < (int)chainRows.size() && ci < cg.getNumChains(); ++ci) {
        auto* row = chainRows[ci];
        row->volumeKnob.setBounds (44, y + 4, 50, 68);
        int px = 100;
        for (auto* slot : row->slotComponents) { slot->setBounds (px, y + 2, 152, 84); px += 158; }
        row->addToChainButton.setBounds (px + 4, y + 28, 30, 30);
        row->removeChainButton.setBounds (getWidth() - 30, y + 32, 22, 22);
        y += 88;
    }

    // Macro knobs: compact 4x2 grid in bottom-left (Serum 2 style)
    int macroTop = getHeight() - 230;
    int macroW = 50, macroH = 50;
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        int col = i % 4;
        int row2 = i / 4;
        int mx = 6 + col * macroW;
        int my = macroTop + 16 + row2 * (macroH + 2);
        macroLabels[i].setBounds (mx, my, macroW, 10);
        macroKnobs[i].setBounds (mx + 2, my + 10, macroW - 4, 34);
        learnButtons[i].setBounds (mx + 5, my + 44, macroW - 10, 12);
    }

    // Tabs + panels fill the right side of the bottom section
    int panelLeft = 214;
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

        row->removeChainButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        row->removeChainButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        row->removeChainButton.onClick = [this, ci]() {
            for (auto& s : proc.getChainGraph().getChain (ci).slots) {
                closePluginWindow (s.nodeId);
                proc.getMacroManager().removeMappingsForNode (s.nodeId);
                proc.getLfoEngine().removeTargetsForNode (s.nodeId);
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
            auto* comp = row->slotComponents.add (new PluginSlotComponent (proc, chain.slots[(size_t)si].nodeId, ci, si));
            comp->onOpenEditor = [this] (auto nid) { openPluginWindow (nid); };
            comp->onRemove = [this] (auto nid) {
                closePluginWindow (nid); proc.getMacroManager().removeMappingsForNode (nid);
                proc.getLfoEngine().removeTargetsForNode (nid); proc.getChainGraph().removePlugin (proc.getGraph(), nid);
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
        } else {
            // Show the first mapped parameter's plugin name + param name
            juce::String name;
            for (auto& m : mappings) {
                if (auto* node = proc.getGraph().getNodeForId (m.nodeId)) {
                    auto& params = node->getProcessor()->getParameters();
                    if (m.paramIndex < params.size()) {
                        if (name.isNotEmpty()) name += ", ";
                        name += params[m.paramIndex]->getName (20);
                    }
                }
            }
            if (name.isEmpty()) name = "?";
            // Truncate if too long
            if (name.length() > 28) name = name.substring (0, 25) + "...";
            macroLabels[i].setText (name, juce::dontSendNotification);
            macroLabels[i].setColour (juce::Label::textColourId, Colors::accent);
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
                    if (auto* node = proc.getGraph().getNodeForId (m.nodeId)) {
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
                            proc.getMacroManager().removeMapping (i, m.nodeId, m.paramIndex);
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
