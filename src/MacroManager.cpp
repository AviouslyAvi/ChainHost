#include "MacroManager.h"
#include "ChainGraph.h"
#include "LfoEngine.h"

//==============================================================================
// InternalParams helpers
//==============================================================================

juce::String InternalParams::paramName (int paramIdx)
{
    if (paramIdx < 0 || paramIdx >= NumParams) return "?";
    int li = lfoIndex (paramIdx);
    int lp = lfoParam (paramIdx);
    static const char* names[] = { "Rate", "Depth", "Rise", "Smooth", "Delay", "Phase" };
    return "LFO " + juce::String (li + 1) + " " + names[lp];
}

float InternalParams::getParamValue (int paramIdx, const LfoEngine& lfo)
{
    if (paramIdx < 0 || paramIdx >= NumParams) return 0.0f;
    auto& l = lfo.getLfo (lfoIndex (paramIdx));
    switch (lfoParam (paramIdx))
    {
        case 0: return juce::jmap (l.rate, LfoEngine::kMinRate, LfoEngine::kMaxRate, 0.0f, 1.0f);
        case 1: return l.depth;
        case 2: return l.riseTime;
        case 3: return l.smooth;
        case 4: return l.delayTime / 5.0f;
        case 5: return l.startPhase;
        default: return 0.0f;
    }
}

void InternalParams::setParamValue (int paramIdx, float normValue, LfoEngine& lfo)
{
    if (paramIdx < 0 || paramIdx >= NumParams) return;
    auto& l = lfo.getLfo (lfoIndex (paramIdx));
    switch (lfoParam (paramIdx))
    {
        case 0: l.rate = juce::jmap (normValue, LfoEngine::kMinRate, LfoEngine::kMaxRate); break;
        case 1: l.depth = normValue; break;
        case 2: l.riseTime = normValue; break;
        case 3: l.smooth = normValue; break;
        case 4: l.delayTime = normValue * 5.0f; break;
        case 5: l.startPhase = normValue; break;
    }
}

void MacroManager::addMapping (int macroIndex, const juce::String& slotUid,
                               int paramIndex, float minVal, float maxVal)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    const juce::ScopedLock sl (lock);

    // Avoid duplicates
    for (auto& m : mappings[macroIndex])
        if (m.slotUid == slotUid && m.paramIndex == paramIndex)
            return;

    mappings[macroIndex].push_back ({ slotUid, paramIndex, minVal, maxVal });
}

void MacroManager::removeMapping (int macroIndex, const juce::String& slotUid, int paramIndex)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    const juce::ScopedLock sl (lock);
    auto& v = mappings[macroIndex];
    v.erase (std::remove_if (v.begin(), v.end(),
        [&] (const MacroMapping& m) { return m.slotUid == slotUid && m.paramIndex == paramIndex; }),
        v.end());
}

void MacroManager::clearMappings (int macroIndex)
{
    if (macroIndex >= 0 && macroIndex < numMacros)
    {
        const juce::ScopedLock sl (lock);
        mappings[macroIndex].clear();
    }
}

void MacroManager::removeMappingsForUid (const juce::String& slotUid)
{
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < numMacros; ++i)
    {
        auto& v = mappings[i];
        v.erase (std::remove_if (v.begin(), v.end(),
            [&] (const MacroMapping& m) { return m.slotUid == slotUid; }),
            v.end());
    }
}

void MacroManager::setMacroValue (int macroIndex, float normalisedValue,
                                  juce::AudioProcessorGraph& graph,
                                  const ChainGraph& chainGraph,
                                  LfoEngine* lfoEngine)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    const juce::ScopedLock sl (lock);
    lastValue[macroIndex] = normalisedValue;

    for (auto& m : mappings[macroIndex])
    {
        float scaled = m.minValue + normalisedValue * (m.maxValue - m.minValue);

        if (m.slotUid == InternalParams::uid && lfoEngine != nullptr)
        {
            InternalParams::setParamValue (m.paramIndex, scaled, *lfoEngine);
            continue;
        }

        auto nodeId = chainGraph.getNodeIdForUid (m.slotUid);
        if (auto* node = graph.getNodeForId (nodeId))
        {
            if (auto* proc = node->getProcessor())
            {
                auto& params = proc->getParameters();
                if (m.paramIndex >= 0 && m.paramIndex < params.size())
                    params[m.paramIndex]->setValue (scaled);
            }
        }
    }
}

const std::vector<MacroMapping>& MacroManager::getMappings (int macroIndex) const
{
    static const std::vector<MacroMapping> empty;
    if (macroIndex < 0 || macroIndex >= numMacros)
        return empty;

    // Warning: returning a reference to a vector that is protected by a lock is dangerous
    // but without changing the API, we'll just return it. 
    // Ideally, this should return a copy or the caller should hold the lock.
    return mappings[macroIndex];
}

void MacroManager::setMacroName (int macroIndex, const juce::String& name)
{
    if (macroIndex >= 0 && macroIndex < numMacros)
        macroNames[macroIndex] = name;
}

juce::String MacroManager::getMacroName (int macroIndex) const
{
    if (macroIndex >= 0 && macroIndex < numMacros)
        return macroNames[macroIndex];
    return {};
}

std::unique_ptr<juce::XmlElement> MacroManager::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("MacroMappings");

    const juce::ScopedLock sl (lock);
    for (int i = 0; i < numMacros; ++i)
    {
        if (macroNames[i].isNotEmpty())
        {
            auto* n = xml->createNewChildElement ("MacroName");
            n->setAttribute ("macro", i);
            n->setAttribute ("name", macroNames[i]);
        }

        for (auto& m : mappings[i])
        {
            auto* e = xml->createNewChildElement ("Mapping");
            e->setAttribute ("macro", i);
            e->setAttribute ("uid", m.slotUid);
            e->setAttribute ("paramIndex", m.paramIndex);
            e->setAttribute ("min", (double) m.minValue);
            e->setAttribute ("max", (double) m.maxValue);
        }
    }

    return xml;
}

void MacroManager::fromXml (const juce::XmlElement& xml)
{
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < numMacros; ++i)
    {
        mappings[i].clear();
        macroNames[i] = {};
    }

    for (auto* n : xml.getChildWithTagNameIterator ("MacroName"))
    {
        int idx = n->getIntAttribute ("macro", -1);
        if (idx >= 0 && idx < numMacros)
            macroNames[idx] = n->getStringAttribute ("name");
    }

    for (auto* e : xml.getChildWithTagNameIterator ("Mapping"))
    {
        int macroIdx = e->getIntAttribute ("macro", -1);
        if (macroIdx < 0 || macroIdx >= numMacros)
            continue;

        MacroMapping m;
        m.slotUid = e->getStringAttribute ("uid");
        m.paramIndex = e->getIntAttribute ("paramIndex");
        m.minValue = (float) e->getDoubleAttribute ("min", 0.0);
        m.maxValue = (float) e->getDoubleAttribute ("max", 1.0);
        mappings[macroIdx].push_back (m);
    }
}
