#include "LfoEngine.h"
#include "ChainGraph.h"

LfoEngine::LfoEngine() {}
LfoEngine::~LfoEngine() { stopTimer(); }

juce::String LfoEngine::divisionName (int div)
{
    switch (div)
    {
        case 1:   return "1/1";
        case 2:   return "1/2";
        case 4:   return "1/4";
        case 8:   return "1/8";
        case 16:  return "1/16";
        case 32:  return "1/32";
        case 64:  return "1/64";
        case 128: return "1/128";
        default:  return "1/" + juce::String (div);
    }
}

float LfoEngine::divisionToHz (int div, float bpm)
{
    // Period of one cycle = (4 / div) beats. At bpm beats/min, period = (4/div) * (60/bpm) sec
    // Rate = 1/period = bpm * div / 240
    return bpm * (float) div / 240.0f;
}

void LfoEngine::start (MacroManager& macros, juce::AudioProcessorGraph& graph, const ChainGraph& cg)
{
    pendingMacros = &macros;
    pendingGraph = &graph;
    pendingChainGraph = &cg;
    lastTimeMs = juce::Time::getMillisecondCounterHiRes();
    startTimer (16); // ~60 Hz
}

void LfoEngine::stop() { stopTimer(); }

void LfoEngine::timerCallback()
{
    if (pendingMacros && pendingGraph && pendingChainGraph)
        process (*pendingMacros, *pendingGraph, *pendingChainGraph);
}

void LfoEngine::process (MacroManager& macros, juce::AudioProcessorGraph& graph, const ChainGraph& chainGraph)
{
    const juce::ScopedLock sl (lock);
    pendingMacros = &macros;
    pendingGraph = &graph;
    pendingChainGraph = &chainGraph;

    double now = juce::Time::getMillisecondCounterHiRes();
    double deltaSec = (now - lastTimeMs) / 1000.0;
    lastTimeMs = now;

    if (deltaSec <= 0.0 || deltaSec > 1.0)
        deltaSec = 1.0 / 60.0;

    for (int li = 0; li < numLfos; ++li)
    {
        auto& lfo = lfos[li];
        if (! lfo.enabled || lfo.targets.empty())
            continue;

        float effectiveRate = lfo.syncToHost
            ? divisionToHz (lfo.noteDivision, hostBpm.load())
            : lfo.rate;
        lfo.phase += (float) (effectiveRate * deltaSec);

        // S&H: update random when phase wraps
        if (lfo.phase >= 1.0f)
        {
            lfo.randomNeedsUpdate = true;
            lfo.phase -= std::floor (lfo.phase);
        }

        if (lfo.randomNeedsUpdate && lfo.shape == Random)
        {
            lfo.lastRandom = juce::Random::getSystemRandom().nextFloat();
            lfo.randomNeedsUpdate = false;
        }

        float val = waveformAt (lfo.shape, lfo.phase, lfo.lastRandom);

        for (auto& target : lfo.targets)
        {
            // Per-target modulation: map waveform (0–1) through center + depth + range
            // val 0.5 = center position, depth scales deviation from center
            float deviation = (val - 0.5f) * lfo.depth;  // –0.5..+0.5
            float norm = target.center + deviation;       // offset by target center
            norm = juce::jlimit (0.0f, 1.0f, norm);
            // Map through target's min/max range
            float modulated = target.minValue + norm * (target.maxValue - target.minValue);
            modulated = juce::jlimit (0.0f, 1.0f, modulated);

            if (target.type == LfoTarget::Macro)
            {
                macros.setMacroValue (target.macroIndex, modulated, graph, chainGraph);
            }
            else
            {
                auto nodeId = chainGraph.getNodeIdForUid (target.slotUid);
                if (auto* node = graph.getNodeForId (nodeId))
                {
                    auto& params = node->getProcessor()->getParameters();
                    if (target.paramIndex >= 0 && target.paramIndex < params.size())
                        params[target.paramIndex]->setValue (modulated);
                }
            }
        }
    }
}

float LfoEngine::waveformAt (Shape shape, float phase, float lastRandom)
{
    phase = phase - std::floor (phase); // ensure 0–1
    switch (shape)
    {
        case Sine:     return 0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi);
        case Triangle: return 1.0f - std::abs (2.0f * phase - 1.0f);
        case SawUp:    return phase;
        case Square:   return phase < 0.5f ? 1.0f : 0.0f;
        case Random:   return lastRandom;
        default:       return 0.5f;
    }
}

juce::String LfoEngine::shapeName (Shape s)
{
    switch (s)
    {
        case Sine:     return "Sine";
        case Triangle: return "Tri";
        case SawUp:    return "Saw";
        case Square:   return "Sqr";
        case Random:   return "S&H";
        default:       return "?";
    }
}

