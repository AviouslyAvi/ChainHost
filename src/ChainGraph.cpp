#include "ChainGraph.h"

ChainGraph::ChainGraph() {}

void ChainGraph::init (juce::AudioProcessorGraph& graph)
{
    graph.clear();
    chains.clear();

    audioInputNodeId  = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode))->nodeID;
    audioOutputNodeId = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode))->nodeID;
    midiInputNodeId   = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode))->nodeID;
    midiOutputNodeId  = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode))->nodeID;

    addParallelChain (graph);
}

juce::AudioProcessorGraph::NodeID ChainGraph::addPlugin (
    juce::AudioProcessorGraph& graph, PluginScanner& scanner,
    const juce::PluginDescription& desc, int chainIndex, int position,
    const juce::String& uid)
{
    if (chainIndex < 0 || chainIndex >= (int) chains.size())
        return {};

    juce::String errorMsg;
    auto instance = scanner.getFormatManager().createPluginInstance (
        desc, graph.getSampleRate(), graph.getBlockSize(), errorMsg);
    if (! instance) return {};

    auto pluginNodeId = graph.addNode (std::move (instance))->nodeID;
    auto* pluginNode = graph.getNodeForId (pluginNodeId);
    int numChans = pluginNode->getProcessor()->getTotalNumInputChannels();

    // Shared buffer links the DryCapture (before plugin) to the DryWet (after plugin)
    auto sharedDry = std::make_shared<SharedDryBuffer>();
    sharedDry->prepare (numChans, graph.getBlockSize());
    
    auto dc = std::make_unique<DryCaptureProcessor> (sharedDry);
    dc->setPlayConfigDetails (numChans, numChans, graph.getSampleRate(), graph.getBlockSize());
    auto dcNodeId = graph.addNode (std::move (dc))->nodeID;

    auto dw = std::make_unique<DryWetProcessor> (sharedDry);
    dw->setPlayConfigDetails (numChans, numChans, graph.getSampleRate(), graph.getBlockSize());
    auto dwNodeId = graph.addNode (std::move (dw))->nodeID;

    PluginSlot slot;
    slot.uid = uid.isEmpty() ? juce::Uuid().toString() : uid;
    slot.nodeId = pluginNodeId;
    slot.dryCaptureNodeId = dcNodeId;
    slot.dryWetNodeId = dwNodeId;

    auto& chain = chains[(size_t) chainIndex];
    if (position < 0 || position >= (int) chain.slots.size())
        chain.slots.push_back (slot);
    else
        chain.slots.insert (chain.slots.begin() + position, slot);

    rebuildConnections (graph);
    return pluginNodeId;
}

void ChainGraph::removePlugin (juce::AudioProcessorGraph& graph,
                               juce::AudioProcessorGraph::NodeID nodeId)
{
    for (auto& chain : chains)
    {
        for (auto it = chain.slots.begin(); it != chain.slots.end(); ++it)
        {
            if (it->nodeId == nodeId)
            {
                graph.removeNode (it->dryWetNodeId);
                graph.removeNode (it->dryCaptureNodeId);
                graph.removeNode (it->nodeId);
                chain.slots.erase (it);
                rebuildConnections (graph);
                return;
            }
        }
    }
}

void ChainGraph::setSlotBypassed (juce::AudioProcessorGraph& graph,
                                  juce::AudioProcessorGraph::NodeID nodeId, bool bypassed)
{
    if (auto* slot = findSlotMutable (nodeId))
    {
        slot->bypassed = bypassed;
        if (auto* node = graph.getNodeForId (nodeId))
            node->setBypassed (bypassed);
    }
}

bool ChainGraph::isSlotBypassed (juce::AudioProcessorGraph::NodeID nodeId) const
{
    for (auto& chain : chains)
        for (auto& slot : chain.slots)
            if (slot.nodeId == nodeId) return slot.bypassed;
    return false;
}

