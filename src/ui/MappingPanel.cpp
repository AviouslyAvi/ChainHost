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
                for (int pi = 0; pi < params.size(); ++pi) {
                    float v = params[pi]->getValue();
                    learnSnapshot.push_back ({ slot.nodeId, pi, v, v });
                }
            }
    // Internal ChainHost parameters (LFO knobs)
    for (int ip = 0; ip < InternalParams::NumParams; ++ip) {
        float v = InternalParams::getParamValue (ip, proc.getLfoEngine());
        learnSnapshot.push_back ({ {}, -(ip + 1), v, v });
    }
}

void MappingPanel::checkLearn() {
    if (learningMacroIndex < 0) return;

    // Rolling snapshot: compare current values to previous tick. LFO/macro
    // modulation is smooth (small delta per 50ms tick), user interaction is sudden.
    static constexpr float kLearnThreshold = 0.08f;

    // Collect params currently modulated by any enabled LFO — skip them so
    // LFO modulation doesn't get learned as the user moving a param.
    // Includes direct Parameter targets AND params reached via Macro targets.
    std::set<std::pair<uint32_t, int>> lfoTargeted;
    auto& lfoEng = proc.getLfoEngine();
    auto& mm = proc.getMacroManager();
    auto& cg = proc.getChainGraph();
    for (int li = 0; li < LfoEngine::numLfos; ++li) {
        auto& lfo = lfoEng.getLfo (li);
        if (! lfo.enabled) continue;
        for (auto& t : lfo.targets) {
            if (t.type == LfoTarget::Parameter)
                lfoTargeted.insert ({ t.nodeId.uid, t.paramIndex });
            else if (t.type == LfoTarget::Macro) {
                for (auto& m : mm.getMappings (t.macroIndex)) {
                    if (m.slotUid == InternalParams::uid) continue;
                    auto nid = cg.getNodeIdForUid (m.slotUid);
                    lfoTargeted.insert ({ nid.uid, m.paramIndex });
                }
            }
        }
    }

    // Fast move: large jump within one 50ms tick (rolling baseline).
    // Slow move: cumulative drift from learn-start exceeds kDriftThreshold.
    // LFO-targeted params are excluded from both checks.
    static constexpr float kDriftThreshold = 0.05f;

    float bestDelta = 0; juce::AudioProcessorGraph::NodeID bestNode {}; int bestParam = -1;
    bool bestIsInternal = false; int bestInternalIdx = -1;
    bool triggered = false;
    for (auto& snap : learnSnapshot)
    {
        if (snap.paramIndex < 0) {
            int ip = -(snap.paramIndex + 1);
            float cur = InternalParams::getParamValue (ip, proc.getLfoEngine());
            float tickDelta = std::abs (cur - snap.value);
            float drift = std::abs (cur - snap.startValue);
            snap.value = cur;
            if (tickDelta > kLearnThreshold && tickDelta > bestDelta) {
                bestDelta = tickDelta; bestIsInternal = true; bestInternalIdx = ip; triggered = true;
            } else if (drift > kDriftThreshold && drift > bestDelta) {
                bestDelta = drift; bestIsInternal = true; bestInternalIdx = ip; triggered = true;
            }
        }
        else if (auto* node = proc.getGraph().getNodeForId (snap.nodeId)) {
            auto& params = node->getProcessor()->getParameters();
            if (snap.paramIndex < params.size()) {
                float cur = params[snap.paramIndex]->getValue();
                float tickDelta = std::abs (cur - snap.value);
                float drift = std::abs (cur - snap.startValue);
                snap.value = cur;
                if (lfoTargeted.count ({ snap.nodeId.uid, snap.paramIndex })) continue;
                if (tickDelta > kLearnThreshold && tickDelta > bestDelta) {
                    bestDelta = tickDelta; bestNode = snap.nodeId; bestParam = snap.paramIndex; bestIsInternal = false; triggered = true;
                } else if (drift > kDriftThreshold && drift > bestDelta) {
                    bestDelta = drift; bestNode = snap.nodeId; bestParam = snap.paramIndex; bestIsInternal = false; triggered = true;
                }
            }
        }
    }
    if (triggered) {
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
