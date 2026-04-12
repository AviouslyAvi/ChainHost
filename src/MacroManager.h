#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ChainGraph;
class LfoEngine;

//==============================================================================
// Internal ChainHost parameter indices for macro mapping
// Layout: 4 LFOs × 4 params = 16 total
namespace InternalParams
{
    static constexpr const char* uid = "__internal__";

    enum Index
    {
        LFO0_Rate = 0, LFO0_Depth, LFO0_Rise, LFO0_Smooth,
        LFO1_Rate,     LFO1_Depth, LFO1_Rise, LFO1_Smooth,
        LFO2_Rate,     LFO2_Depth, LFO2_Rise, LFO2_Smooth,
        LFO3_Rate,     LFO3_Depth, LFO3_Rise, LFO3_Smooth,
        NumParams
    };

    inline int lfoIndex (int paramIdx) { return paramIdx / 4; }
    inline int lfoParam (int paramIdx) { return paramIdx % 4; } // 0=rate,1=depth,2=rise,3=smooth

    juce::String paramName (int paramIdx);
    float getParamValue (int paramIdx, const LfoEngine& lfo);
    void setParamValue (int paramIdx, float normValue, LfoEngine& lfo);
}

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
                        const ChainGraph& chainGraph,
                        LfoEngine* lfoEngine = nullptr);

    float getLastValue (int macroIndex) const { return lastValue[juce::jlimit (0, numMacros - 1, macroIndex)]; }

    const std::vector<MacroMapping>& getMappings (int macroIndex) const;

    void setMacroName (int macroIndex, const juce::String& name);
    juce::String getMacroName (int macroIndex) const;

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    std::vector<MacroMapping> mappings[numMacros];
    juce::String macroNames[numMacros];
    float lastValue[numMacros] = {};
    juce::CriticalSection lock;
};