void LfoEngine::addTarget (int lfoIndex, const LfoTarget& t)
{
    if (lfoIndex < 0 || lfoIndex >= numLfos) return;
    const juce::ScopedLock sl (lock);
    lfos[lfoIndex].targets.push_back (t);
}

void LfoEngine::removeTarget (int lfoIndex, int targetIndex)
{
    if (lfoIndex < 0 || lfoIndex >= numLfos) return;
    const juce::ScopedLock sl (lock);
    auto& v = lfos[lfoIndex].targets;
    if (targetIndex >= 0 && targetIndex < (int) v.size())
        v.erase (v.begin() + targetIndex);
}

void LfoEngine::clearTargets (int lfoIndex)
{
    if (lfoIndex >= 0 && lfoIndex < numLfos)
    {
        const juce::ScopedLock sl (lock);
        lfos[lfoIndex].targets.clear();
    }
}

void LfoEngine::removeTargetsForUid (const juce::String& slotUid)
{
    const juce::ScopedLock sl (lock);
    for (int li = 0; li < numLfos; ++li)
    {
        auto& v = lfos[li].targets;
        v.erase (std::remove_if (v.begin(), v.end(),
            [&] (const LfoTarget& t) {
                return t.type == LfoTarget::Parameter && t.slotUid == slotUid;
            }), v.end());
    }
}

std::unique_ptr<juce::XmlElement> LfoEngine::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("LfoEngine");

    const juce::ScopedLock sl (lock);
    for (int li = 0; li < numLfos; ++li)
    {
        auto& lfo = lfos[li];
        auto* lfoXml = xml->createNewChildElement ("Lfo");
        lfoXml->setAttribute ("index", li);
        lfoXml->setAttribute ("enabled", lfo.enabled);
        lfoXml->setAttribute ("shape", (int) lfo.shape);
        lfoXml->setAttribute ("rate", (double) lfo.rate);
        lfoXml->setAttribute ("depth", (double) lfo.depth);
        lfoXml->setAttribute ("syncToHost", lfo.syncToHost);
        lfoXml->setAttribute ("noteDivision", lfo.noteDivision);

        for (auto& t : lfo.targets)
        {
            auto* tXml = lfoXml->createNewChildElement ("Target");
            tXml->setAttribute ("type", (int) t.type);
            tXml->setAttribute ("macroIndex", t.macroIndex);
            tXml->setAttribute ("uid", t.slotUid);
            tXml->setAttribute ("paramIndex", t.paramIndex);
            tXml->setAttribute ("minValue", (double) t.minValue);
            tXml->setAttribute ("maxValue", (double) t.maxValue);
            tXml->setAttribute ("center", (double) t.center);
        }
    }
    return xml;
}

void LfoEngine::fromXml (const juce::XmlElement& xml)
{
    const juce::ScopedLock sl (lock);
    for (int li = 0; li < numLfos; ++li)
    {
        lfos[li] = Lfo(); // reset
    }

    for (auto* lfoXml : xml.getChildWithTagNameIterator ("Lfo"))
    {
        int li = lfoXml->getIntAttribute ("index", -1);
        if (li < 0 || li >= numLfos) continue;

        auto& lfo = lfos[li];
        lfo.enabled = lfoXml->getBoolAttribute ("enabled", false);
        lfo.shape = (Shape) lfoXml->getIntAttribute ("shape", 0);
        lfo.rate = (float) lfoXml->getDoubleAttribute ("rate", 1.0);
        lfo.depth = (float) lfoXml->getDoubleAttribute ("depth", 0.5);
        lfo.syncToHost = lfoXml->getBoolAttribute ("syncToHost", false);
        lfo.noteDivision = lfoXml->getIntAttribute ("noteDivision", 4);

        for (auto* tXml : lfoXml->getChildWithTagNameIterator ("Target"))
        {
            LfoTarget t;
            t.type = (LfoTarget::Type) tXml->getIntAttribute ("type", 0);
            t.macroIndex = tXml->getIntAttribute ("macroIndex", 0);
            t.slotUid = tXml->getStringAttribute ("uid");
            t.paramIndex = tXml->getIntAttribute ("paramIndex", 0);
            t.minValue = (float) tXml->getDoubleAttribute ("minValue", 0.0);
            t.maxValue = (float) tXml->getDoubleAttribute ("maxValue", 1.0);
            t.center   = (float) tXml->getDoubleAttribute ("center", 0.5);
            lfo.targets.push_back (t);
        }
    }
}
