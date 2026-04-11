#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MacroManager.h"
#include <atomic>

struct LfoTarget
{
    enum Type { Macro, Parameter };
    Type type = Macro;
    int macroIndex = 0;
    juce::AudioProcessorGraph::NodeID nodeId {};
    int paramIndex = 0;
};

//==============================================================================
// Breakpoint for custom waveform drawing (Serum-style)
struct LfoBreakpoint
{
    float x = 0.0f;      // 0–1 normalised time position
    float y = 0.5f;       // 0–1 value
    float curve = 0.0f;   // -1 to +1: curve of segment TO the next point
                          //   0 = linear, >0 = exp (slow start), <0 = log (fast start)
};

//==============================================================================
// Tempo-sync note divisions (Serum order: slow → fast, with dotted/triplet)
enum SyncDiv
{
    Sync_8Bar = 0,
    Sync_4Bar, Sync_2Bar,
    Sync_1Bar,   Sync_1BarD,  Sync_1BarT,
    Sync_1_2,    Sync_1_2D,   Sync_1_2T,
    Sync_1_4,    Sync_1_4D,   Sync_1_4T,
    Sync_1_8,    Sync_1_8D,   Sync_1_8T,
    Sync_1_16,   Sync_1_16D,  Sync_1_16T,
    Sync_1_32,   Sync_1_32D,  Sync_1_32T,
    Sync_1_64,
    NumSyncDivs
};

juce::String syncDivName (SyncDiv d);
float syncDivToBeats (SyncDiv d);           // length in quarter-note beats
float syncDivToHz (SyncDiv d, double bpm);  // frequency at a given BPM

//==============================================================================
class LfoEngine : private juce::Timer
{
public:
    enum Shape { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold, SampleGlide, NumShapes };

    static constexpr int numLfos = 4;
    static constexpr float kMinRate = 0.01f;
    static constexpr float kMaxRate = 30.0f;

    struct Lfo
    {
        bool enabled = false;
        Shape shape = Sine;
        float rate = 1.0f;          // Hz (when not tempo-synced)
        float depth = 0.5f;         // 0–1

        // Phase state
        float phase = 0.0f;         // 0–1 accumulator
        float lastRandom = 0.5f;
        float prevRandom = 0.5f;    // previous S&H value (for SampleGlide interp)
        bool randomNeedsUpdate = true;

        // ── Serum-style features ─────────────────────────────────
        bool tempoSync = false;
        SyncDiv syncDiv = Sync_1_4;

        bool retrigger = true;      // reset phase on note-on
        bool unipolar = true;       // 0→1 (true) vs -1→+1 (false)
        bool envelopeMode = false;  // one-shot: play once then hold
        bool envelopeDone = false;

        float riseTime = 0.0f;     // seconds — LFO fades in from zero
        float risePhase = 1.0f;    // 0→1 ramp, starts at 1 when no rise

        float smooth = 0.0f;       // 0–1 smoothing amount
        float smoothedOutput = 0.5f;

        // ── Custom waveform (breakpoints) ────────────────────────
        bool useCustomShape = false;
        std::vector<LfoBreakpoint> breakpoints;

        // ── Targets ──────────────────────────────────────────────
        std::vector<LfoTarget> targets;
    };

    LfoEngine();
    ~LfoEngine() override;

    void start (MacroManager& macros, juce::AudioProcessorGraph& graph);
    void stop();

    // Call from processBlock to feed tempo + note-on info
    void prepareBlock (juce::AudioPlayHead* playHead, const juce::MidiBuffer& midi);

    void process (MacroManager& macros, juce::AudioProcessorGraph& graph);

    Lfo& getLfo (int index)             { return lfos[juce::jlimit (0, numLfos - 1, index)]; }
    const Lfo& getLfo (int index) const { return lfos[juce::jlimit (0, numLfos - 1, index)]; }

    // Waveform helpers
    static float waveformAt (Shape shape, float phase, float lastRandom, float prevRandom = 0.5f);
    static float customWaveformAt (const std::vector<LfoBreakpoint>& bp, float phase);
    static float applyCurve (float t, float curve);

    // Targets
    void addTarget (int lfoIndex, const LfoTarget& t);
    void removeTarget (int lfoIndex, int targetIndex);
    void clearTargets (int lfoIndex);
    void removeTargetsForNode (juce::AudioProcessorGraph::NodeID nodeId);

    // Serialisation
    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

    static juce::String shapeName (Shape s);

    // Default breakpoints for a sine-like custom shape
    static std::vector<LfoBreakpoint> defaultBreakpoints();

private:
    void timerCallback() override;

    Lfo lfos[numLfos];
    double lastTimeMs = 0.0;

    MacroManager* pendingMacros = nullptr;
    juce::AudioProcessorGraph* pendingGraph = nullptr;

    // Thread-safe data fed from audio thread via prepareBlock
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<bool>   noteOnFlag { false };
};
