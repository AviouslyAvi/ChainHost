#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ChainGraph;

struct MacroMapping
{
    juce::String slotUid;
    int paramIndex;
    float minValue;
    float maxValue;
};

class MacroManager
{
public:
    static constexpr int numMacros = 8;

    MacroManager() = default;

    void addMapping (int macroIndex, const juce::String& slotUid,
                     int paramIndex, float minVal, float maxVal);
    void removeMapping (int macroIndex, const juce::String& slotUid, int paramIndex);
    void clearMappings (int macroIndex);
    void removeMappingsForUid (const juce::String& slotUid);

    void setMacroValue (int macroIndex, float normalisedValue,
                        juce::AudioProcessorGraph& graph,
                        const ChainGraph& chainGraph);

    float getLastValue (int macroIndex) const { return lastValue[juce::jlimit (0, numMacros - 1, macroIndex)]; }

    const std::vector<MacroMapping>& getMappings (int macroIndex) const;

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    std::vector<MacroMapping> mappings[numMacros];
    float lastValue[numMacros] = {};
    juce::CriticalSection lock;
};
