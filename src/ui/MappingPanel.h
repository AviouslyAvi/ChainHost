#pragma once
// MappingPanel — macro parameter mapping UI with MIDI learn
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Colors.h"
#include "FabKnob.h"
#include "../MacroManager.h"

class ChainHostProcessor;

class MappingPanel : public juce::Component
{
public:
    MappingPanel (ChainHostProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSelectedMacro (int i);
    void refresh();
    void startLearn (int macroIndex);
    void stopLearn();
    bool isLearning() const { return learningMacroIndex >= 0; }
    int getLearningMacro() const { return learningMacroIndex; }
    void checkLearn();
    std::function<void()> onMappingChanged;
private:
    ChainHostProcessor& proc;
    int selectedMacro = 0, learningMacroIndex = -1;
    struct ParamSnapshot { juce::AudioProcessorGraph::NodeID nodeId; int paramIndex; float value; };
    std::vector<ParamSnapshot> learnSnapshot;
    juce::TextButton macroSelectButtons[MacroManager::numMacros];
    juce::TextButton mapButton { "+ MAP" }, learnButton { "LEARN" };
    struct MappingRow {
        juce::Label label;
        FabKnob minKnob { "Min", Colors::textMid }, maxKnob { "Max", Colors::textMid };
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
        juce::String slotUid; int paramIndex;
    };
    juce::OwnedArray<MappingRow> mappingRows;
    void showParameterPicker();
    void takeParamSnapshot();
};
