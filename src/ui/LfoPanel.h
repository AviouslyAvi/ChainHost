#pragma once
// LfoPanel — Serum-style LFO control panel with 4 LFOs
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"
#include "FabKnob.h"
#include "LfoWaveformEditor.h"
#include "../LfoEngine.h"

class ChainHostProcessor;

class LfoPanel : public juce::Component
{
public:
    LfoPanel (ChainHostProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void updatePhase();
    void setupMacroDropHandlers();
private:
    ChainHostProcessor& proc;
    int activeLfo = 0;

    juce::TextButton lfoTabs[LfoEngine::numLfos];

    juce::TextButton enableButton { "OFF" };
    juce::ComboBox shapeBox;
    FabKnob rateKnob { "Rate", Colors::lfoBlue }, depthKnob { "Depth", Colors::lfoBlue };
    FabKnob riseKnob { "Rise", Colors::lfoBlue }, smoothKnob { "Smooth", Colors::lfoBlue };

    juce::TextButton syncButton { "FREE" };
    juce::TextButton retrigButton { "TRIG" };
    juce::TextButton polarButton { "UNI" };
    juce::TextButton envButton { "ENV" };

    juce::ComboBox syncDivBox;

    LfoWaveformEditor waveformEditor;
    juce::TextButton toolPointer { "PTR" }, toolPencil { "STEP" }, toolEraser { "ERASE" },
                     toolLine { "LINE" }, toolStairs { "STAIR" };

    juce::ComboBox gridXBox, gridYBox;

    juce::TextButton addTargetButton { "LINK" };

    struct TargetRow {
        juce::Label label;
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
    };
    juce::OwnedArray<TargetRow> targetRows;

    void setActiveLfo (int index);
    void showTargetMenu (int lfoIndex);
    void refreshTargetRows();
    void syncControlsToLfo();
};
