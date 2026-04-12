#pragma once
// LfoPanel — Serum-style LFO control panel with 4 LFOs
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"
#include "ArcKnob.h"
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
    ArcKnob rateKnob { "Rate", Colors::lfoBlue }, depthKnob { "Depth", Colors::lfoBlue };
    ArcKnob delayKnob { "Delay", Colors::lfoBlue }, phaseKnob { "Phase", Colors::lfoBlue };
    ArcKnob riseKnob { "Rise", Colors::lfoBlue }, smoothKnob { "Smooth", Colors::lfoBlue };

    juce::TextButton syncButton { "FREE" };
    juce::TextButton retrigButton { "TRIG" };
    juce::TextButton polarButton { "UNI" };
    juce::TextButton envButton { "ENV" };

    juce::ComboBox syncDivBox;

    LfoWaveformEditor waveformEditor;
    juce::TextButton toolPointer { "PTR" }, toolFlat { "FLAT" },
                     toolRampUp { juce::CharPointer_UTF8 ("\xe2\x86\x97") },
                     toolRampDown { juce::CharPointer_UTF8 ("\xe2\x86\x98") };
    juce::TextButton directionButton { "FWD" };

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
