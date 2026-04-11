#include "LfoEngine.h"

//==============================================================================
// Sync division helpers
//==============================================================================

juce::String syncDivName (SyncDiv d)
{
    switch (d)
    {
        case Sync_8Bar:   return "8 Bar";
        case Sync_4Bar:   return "4 Bar";
        case Sync_2Bar:   return "2 Bar";
        case Sync_1Bar:   return "1 Bar";   case Sync_1BarD: return "1 Bar D"; case Sync_1BarT: return "1 Bar T";
        case Sync_1_2:    return "1/2";      case Sync_1_2D:  return "1/2 D";   case Sync_1_2T:  return "1/2 T";
        case Sync_1_4:    return "1/4";      case Sync_1_4D:  return "1/4 D";   case Sync_1_4T:  return "1/4 T";
        case Sync_1_8:    return "1/8";      case Sync_1_8D:  return "1/8 D";   case Sync_1_8T:  return "1/8 T";
        case Sync_1_16:   return "1/16";     case Sync_1_16D: return "1/16 D";  case Sync_1_16T: return "1/16 T";
        case Sync_1_32:   return "1/32";     case Sync_1_32D: return "1/32 D";  case Sync_1_32T: return "1/32 T";
        case Sync_1_64:   return "1/64";
        default:          return "?";
    }
}

// Returns the length of the division in quarter-note beats
float syncDivToBeats (SyncDiv d)
{
    switch (d)
    {
        case Sync_8Bar:   return 32.0f;
        case Sync_4Bar:   return 16.0f;
        case Sync_2Bar:   return 8.0f;
        case Sync_1Bar:   return 4.0f;   case Sync_1BarD: return 6.0f;   case Sync_1BarT: return 4.0f * 2.0f / 3.0f;
        case Sync_1_2:    return 2.0f;   case Sync_1_2D:  return 3.0f;   case Sync_1_2T:  return 2.0f * 2.0f / 3.0f;
        case Sync_1_4:    return 1.0f;   case Sync_1_4D:  return 1.5f;   case Sync_1_4T:  return 1.0f * 2.0f / 3.0f;
        case Sync_1_8:    return 0.5f;   case Sync_1_8D:  return 0.75f;  case Sync_1_8T:  return 0.5f * 2.0f / 3.0f;
        case Sync_1_16:   return 0.25f;  case Sync_1_16D: return 0.375f; case Sync_1_16T: return 0.25f * 2.0f / 3.0f;
        case Sync_1_32:   return 0.125f; case Sync_1_32D: return 0.1875f;case Sync_1_32T: return 0.125f * 2.0f / 3.0f;
        case Sync_1_64:   return 0.0625f;
        default:          return 1.0f;
    }
}

float syncDivToHz (SyncDiv d, double bpm)
{
    if (bpm <= 0.0) bpm = 120.0;
    float beats = syncDivToBeats (d);
    float secondsPerBeat = 60.0f / (float) bpm;
    float periodSec = beats * secondsPerBeat;
    return (periodSec > 0.0f) ? (1.0f / periodSec) : 1.0f;
}

//==============================================================================
// LfoEngine
//==============================================================================

LfoEngine::LfoEngine() {}
LfoEngine::~LfoEngine() { stopTimer(); }

void LfoEngine::start (MacroManager& macros, juce::AudioProcessorGraph& graph, const ChainGraph& chainGraph)
{
    pendingMacros = &macros;
    pendingGraph = &graph;
    pendingChainGraph = &chainGraph;
    lastTimeMs = juce::Time::getMillisecondCounterHiRes();
    startTimer (16); // ~60 Hz
}

void LfoEngine::stop()
{
    stopTimer();
    pendingMacros = nullptr;
    pendingGraph = nullptr;
    pendingChainGraph = nullptr;
}

void LfoEngine::prepareBlock (juce::AudioPlayHead* playHead, const juce::MidiBuffer& midi)
{
    // Read BPM from host
    if (playHead)
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                currentBpm.store (*bpm, std::memory_order_relaxed);
        }
    }

    // Detect note-on events for retrigger
    for (const auto metadata : midi)
    {
        if (metadata.getMessage().isNoteOn())
        {
            noteOnFlag.store (true, std::memory_order_relaxed);
            break;
        }
    }
}

void LfoEngine::timerCallback()
{
    if (pendingMacros && pendingGraph && pendingChainGraph)
        process (*pendingMacros, *pendingGraph, *pendingChainGraph);
}

