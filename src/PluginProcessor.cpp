#include "PluginProcessor.h"
#include "PluginEditor.h"

ChainHostProcessor::ChainHostProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        auto name = juce::String ("Macro ") + juce::String (i + 1);
        auto paramId = juce::String ("macro") + juce::String (i + 1);
        auto* param = new juce::AudioParameterFloat (
            juce::ParameterID { paramId, 1 }, name, 0.0f, 1.0f, 0.0f);
        macroParams[i] = param;
        addParameter (param);
        param->addListener (this);
    }
}

ChainHostProcessor::~ChainHostProcessor()
{
    lfoEngine.stop();
    for (int i = 0; i < MacroManager::numMacros; ++i)
        macroParams[i]->removeListener (this);
}

void ChainHostProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    graph.setPlayConfigDetails (
        getTotalNumInputChannels(),
        getTotalNumOutputChannels(),
        sampleRate,
        samplesPerBlock);

    if (chainGraph.getNumChains() == 0)
        chainGraph.init (graph);

    graph.prepareToPlay (sampleRate, samplesPerBlock);
    lfoEngine.start (macroManager, graph);
}

void ChainHostProcessor::releaseResources()
{
    graph.releaseResources();
}

void ChainHostProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    graph.processBlock (buffer, midiMessages);
}

juce::AudioProcessorEditor* ChainHostProcessor::createEditor()
{
    return new ChainHostEditor (*this);
}

void ChainHostProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement> ("ChainHostState");

    if (auto graphXml = chainGraph.toXml (graph))
        xml->addChildElement (graphXml.release());

    if (auto macroXml = macroManager.toXml())
        xml->addChildElement (macroXml.release());

    if (auto lfoXml = lfoEngine.toXml())
        xml->addChildElement (lfoXml.release());

    auto* macroVals = xml->createNewChildElement ("MacroValues");
    for (int i = 0; i < MacroManager::numMacros; ++i)
        macroVals->setAttribute ("m" + juce::String (i), (double) macroParams[i]->get());

    copyXmlToBinary (*xml, destData);
}

void ChainHostProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        // Suspend audio processing while we tear down and rebuild the graph
        suspendProcessing (true);

        if (auto* graphXml = xml->getChildByName ("ChainGraph"))
            chainGraph.fromXml (*graphXml, graph, scanner);

        if (auto* macroXml = xml->getChildByName ("MacroMappings"))
            macroManager.fromXml (*macroXml);

        if (auto* lfoXml = xml->getChildByName ("LfoEngine"))
            lfoEngine.fromXml (*lfoXml);

        if (auto* macroVals = xml->getChildByName ("MacroValues"))
        {
            for (int i = 0; i < MacroManager::numMacros; ++i)
            {
                float val = (float) macroVals->getDoubleAttribute ("m" + juce::String (i), 0.0);
                macroParams[i]->setValueNotifyingHost (val);
            }
        }

        suspendProcessing (false);
    }
}

void ChainHostProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    if (parameterIndex >= 0 && parameterIndex < MacroManager::numMacros)
        macroManager.setMacroValue (parameterIndex, newValue, graph);
}

juce::File ChainHostProcessor::getPresetsDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ChainHost")
                   .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChainHostProcessor();
}