void ChainGraph::setSlotDryWet (juce::AudioProcessorGraph& graph,
                                juce::AudioProcessorGraph::NodeID nodeId, float dryWet)
{
    if (auto* slot = findSlotMutable (nodeId))
    {
        slot->dryWet = juce::jlimit (0.0f, 1.0f, dryWet);

        // Update the actual DryWetProcessor
        if (auto* dwNode = graph.getNodeForId (slot->dryWetNodeId))
            if (auto* dwProc = dynamic_cast<DryWetProcessor*> (dwNode->getProcessor()))
                dwProc->setWet (slot->dryWet);
    }
}

float ChainGraph::getSlotDryWet (juce::AudioProcessorGraph::NodeID nodeId) const
{
    for (auto& chain : chains)
        for (auto& slot : chain.slots)
            if (slot.nodeId == nodeId) return slot.dryWet;
    return 1.0f;
}

void ChainGraph::movePlugin (juce::AudioProcessorGraph& graph,
                             int fromChain, int fromSlot, int toChain, int toSlot)
{
    if (fromChain < 0 || fromChain >= (int) chains.size()) return;
    if (toChain < 0 || toChain >= (int) chains.size()) return;
    if (fromSlot < 0 || fromSlot >= (int) chains[(size_t) fromChain].slots.size()) return;

    auto slot = chains[(size_t) fromChain].slots[(size_t) fromSlot];
    chains[(size_t) fromChain].slots.erase (chains[(size_t) fromChain].slots.begin() + fromSlot);

    if (toSlot < 0 || toSlot >= (int) chains[(size_t) toChain].slots.size())
        chains[(size_t) toChain].slots.push_back (slot);
    else
        chains[(size_t) toChain].slots.insert (chains[(size_t) toChain].slots.begin() + toSlot, slot);

    rebuildConnections (graph);
}

int ChainGraph::addParallelChain (juce::AudioProcessorGraph& graph)
{
    ParallelChain chain;
    chain.gainNodeId = graph.addNode (std::make_unique<GainProcessor>())->nodeID;
    chain.volume = 1.0f;
    chains.push_back (chain);
    rebuildConnections (graph);
    return (int) chains.size() - 1;
}

void ChainGraph::removeParallelChain (juce::AudioProcessorGraph& graph, int chainIndex)
{
    if (chainIndex < 0 || chainIndex >= (int) chains.size() || chains.size() <= 1)
        return;

    auto& chain = chains[(size_t) chainIndex];
    for (auto& slot : chain.slots)
    {
        graph.removeNode (slot.dryWetNodeId);
        graph.removeNode (slot.dryCaptureNodeId);
        graph.removeNode (slot.nodeId);
    }
    graph.removeNode (chain.gainNodeId);
    chains.erase (chains.begin() + chainIndex);
    rebuildConnections (graph);
}

void ChainGraph::setChainVolume (juce::AudioProcessorGraph& graph, int chainIndex, float volume)
{
    if (chainIndex < 0 || chainIndex >= (int) chains.size()) return;
    chains[(size_t) chainIndex].volume = volume;
    if (auto* node = graph.getNodeForId (chains[(size_t) chainIndex].gainNodeId))
        if (auto* gain = dynamic_cast<GainProcessor*> (node->getProcessor()))
            gain->setGain (chains[(size_t) chainIndex].muted ? 0.0f : volume);
}

void ChainGraph::setChainMuted (juce::AudioProcessorGraph& graph, int chainIndex, bool muted)
{
    if (chainIndex < 0 || chainIndex >= (int) chains.size()) return;
    chains[(size_t) chainIndex].muted = muted;
    if (auto* node = graph.getNodeForId (chains[(size_t) chainIndex].gainNodeId))
        if (auto* gain = dynamic_cast<GainProcessor*> (node->getProcessor()))
            gain->setGain (muted ? 0.0f : chains[(size_t) chainIndex].volume);
}