void LfoEngine::process (MacroManager& macros, juce::AudioProcessorGraph& graph, const ChainGraph& chainGraph)
{
    pendingMacros = &macros;
    pendingGraph = &graph;
    pendingChainGraph = &chainGraph;

    double now = juce::Time::getMillisecondCounterHiRes();
    double deltaSec = (now - lastTimeMs) / 1000.0;
    lastTimeMs = now;

    if (deltaSec <= 0.0 || deltaSec > 1.0)
        deltaSec = 1.0 / 60.0;

    double bpm = currentBpm.load (std::memory_order_relaxed);
    bool hadNoteOn = noteOnFlag.exchange (false, std::memory_order_relaxed);

    for (int li = 0; li < numLfos; ++li)
    {
        auto& lfo = lfos[li];
        if (! lfo.enabled || lfo.targets.empty())
            continue;

        // ── Retrigger on note-on ─────────────────────────────────
        if (hadNoteOn && lfo.retrigger)
        {
            lfo.phase = 0.0f;
            lfo.risePhase = (lfo.riseTime > 0.0f) ? 0.0f : 1.0f;
            lfo.envelopeDone = false;
            lfo.randomNeedsUpdate = true;
        }

        // ── Skip if envelope already done (hold final value) ─────
        if (lfo.envelopeMode && lfo.envelopeDone)
        {
            float output = juce::jlimit (0.0f, 1.0f, lfo.smoothedOutput);
            for (auto& target : lfo.targets)
            {
                if (target.type == LfoTarget::Macro)
                    macros.setMacroValue (target.macroIndex, output, graph, chainGraph);
                else if (auto* node = graph.getNodeForId (target.nodeId))
                {
                    auto& params = node->getProcessor()->getParameters();
                    if (target.paramIndex >= 0 && target.paramIndex < params.size())
                        params[target.paramIndex]->setValue (output);
                }
            }
            continue;
        }

        // ── Calculate effective rate ─────────────────────────────
        float effectiveRate = lfo.tempoSync
            ? syncDivToHz (lfo.syncDiv, bpm)
            : lfo.rate;

        // ── Advance phase ────────────────────────────────────────
        float prevPhase = lfo.phase;
        lfo.phase += (float) (effectiveRate * deltaSec);

        if (lfo.envelopeMode)
        {
            // One-shot: clamp at 1.0
            if (lfo.phase >= 1.0f)
            {
                lfo.phase = 1.0f;
                lfo.envelopeDone = true;
            }
        }
        else
        {
            // Free-running: wrap
            if (lfo.phase >= 1.0f)
            {
                lfo.randomNeedsUpdate = true;
                lfo.phase -= std::floor (lfo.phase);
            }
        }

        // ── S&H / S&G random update ─────────────────────────────
        if (lfo.randomNeedsUpdate && (lfo.shape == SampleHold || lfo.shape == SampleGlide))
        {
            lfo.prevRandom = lfo.lastRandom;
            lfo.lastRandom = juce::Random::getSystemRandom().nextFloat();
            lfo.randomNeedsUpdate = false;
        }

        // ── Compute raw waveform value ───────────────────────────
        float raw;
        if (lfo.useCustomShape && ! lfo.breakpoints.empty())
            raw = customWaveformAt (lfo.breakpoints, lfo.phase);
        else
            raw = waveformAt (lfo.shape, lfo.phase, lfo.lastRandom, lfo.prevRandom);

        // raw is 0–1 (unipolar native)

        // ── Apply unipolar / bipolar ─────────────────────────────
        float modVal;
        if (lfo.unipolar)
        {
            // depth scales from center (0.5) outward
            modVal = 0.5f + (raw - 0.5f) * lfo.depth;
        }
        else
        {
            // bipolar: -1 to +1 mapped to 0–1 param range
            float bipolar = (raw - 0.5f) * 2.0f; // -1 to +1
            modVal = 0.5f + bipolar * lfo.depth * 0.5f;
        }

        // ── Rise envelope ────────────────────────────────────────
        if (lfo.riseTime > 0.0f && lfo.risePhase < 1.0f)
        {
            lfo.risePhase += (float) (deltaSec / (double) lfo.riseTime);
            if (lfo.risePhase > 1.0f) lfo.risePhase = 1.0f;
            // Blend from center (no modulation) to full modulation
            modVal = 0.5f + (modVal - 0.5f) * lfo.risePhase;
        }

        // ── Smoothing (one-pole lowpass) ─────────────────────────
        if (lfo.smooth > 0.001f)
        {
            float coeff = std::exp (-1.0f / (lfo.smooth * 10.0f + 0.01f));
            lfo.smoothedOutput = lfo.smoothedOutput + (1.0f - coeff) * (modVal - lfo.smoothedOutput);
        }
        else
        {
            lfo.smoothedOutput = modVal;
        }

        float output = juce::jlimit (0.0f, 1.0f, lfo.smoothedOutput);

        // ── Apply to targets ─────────────────────────────────────
        for (auto& target : lfo.targets)
        {
            if (target.type == LfoTarget::Macro)
                macros.setMacroValue (target.macroIndex, output, graph, chainGraph);
            else if (auto* node = graph.getNodeForId (target.nodeId))
            {
                auto& params = node->getProcessor()->getParameters();
                if (target.paramIndex >= 0 && target.paramIndex < params.size())
                    params[target.paramIndex]->setValue (output);
            }
        }
    }
}

