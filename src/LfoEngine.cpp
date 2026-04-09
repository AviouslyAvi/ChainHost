#include "LfoEngine.h"

LfoEngine::LfoEngine() {}
LfoEngine::~LfoEngine() { stopTimer(); }

void LfoEngine::start (MacroManager& macros, juce::AudioProcessorGraph& graph)
{
    pendingMacros = &macros;
    pendingGraph = &graph;
    lastTimeMs = juce::Time::getMillisecondCounterHiRes();
    startTimer (16); // ~60 Hz
}

void LfoEngine::stop() { stopTimer(); }

void LfoEngine::timerCallback()
{
    if (pendingMacros && pendingGraph)
        process (*pendingMacros, *pendingGraph);
}

void LfoEngine::process (MacroManager& macros, juce::AudioProcessorGraph& graph)
{
    pendingMacros = &macros;
    pendingGraph = &graph;

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

        float prevPhase = lfo.phase;
        lfo.phase += (float) (lfo.rate * deltaSec);

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
        // Apply depth: lerp from 0.5 (center) toward val
        float modulated = 0.5f + (val - 0.5f) * lfo.depth;
        modulated = juce::jlimit (0.0f, 1.0f, modulated);

        for (auto& target : lfo.targets)
        {
            if (target.type == LfoTarget::Macro)
            {
                macros.setMacroValue (target.macroIndex, modulated, graph);
            }
            else
            {
                if (auto* node = graph.getNodeForId (target.nodeId))
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

std::unique_ptr<juce::XmlElement> LfoEngine::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("LfoEngine");

    for (int li = 0; li < numLfos; ++li)
    {
        auto& lfo = lfos[li];
        auto* lfoXml = xml->createNewChildElement ("Lfo");
        lfoXml->setAttribute ("index", li);
        lfoXml->setAttribute ("enabled", lfo.enabled);
        lfoXml->setAttribute ("shape", (int) lfo.shape);
        lfoXml->setAttribute ("rate", (double) lfo.rate);
        lfoXml->setAttribute ("depth", (double) lfo.depth);

        for (auto& t : lfo.targets)
        {
            auto* tXml = lfoXml->createNewChildElement ("Target");
            tXml->setAttribute ("type", (int) t.type);
            tXml->setAttribute ("macroIndex", t.macroIndex);
            tXml->setAttribute ("nodeId", (int) t.nodeId.uid);
            tXml->setAttribute ("paramIndex", t.paramIndex);
        }
    }
    return xml;
}

void LfoEngine::fromXml (const juce::XmlElement& xml)
{
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

        for (auto* tXml : lfoXml->getChildWithTagNameIterator ("Target"))
        {
            LfoTarget t;
            t.type = (LfoTarget::Type) tXml->getIntAttribute ("type", 0);
            t.macroIndex = tXml->getIntAttribute ("macroIndex", 0);
            t.nodeId = juce::AudioProcessorGraph::NodeID (
                (juce::uint32) tXml->getIntAttribute ("nodeId", 0));
            t.paramIndex = tXml->getIntAttribute ("paramIndex", 0);
            lfo.targets.push_back (t);
        }
    }
}
