#include "MappingPanel.h"
#include "../PluginProcessor.h"
#include <set>

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
        g.drawText ("Move a parameter on any plugin or LFO knob...", 10, 50, getWidth() - 20, 16, juce::Justification::centredLeft);
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
        row->slotUid = m.slotUid; row->paramIndex = m.paramIndex;
        juce::String pName = "?", plugName = "?";
        if (m.slotUid == InternalParams::uid) {
            plugName = "ChainHost";
            pName = InternalParams::paramName (m.paramIndex);
        } else {
            auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
            if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
                plugName = node->getProcessor()->getName();
                auto& params = node->getProcessor()->getParameters();
                if (m.paramIndex < params.size()) pName = params[m.paramIndex]->getName (28);
            }
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
                    learnSnapshot.push_back ({ slot.nodeId, pi, params[pi]->getValue() });
            }
    // Internal ChainHost parameters (LFO knobs)
    for (int ip = 0; ip < InternalParams::NumParams; ++ip)
        learnSnapshot.push_back ({ {}, -(ip + 1),
            InternalParams::getParamValue (ip, proc.getLfoEngine()) });
}

void MappingPanel::checkLearn() {
    if (learningMacroIndex < 0) return;

    // Build set of params already targeted by any LFO — skip them to avoid false detections
    auto& lfoEngine = proc.getLfoEngine();
    std::set<std::pair<juce::uint32, int>> lfoTargeted;
    for (int li = 0; li < LfoEngine::numLfos; ++li)
        for (auto& tgt : lfoEngine.getLfo (li).targets)
            if (tgt.type == LfoTarget::Parameter)
                lfoTargeted.insert ({ tgt.nodeId.uid, tgt.paramIndex });

    // Also skip params already mapped to any macro
    auto& mm = proc.getMacroManager();
    std::set<std::pair<std::string, int>> macroMapped;
    for (int mi = 0; mi < MacroManager::numMacros; ++mi)
        for (auto& m : mm.getMappings (mi))
            macroMapped.insert ({ m.slotUid.toStdString(), m.paramIndex });

    float bestDelta = 0; juce::AudioProcessorGraph::NodeID bestNode {}; int bestParam = -1;
    bool bestIsInternal = false; int bestInternalIdx = -1;
    for (auto& snap : learnSnapshot)
    {
        if (snap.paramIndex < 0) {
            // Internal param: paramIndex is -(ip+1)
            int ip = -(snap.paramIndex + 1);
            float cur = InternalParams::getParamValue (ip, lfoEngine);
            float d = std::abs (cur - snap.value);
            if (d > bestDelta) { bestDelta = d; bestIsInternal = true; bestInternalIdx = ip; }
        }
        else if (auto* node = proc.getGraph().getNodeForId (snap.nodeId)) {
            // Skip if already targeted by an LFO
            if (lfoTargeted.count ({ snap.nodeId.uid, snap.paramIndex }))
                continue;
            // Skip if already mapped to a macro
            auto uid = proc.getChainGraph().getUidForNodeId (snap.nodeId);
            if (macroMapped.count ({ uid.toStdString(), snap.paramIndex }))
                continue;

            auto& params = node->getProcessor()->getParameters();
            if (snap.paramIndex < params.size()) {
                float d = std::abs (params[snap.paramIndex]->getValue() - snap.value);
                if (d > bestDelta) { bestDelta = d; bestNode = snap.nodeId; bestParam = snap.paramIndex; bestIsInternal = false; }
            }
        }
    }
    if (bestDelta > 0.05f) {
        if (bestIsInternal && bestInternalIdx >= 0) {
            proc.getMacroManager().addMapping (learningMacroIndex, InternalParams::uid, bestInternalIdx, 0.0f, 1.0f);
            setSelectedMacro (learningMacroIndex); stopLearn(); refresh();
            if (onMappingChanged) onMappingChanged();
        }
        else if (bestParam >= 0) {
            auto uid = proc.getChainGraph().getUidForNodeId (bestNode);
            if (uid.isNotEmpty()) {
                proc.getMacroManager().addMapping (learningMacroIndex, uid, bestParam, 0.0f, 1.0f);
                setSelectedMacro (learningMacroIndex); stopLearn(); refresh();
                if (onMappingChanged) onMappingChanged();
            }
        }
    }
}

void MappingPanel::showParameterPicker() {
    juce::PopupMenu menu; auto& cg = proc.getChainGraph();
    int itemId = 1;
    struct PI { juce::String uid; int p; };
    std::vector<PI> lookup;

    // Internal ChainHost params (LFO knobs)
    {
        juce::PopupMenu intSub;
        for (int ip = 0; ip < InternalParams::NumParams; ++ip)
        {
            lookup.push_back ({ InternalParams::uid, ip });
            intSub.addItem (itemId++, InternalParams::paramName (ip));
        }
        menu.addSubMenu ("ChainHost", intSub);
    }

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
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (mapButton),
        [this, lookup] (int r) { if (r > 0 && r <= (int)lookup.size()) {
            proc.getMacroManager().addMapping (selectedMacro, lookup[(size_t)(r-1)].uid, lookup[(size_t)(r-1)].p, 0.0f, 1.0f);
            refresh(); if (onMappingChanged) onMappingChanged(); }});
}