//==============================================================================
// Waveform generation
//==============================================================================

float LfoEngine::waveformAt (Shape shape, float phase, float lastRandom, float prevRandom)
{
    phase = phase - std::floor (phase);
    switch (shape)
    {
        case Sine:        return 0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi);
        case Triangle:    return 1.0f - std::abs (2.0f * phase - 1.0f);
        case SawUp:       return phase;
        case SawDown:     return 1.0f - phase;
        case Square:      return phase < 0.5f ? 1.0f : 0.0f;
        case SampleHold:  return lastRandom;
        case SampleGlide: return prevRandom + (lastRandom - prevRandom) * phase;
        default:          return 0.5f;
    }
}

float LfoEngine::applyCurve (float t, float curve)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    if (std::abs (curve) < 0.005f)
        return t;

    // Power-based curve: positive = slow start, negative = fast start
    if (curve > 0.0f)
        return std::pow (t, 1.0f + curve * 3.0f);
    else
        return 1.0f - std::pow (1.0f - t, 1.0f + std::abs (curve) * 3.0f);
}

float LfoEngine::customWaveformAt (const std::vector<LfoBreakpoint>& bp, float phase)
{
    if (bp.empty()) return 0.5f;
    if (bp.size() == 1) return bp[0].y;

    phase = juce::jlimit (0.0f, 1.0f, phase);

    // Find the segment: last point with x <= phase
    int seg = 0;
    for (int i = (int) bp.size() - 1; i >= 0; --i)
    {
        if (bp[(size_t) i].x <= phase)
        {
            seg = i;
            break;
        }
    }

    // If we're past the last point, return the last y
    if (seg >= (int) bp.size() - 1)
        return bp.back().y;

    auto& p0 = bp[(size_t) seg];
    auto& p1 = bp[(size_t) seg + 1];

    float dx = p1.x - p0.x;
    if (dx < 0.0001f)
        return p0.y;

    float t = (phase - p0.x) / dx;
    t = applyCurve (t, p0.curve);

    return p0.y + (p1.y - p0.y) * t;
}

std::vector<LfoBreakpoint> LfoEngine::defaultBreakpoints()
{
    // 8-point sine approximation
    return {
        { 0.0f,   0.5f,   0.5f },
        { 0.25f,  1.0f,   0.0f },
        { 0.5f,   0.5f,  -0.5f },
        { 0.75f,  0.0f,   0.0f },
        { 1.0f,   0.5f,   0.5f }
    };
}

//==============================================================================
// Shape names
//==============================================================================

juce::String LfoEngine::shapeName (Shape s)
{
    switch (s)
    {
        case Sine:        return "Sine";
        case Triangle:    return "Tri";
        case SawUp:       return "Saw\xe2\x86\x91";
        case SawDown:     return "Saw\xe2\x86\x93";
        case Square:      return "Sqr";
        case SampleHold:  return "S&H";
        case SampleGlide: return "S&G";
        default:          return "?";
    }
}

//==============================================================================
// Targets
//==============================================================================

void LfoEngine::addTarget (int lfoIndex, const LfoTarget& t)
{
    if (lfoIndex < 0 || lfoIndex >= numLfos) return;
    lfos[lfoIndex].targets.push_back (t);
}

void LfoEngine::removeTarget (int lfoIndex, int targetIndex)
{
    if (lfoIndex < 0 || lfoIndex >= numLfos) return;
    auto& v = lfos[lfoIndex].targets;
    if (targetIndex >= 0 && targetIndex < (int) v.size())
        v.erase (v.begin() + targetIndex);
}

void LfoEngine::clearTargets (int lfoIndex)
{
    if (lfoIndex >= 0 && lfoIndex < numLfos)
        lfos[lfoIndex].targets.clear();
}

void LfoEngine::removeTargetsForNode (juce::AudioProcessorGraph::NodeID nodeId)
{
    for (int li = 0; li < numLfos; ++li)
    {
        auto& v = lfos[li].targets;
        v.erase (std::remove_if (v.begin(), v.end(),
            [&] (const LfoTarget& t) {
                return t.type == LfoTarget::Parameter && t.nodeId == nodeId;
            }), v.end());
    }
}

