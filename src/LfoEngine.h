#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MacroManager.h"

struct LfoTarget
{
    enum Type { Macro, Parameter };
    Type type = Macro;
    int macroIndex = 0;                                    // used when type == Macro
    juce::AudioProcessorGraph::NodeID nodeId {};           // used when type == Parameter
    int paramIndex = 0;                                    // used when type == Parameter
};

class LfoEngine : private juce::Timer
{
public:
    enum Shape { Sine = 0, Triangle, SawUp, Square, Random, NumShapes };

    static constexpr int numLfos = 2;
    static constexpr float kMinRate = 0.01f;
    static constexpr float kMaxRate = 20.0f;

    struct Lfo
    {
        bool enabled = false;
        Shape shape = Sine;
        float rate = 1.0f;      // Hz
        float depth = 0.5f;     // 0–1
        float phase = 0.0f;     // 0–1 internal accumulator
        float lastRandom = 0.5f;
        bool randomNeedsUpdate = true;
        std::vector<LfoTarget> targets;
    };

    LfoEngine();
    ~LfoEngine() override;

    void start (MacroManager& macros, juce::AudioProcessorGraph& graph);
    void stop();

    // Called each tick — modulates targets
    void process (MacroManager& macros, juce::AudioProcessorGraph& graph);

    Lfo& getLfo (int index)             { return lfos[juce::jlimit (0, numLfos - 1, index)]; }
    const Lfo& getLfo (int index) const { return lfos[juce::jlimit (0, numLfos - 1, index)]; }

    // Waveform value at a given phase (0–1), returns 0–1
    static float waveformAt (Shape shape, float phase, float lastRandom);

    void addTarget (int lfoIndex, const LfoTarget& t);
    void removeTarget (int lfoIndex, int targetIndex);
    void clearTargets (int lfoIndex);
    void removeTargetsForNode (juce::AudioProcessorGraph::NodeID nodeId);

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

    static juce::String shapeName (Shape s);

private:
    void timerCallback() override;

    Lfo lfos[numLfos];
    double lastTimeMs = 0.0;

    // Deferred process call — set by timer, executed externally
    MacroManager* pendingMacros = nullptr;
    juce::AudioProcessorGraph* pendingGraph = nullptr;
};
