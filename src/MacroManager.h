#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

struct MacroMapping
{
    juce::AudioProcessorGraph::NodeID nodeId;
    int paramIndex;
    float minValue;
    float maxValue;
};

class MacroManager
{
public:
    static constexpr int numMacros = 8;

    MacroManager() = default;

    void addMapping (int macroIndex, juce::AudioProcessorGraph::NodeID nodeId,
                     int paramIndex, float minVal, float maxVal);
    void removeMapping (int macroIndex, juce::AudioProcessorGraph::NodeID nodeId, int paramIndex);
    void clearMappings (int macroIndex);
    void removeMappingsForNode (juce::AudioProcessorGraph::NodeID nodeId);

    void setMacroValue (int macroIndex, float normalisedValue,
                        juce::AudioProcessorGraph& graph);

    const std::vector<MacroMapping>& getMappings (int macroIndex) const;

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    std::vector<MacroMapping> mappings[numMacros];
};