bool ChainGraph::isChainMuted (int chainIndex) const
{
    if (chainIndex < 0 || chainIndex >= (int) chains.size()) return false;
    return chains[(size_t) chainIndex].muted;
}

bool ChainGraph::findSlot (juce::AudioProcessorGraph::NodeID nodeId, int& ci, int& si) const
{
    for (int c = 0; c < (int) chains.size(); ++c)
        for (int s = 0; s < (int) chains[(size_t) c].slots.size(); ++s)
            if (chains[(size_t) c].slots[(size_t) s].nodeId == nodeId)
            { ci = c; si = s; return true; }
    return false;
}

juce::String ChainGraph::getUidForNodeId (juce::AudioProcessorGraph::NodeID nodeId) const
{
    for (auto& chain : chains)
        for (auto& slot : chain.slots)
            if (slot.nodeId == nodeId) return slot.uid;
    return {};
}

juce::AudioProcessorGraph::NodeID ChainGraph::getNodeIdForUid (const juce::String& uid) const
{
    for (auto& chain : chains)
        for (auto& slot : chain.slots)
            if (slot.uid == uid) return slot.nodeId;
    return {};
}

PluginSlot* ChainGraph::findSlotMutable (juce::AudioProcessorGraph::NodeID nodeId)
{
    for (auto& chain : chains)
        for (auto& slot : chain.slots)
            if (slot.nodeId == nodeId) return &slot;
    return nullptr;
}

void ChainGraph::rebuildConnections (juce::AudioProcessorGraph& graph)
{
    for (auto& conn : graph.getConnections())
        graph.removeConnection (conn);

    for (auto& chain : chains)
    {
        if (auto* node = graph.getNodeForId (chain.gainNodeId))
            if (auto* gain = dynamic_cast<GainProcessor*> (node->getProcessor()))
                gain->setGain (chain.volume);

        // Update all DryWetProcessor values
        for (auto& slot : chain.slots)
        {
            if (auto* node = graph.getNodeForId (slot.nodeId))
                node->setBypassed (slot.bypassed);
            if (auto* dwNode = graph.getNodeForId (slot.dryWetNodeId))
                if (auto* dwProc = dynamic_cast<DryWetProcessor*> (dwNode->getProcessor()))
                    dwProc->setWet (slot.dryWet);
        }

        if (chain.slots.empty())
        {
            connectNodes (graph, audioInputNodeId, chain.gainNodeId);
            connectNodes (graph, chain.gainNodeId, audioOutputNodeId);
            connectNodes (graph, midiInputNodeId, midiOutputNodeId, true);
        }
        else
        {
            // Input -> Gain -> DryCapture -> Plugin -> DryWet -> [next DryCapture -> ...] -> Output
            connectNodes (graph, audioInputNodeId, chain.gainNodeId);
            connectNodes (graph, chain.gainNodeId, chain.slots.front().dryCaptureNodeId);
            connectNodes (graph, midiInputNodeId, chain.slots.front().nodeId, true);

            for (size_t i = 0; i < chain.slots.size(); ++i)
            {
                auto& slot = chain.slots[i];

                // dryCapture -> plugin -> dryWet
                connectNodes (graph, slot.dryCaptureNodeId, slot.nodeId);
                connectNodes (graph, slot.nodeId, slot.dryWetNodeId);

                if (i + 1 < chain.slots.size())
                {
                    // dryWet -> next dryCapture
                    connectNodes (graph, slot.dryWetNodeId, chain.slots[i + 1].dryCaptureNodeId);
                    connectNodes (graph, slot.nodeId, chain.slots[i + 1].nodeId, true);
                }
                else
                {
                    // last dryWet -> output
                    connectNodes (graph, slot.dryWetNodeId, audioOutputNodeId);
                    connectNodes (graph, slot.nodeId, midiOutputNodeId, true);
                }
            }
        }
    }
}

