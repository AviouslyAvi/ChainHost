#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MacroManager.h"

struct LfoTarget
{
    enum Type { Macro, Parameter };
    Type type = Macro;
    int macroIndex = 0;                                    // used when type == Macro
    juce::String slotUid;                                  // used when type == Parameter
    int paramIndex = 0;                                    // used when type == Parameter
    float minValue = 0.0f;                                 // bottom of modulation range (0–1)
    float maxValue = 1.0f;                                 // top of modulation range (0–1)
    float center   = 0.5f;                                 // resting position when depth is 0
};

class LfoEngine : private juce::Timer
{
public:
    enum Shape { Sine = 0, Triangle, SawUp, Square, Random, NumShapes };

    static constexpr int numLfos = 2;
    static constexpr float kMinRate = 0.01f;
    static constexpr float kMaxRate = 20.0f;

    // Note divisions for BPM sync mode
    static constexpr int numDivisions = 8;
    static constexpr int divisions[numDivisions] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    static juce::String divisionName (int div);
    static float divisionToHz (int div, float bpm);

    struct Lfo
    {
        bool enabled = false;
        Shape shape = Sine;
        float rate = 1.0f;      // Hz (used in free mode)
        float depth = 0.5f;     // 0–1
        float phase = 0.0f;     // 0–1 internal accumulator
        float lastRandom = 0.5f;
        bool randomNeedsUpdate = true;
        bool syncToHost = false; // true = BPM mode, false = Hz mode
        int noteDivision = 4;   // denominator: 1=whole, 4=quarter, 8=eighth, etc.
        std::vector<LfoTarget> targets;
    };

    LfoEngine();
    ~LfoEngine() override;

    void start (MacroManager& macros, juce::AudioProcessorGraph& graph, const ChainGraph& chainGraph);
    void stop();

    // Called each tick — modulates targets
    void process (MacroManager& macros, juce::AudioProcessorGraph& graph, const ChainGraph& chainGraph);

    Lfo& getLfo (int index)             { return lfos[juce::jlimit (0, numLfos - 1, index)]; }
    const Lfo& getLfo (int index) const { return lfos[juce::jlimit (0, numLfos - 1, index)]; }

    // Waveform value at a given phase (0–1), returns 0–1
    static float waveformAt (Shape shape, float phase, float lastRandom);

    void addTarget (int lfoIndex, const LfoTarget& t);
    void removeTarget (int lfoIndex, int targetIndex);
    void clearTargets (int lfoIndex);
    void removeTargetsForUid (const juce::String& slotUid);

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

    static juce::String shapeName (Shape s);

    void setHostBpm (float bpm) { hostBpm.store (bpm); }
    float getHostBpm() const { return hostBpm.load(); }

private:
    void timerCallback() override;

    Lfo lfos[numLfos];
    double lastTimeMs = 0.0;
    std::atomic<float> hostBpm { 120.0f };

    juce::CriticalSection lock;

    // Deferred process call — set by timer, executed externally
    MacroManager* pendingMacros = nullptr;
    juce::AudioProcessorGraph* pendingGraph = nullptr;
    const ChainGraph* pendingChainGraph = nullptr;
};
