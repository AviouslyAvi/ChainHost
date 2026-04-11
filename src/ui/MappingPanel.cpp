#include "MappingPanel.h"
#include "../PluginProcessor.h"

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