void ChainGraph::connectNodes (juce::AudioProcessorGraph& graph,
                               juce::AudioProcessorGraph::NodeID src,
                               juce::AudioProcessorGraph::NodeID dst, bool midi)
{
    if (midi)
    {
        graph.addConnection ({ { src, juce::AudioProcessorGraph::midiChannelIndex },
                               { dst, juce::AudioProcessorGraph::midiChannelIndex } });
    }
    else
    {
        auto* srcNode = graph.getNodeForId (src);
        auto* dstNode = graph.getNodeForId (dst);
        if (! srcNode || ! dstNode) return;
        int chans = juce::jmin (srcNode->getProcessor()->getTotalNumOutputChannels(),
                                dstNode->getProcessor()->getTotalNumInputChannels());
        for (int ch = 0; ch < chans; ++ch)
            graph.addConnection ({ { src, ch }, { dst, ch } });
    }
}

std::unique_ptr<juce::XmlElement> ChainGraph::toXml (juce::AudioProcessorGraph& graph) const
{
    auto xml = std::make_unique<juce::XmlElement> ("ChainGraph");

    for (auto& chain : chains)
    {
        auto* chainXml = xml->createNewChildElement ("Chain");
        chainXml->setAttribute ("volume", (double) chain.volume);
        chainXml->setAttribute ("muted", chain.muted);

        for (auto& slot : chain.slots)
        {
            if (auto* node = graph.getNodeForId (slot.nodeId))
            {
                auto* pluginXml = chainXml->createNewChildElement ("Plugin");
                pluginXml->setAttribute ("uid", slot.uid);
                pluginXml->setAttribute ("bypassed", slot.bypassed);
                pluginXml->setAttribute ("dryWet", (double) slot.dryWet);

                if (auto* inst = dynamic_cast<juce::AudioPluginInstance*> (node->getProcessor()))
                    pluginXml->addChildElement (inst->getPluginDescription().createXml().release());

                juce::MemoryBlock state;
                node->getProcessor()->getStateInformation (state);
                pluginXml->setAttribute ("state", state.toBase64Encoding());
            }
        }
    }
    return xml;
}

void ChainGraph::fromXml (const juce::XmlElement& xml, juce::AudioProcessorGraph& graph,
                          PluginScanner& scanner)
{
    init (graph);
    if (! chains.empty()) { graph.removeNode (chains[0].gainNodeId); chains.clear(); }

    for (auto* chainXml : xml.getChildWithTagNameIterator ("Chain"))
    {
        int ci = addParallelChain (graph);
        setChainVolume (graph, ci, (float) chainXml->getDoubleAttribute ("volume", 1.0));
        setChainMuted (graph, ci, chainXml->getBoolAttribute ("muted", false));

        for (auto* pluginXml : chainXml->getChildWithTagNameIterator ("Plugin"))
        {
            if (auto* descXml = pluginXml->getChildByName ("plugin"))
            {
                juce::PluginDescription desc;
                desc.loadFromXml (*descXml);
                
                auto uid = pluginXml->getStringAttribute ("uid");
                auto nodeId = addPlugin (graph, scanner, desc, ci, -1, uid);

                if (nodeId != juce::AudioProcessorGraph::NodeID())
                {
                    setSlotBypassed (graph, nodeId, pluginXml->getBoolAttribute ("bypassed", false));
                    setSlotDryWet (graph, nodeId, (float) pluginXml->getDoubleAttribute ("dryWet", 1.0));

                    auto stateStr = pluginXml->getStringAttribute ("state");
                    if (stateStr.isNotEmpty())
                    {
                        juce::MemoryBlock state;
                        state.fromBase64Encoding (stateStr);
                        if (auto* node = graph.getNodeForId (nodeId))
                            node->getProcessor()->setStateInformation (state.getData(), (int) state.getSize());
                    }
                }
            }
        }
    }
    rebuildConnections (graph);
}
