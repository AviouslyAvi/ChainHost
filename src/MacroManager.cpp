#include "MacroManager.h"
#include "ChainGraph.h"

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
                                  const ChainGraph& chainGraph)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    const juce::ScopedLock sl (lock);
    lastValue[macroIndex] = normalisedValue;

    for (auto& m : mappings[macroIndex])
    {
        auto nodeId = chainGraph.getNodeIdForUid (m.slotUid);
        if (auto* node = graph.getNodeForId (nodeId))
        {
            if (auto* proc = node->getProcessor())
            {
                auto& params = proc->getParameters();
                if (m.paramIndex >= 0 && m.paramIndex < params.size())
                {
                    float scaled = m.minValue + normalisedValue * (m.maxValue - m.minValue);
                    params[m.paramIndex]->setValue (scaled);
                }
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

std::unique_ptr<juce::XmlElement> MacroManager::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("MacroMappings");

    const juce::ScopedLock sl (lock);
    for (int i = 0; i < numMacros; ++i)
    {
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
        mappings[i].clear();

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
