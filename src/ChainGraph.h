#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginScanner.h"

// Internal gain processor for per-chain volume
class GainProcessor : public juce::AudioProcessor
{
public:
    GainProcessor() : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo())
        .withOutput ("Output", juce::AudioChannelSet::stereo()))
    {}

    const juce::String getName() const override { return "Gain"; }
    void prepareToPlay (double sr, int bs) override { juce::ignoreUnused (sr, bs); }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (gain.load());
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    void setGain (float g) { gain.store (g); }
    float getGain() const { return gain.load(); }

private:
    std::atomic<float> gain { 1.0f };
};

// Shared buffer used to pass the dry (pre-plugin) signal from DryCaptureProcessor
// to DryWetProcessor, since they are separate nodes in the AudioProcessorGraph.
struct SharedDryBuffer
{
    juce::AudioBuffer<float> buffer;
    void prepare (int channels, int maxSamples)
    {
        if (maxSamples > buffer.getNumSamples() || channels > buffer.getNumChannels())
            buffer.setSize (channels, maxSamples, false, true, false);
    }
};

// Placed BEFORE each plugin in the graph. Captures the dry signal into a shared
// buffer, then passes the audio through unchanged to the plugin.
class DryCaptureProcessor : public juce::AudioProcessor
{
public:
    DryCaptureProcessor (std::shared_ptr<SharedDryBuffer> buf)
        : AudioProcessor (BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo())
            .withOutput ("Output", juce::AudioChannelSet::stereo())),
          sharedDry (std::move (buf)) {}

    const juce::String getName() const override { return "DryCapture"; }

    void prepareToPlay (double sr, int bs) override
    {
        juce::ignoreUnused (sr);
        if (sharedDry) sharedDry->prepare (2, bs);
    }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (! sharedDry) return;
        auto& dry = sharedDry->buffer;
        int ch = juce::jmin (buffer.getNumChannels(), dry.getNumChannels());
        int n  = juce::jmin (buffer.getNumSamples(), dry.getNumSamples());
        for (int c = 0; c < ch; ++c)
            dry.copyFrom (c, 0, buffer, c, 0, n);
        // Audio passes through unmodified
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::shared_ptr<SharedDryBuffer> sharedDry;
};

// Placed AFTER each plugin in the graph. Reads the dry signal from the shared
// buffer and blends it with the incoming wet signal based on the wet amount.
class DryWetProcessor : public juce::AudioProcessor
{
public:
    DryWetProcessor (std::shared_ptr<SharedDryBuffer> buf)
        : AudioProcessor (BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo())
            .withOutput ("Output", juce::AudioChannelSet::stereo())),
          sharedDry (std::move (buf)) {}

    const juce::String getName() const override { return "DryWet"; }

    void prepareToPlay (double sr, int bs) override
    {
        juce::ignoreUnused (sr);
        if (sharedDry) sharedDry->prepare (2, bs);
    }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        float w = wet.load();
        if (w >= 1.0f || ! sharedDry) return;

        auto& dry = sharedDry->buffer;
        int n  = juce::jmin (buffer.getNumSamples(), dry.getNumSamples());
        int ch = juce::jmin (buffer.getNumChannels(), dry.getNumChannels());

        if (w <= 0.0f)
        {
            for (int c = 0; c < ch; ++c)
                buffer.copyFrom (c, 0, dry, c, 0, n);
            return;
        }

        for (int c = 0; c < ch; ++c)
        {
            auto* out    = buffer.getWritePointer (c);
            auto* dryPtr = dry.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                out[i] = dryPtr[i] * (1.0f - w) + out[i] * w;
        }
    }

    void setWet (float w) { wet.store (juce::jlimit (0.0f, 1.0f, w)); }
    float getWet() const { return wet.load(); }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::atomic<float> wet { 1.0f };
    std::shared_ptr<SharedDryBuffer> sharedDry;
};

// Per-slot info
struct PluginSlot
{
    juce::AudioProcessorGraph::NodeID nodeId;
    juce::AudioProcessorGraph::NodeID dryCaptureNodeId; // DryCaptureProcessor before this plugin
    juce::AudioProcessorGraph::NodeID dryWetNodeId;     // DryWetProcessor after this plugin
    bool bypassed = false;
    float dryWet = 1.0f;
};

struct ParallelChain
{
    juce::AudioProcessorGraph::NodeID gainNodeId;
    std::vector<PluginSlot> slots;
    float volume = 1.0f;
};

class ChainGraph
{
public:
    ChainGraph();

    void init (juce::AudioProcessorGraph& graph);

    juce::AudioProcessorGraph::NodeID addPlugin (
        juce::AudioProcessorGraph& graph,
        PluginScanner& scanner,
        const juce::PluginDescription& desc,
        int chainIndex = 0,
        int position = -1);

    void removePlugin (juce::AudioProcessorGraph& graph,
                       juce::AudioProcessorGraph::NodeID nodeId);

    void setSlotBypassed (juce::AudioProcessorGraph& graph,
                          juce::AudioProcessorGraph::NodeID nodeId, bool bypassed);
    bool isSlotBypassed (juce::AudioProcessorGraph::NodeID nodeId) const;

    void setSlotDryWet (juce::AudioProcessorGraph& graph,
                        juce::AudioProcessorGraph::NodeID nodeId, float dryWet);
    float getSlotDryWet (juce::AudioProcessorGraph::NodeID nodeId) const;

    void movePlugin (juce::AudioProcessorGraph& graph,
                     int fromChain, int fromSlot, int toChain, int toSlot);

    int addParallelChain (juce::AudioProcessorGraph& graph);
    void removeParallelChain (juce::AudioProcessorGraph& graph, int chainIndex);
    void setChainVolume (juce::AudioProcessorGraph& graph, int chainIndex, float volume);

    void rebuildConnections (juce::AudioProcessorGraph& graph);

    int getNumChains() const { return (int) chains.size(); }
    const ParallelChain& getChain (int index) const { return chains[(size_t) index]; }

    bool findSlot (juce::AudioProcessorGraph::NodeID nodeId, int& chainIndex, int& slotIndex) const;

    std::unique_ptr<juce::XmlElement> toXml (juce::AudioProcessorGraph& graph) const;
    void fromXml (const juce::XmlElement& xml, juce::AudioProcessorGraph& graph,
                  PluginScanner& scanner);

private:
    juce::AudioProcessorGraph::NodeID audioInputNodeId;
    juce::AudioProcessorGraph::NodeID audioOutputNodeId;
    juce::AudioProcessorGraph::NodeID midiInputNodeId;
    juce::AudioProcessorGraph::NodeID midiOutputNodeId;

    std::vector<ParallelChain> chains;

    PluginSlot* findSlotMutable (juce::AudioProcessorGraph::NodeID nodeId);

    void connectNodes (juce::AudioProcessorGraph& graph,
                       juce::AudioProcessorGraph::NodeID src,
                       juce::AudioProcessorGraph::NodeID dst,
                       bool midi = false);
};