//==============================================================================
// Serialisation
//==============================================================================

std::unique_ptr<juce::XmlElement> LfoEngine::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("LfoEngine");

    for (int li = 0; li < numLfos; ++li)
    {
        auto& lfo = lfos[li];
        auto* lx = xml->createNewChildElement ("Lfo");
        lx->setAttribute ("index",        li);
        lx->setAttribute ("enabled",      lfo.enabled);
        lx->setAttribute ("shape",        (int) lfo.shape);
        lx->setAttribute ("rate",         (double) lfo.rate);
        lx->setAttribute ("depth",        (double) lfo.depth);
        lx->setAttribute ("tempoSync",    lfo.tempoSync);
        lx->setAttribute ("syncDiv",      (int) lfo.syncDiv);
        lx->setAttribute ("retrigger",    lfo.retrigger);
        lx->setAttribute ("unipolar",     lfo.unipolar);
        lx->setAttribute ("envelopeMode", lfo.envelopeMode);
        lx->setAttribute ("riseTime",     (double) lfo.riseTime);
        lx->setAttribute ("smooth",       (double) lfo.smooth);
        lx->setAttribute ("useCustom",    lfo.useCustomShape);

        // Breakpoints
        if (! lfo.breakpoints.empty())
        {
            auto* bpXml = lx->createNewChildElement ("Breakpoints");
            for (auto& bp : lfo.breakpoints)
            {
                auto* pt = bpXml->createNewChildElement ("P");
                pt->setAttribute ("x", (double) bp.x);
                pt->setAttribute ("y", (double) bp.y);
                pt->setAttribute ("c", (double) bp.curve);
            }
        }

        // Targets
        for (auto& t : lfo.targets)
        {
            auto* tx = lx->createNewChildElement ("Target");
            tx->setAttribute ("type",       (int) t.type);
            tx->setAttribute ("macroIndex", t.macroIndex);
            tx->setAttribute ("nodeId",     (int) t.nodeId.uid);
            tx->setAttribute ("paramIndex", t.paramIndex);
        }
    }
    return xml;
}

void LfoEngine::fromXml (const juce::XmlElement& xml)
{
    for (int li = 0; li < numLfos; ++li)
        lfos[li] = Lfo();

    for (auto* lx : xml.getChildWithTagNameIterator ("Lfo"))
    {
        int li = lx->getIntAttribute ("index", -1);
        if (li < 0 || li >= numLfos) continue;

        auto& lfo = lfos[li];
        lfo.enabled      = lx->getBoolAttribute   ("enabled",      true);
        lfo.shape         = (Shape) lx->getIntAttribute ("shape",   0);
        lfo.rate          = (float) lx->getDoubleAttribute ("rate", 1.0);
        lfo.depth         = (float) lx->getDoubleAttribute ("depth", 0.5);
        lfo.tempoSync     = lx->getBoolAttribute   ("tempoSync",    false);
        lfo.syncDiv       = (SyncDiv) lx->getIntAttribute ("syncDiv", Sync_1_4);
        lfo.retrigger     = lx->getBoolAttribute   ("retrigger",    true);
        lfo.unipolar      = lx->getBoolAttribute   ("unipolar",     true);
        lfo.envelopeMode  = lx->getBoolAttribute   ("envelopeMode", false);
        lfo.riseTime      = (float) lx->getDoubleAttribute ("riseTime", 0.0);
        lfo.smooth        = (float) lx->getDoubleAttribute ("smooth",   0.0);
        lfo.useCustomShape = lx->getBoolAttribute  ("useCustom",    false);

        // Breakpoints
        if (auto* bpXml = lx->getChildByName ("Breakpoints"))
        {
            for (auto* pt : bpXml->getChildWithTagNameIterator ("P"))
            {
                LfoBreakpoint bp;
                bp.x     = (float) pt->getDoubleAttribute ("x", 0.0);
                bp.y     = (float) pt->getDoubleAttribute ("y", 0.5);
                bp.curve = (float) pt->getDoubleAttribute ("c", 0.0);
                lfo.breakpoints.push_back (bp);
            }
        }

        // Targets
        for (auto* tx : lx->getChildWithTagNameIterator ("Target"))
        {
            LfoTarget t;
            t.type       = (LfoTarget::Type) tx->getIntAttribute ("type", 0);
            t.macroIndex = tx->getIntAttribute ("macroIndex", 0);
            t.nodeId     = juce::AudioProcessorGraph::NodeID (
                               (juce::uint32) tx->getIntAttribute ("nodeId", 0));
            t.paramIndex = tx->getIntAttribute ("paramIndex", 0);
            lfo.targets.push_back (t);
        }
    }
}
