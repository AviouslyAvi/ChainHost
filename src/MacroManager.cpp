#include "MacroManager.h"

void MacroManager::addMapping (int macroIndex, juce::AudioProcessorGraph::NodeID nodeId,
                               int paramIndex, float minVal, float maxVal)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    // Avoid duplicates
    for (auto& m : mappings[macroIndex])
        if (m.nodeId == nodeId && m.paramIndex == paramIndex)
            return;

    mappings[macroIndex].push_back ({ nodeId, paramIndex, minVal, maxVal });
}

void MacroManager::removeMapping (int macroIndex, juce::AudioProcessorGraph::NodeID nodeId, int paramIndex)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    auto& v = mappings[macroIndex];
    v.erase (std::remove_if (v.begin(), v.end(),
        [&] (const MacroMapping& m) { return m.nodeId == nodeId && m.paramIndex == paramIndex; }),
        v.end());
}

void MacroManager::clearMappings (int macroIndex)
{
    if (macroIndex >= 0 && macroIndex < numMacros)
        mappings[macroIndex].clear();
}

void MacroManager::removeMappingsForNode (juce::AudioProcessorGraph::NodeID nodeId)
{
    for (int i = 0; i < numMacros; ++i)
    {
        auto& v = mappings[i];
        v.erase (std::remove_if (v.begin(), v.end(),
            [&] (const MacroMapping& m) { return m.nodeId == nodeId; }),
            v.end());
    }
}

void MacroManager::setMacroValue (int macroIndex, float normalisedValue,
                                  juce::AudioProcessorGraph& graph)
{
    if (macroIndex < 0 || macroIndex >= numMacros)
        return;

    for (auto& m : mappings[macroIndex])
    {
        if (auto* node = graph.getNodeForId (m.nodeId))
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
    return mappings[macroIndex];
}

std::unique_ptr<juce::XmlElement> MacroManager::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("MacroMappings");

    for (int i = 0; i < numMacros; ++i)
    {
        for (auto& m : mappings[i])
        {
            auto* e = xml->createNewChildElement ("Mapping");
            e->setAttribute ("macro", i);
            e->setAttribute ("nodeId", (int) m.nodeId.uid);
            e->setAttribute ("paramIndex", m.paramIndex);
            e->setAttribute ("min", (double) m.minValue);
            e->setAttribute ("max", (double) m.maxValue);
        }
    }

    return xml;
}

void MacroManager::fromXml (const juce::XmlElement& xml)
{
    for (int i = 0; i < numMacros; ++i)
        mappings[i].clear();

    for (auto* e : xml.getChildWithTagNameIterator ("Mapping"))
    {
        int macroIdx = e->getIntAttribute ("macro", -1);
        if (macroIdx < 0 || macroIdx >= numMacros)
            continue;

        MacroMapping m;
        m.nodeId = juce::AudioProcessorGraph::NodeID ((juce::uint32) e->getIntAttribute ("nodeId"));
        m.paramIndex = e->getIntAttribute ("paramIndex");
        m.minValue = (float) e->getDoubleAttribute ("min", 0.0);
        m.maxValue = (float) e->getDoubleAttribute ("max", 1.0);
        mappings[macroIdx].push_back (m);
    }
}
