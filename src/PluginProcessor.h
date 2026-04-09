#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChainGraph.h"
#include "MacroManager.h"
#include "PluginScanner.h"
#include "LfoEngine.h"

class ChainHostProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorParameter::Listener
{
public:
    ChainHostProcessor();
    ~ChainHostProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter listener
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    // Accessors
    juce::AudioProcessorGraph& getGraph() { return graph; }
    ChainGraph& getChainGraph() { return chainGraph; }
    MacroManager& getMacroManager() { return macroManager; }
    PluginScanner& getScanner() { return scanner; }
    LfoEngine& getLfoEngine() { return lfoEngine; }

    // Preset directory
    static juce::File getPresetsDirectory();

private:
    juce::AudioProcessorGraph graph;
    ChainGraph chainGraph;
    MacroManager macroManager;
    PluginScanner scanner;
    LfoEngine lfoEngine;

    juce::AudioParameterFloat* macroParams[MacroManager::numMacros] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainHostProcessor)
};
