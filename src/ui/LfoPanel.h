#pragma once
// LfoPanel — Serum-style LFO control panel with 4 LFOs
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <set>
#include <vector>
#include "Colors.h"
#include "ArcKnob.h"
#include "LfoWaveformEditor.h"
#include "../LfoEngine.h"

class ChainHostProcessor;

class LfoPanel : public juce::Component,
                 private juce::Timer
{
public:
    LfoPanel (ChainHostProcessor& p);
    ~LfoPanel() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void updatePhase();
    void setupMacroDropHandlers();
private:
    void timerCallback() override;
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
    juce::TextButton toolPointer { "PTR" }, toolFlat { "---" },
                     toolRampUp { juce::CharPointer_UTF8 ("\xe2\x86\x97") },
                     toolRampDown { juce::CharPointer_UTF8 ("\xe2\x86\x98") },
                     toolErase { juce::CharPointer_UTF8 ("\xe2\x8c\xab") };
    juce::TextButton directionButton { "FWD" };

    juce::ComboBox gridXBox, gridYBox;

    juce::TextButton addTargetButton { "LINK" };

    // LFO drag handle — drag onto plugin params to assign as target
    class LfoDragHandle : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        LfoDragHandle() {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override {}
        void mouseDrag (const juce::MouseEvent& e) override;
        int lfoIndex = 0;
    };
    LfoDragHandle lfoDragHandle;

    // LFO link button — opens plugin param picker
    class LfoLinkButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void paint (juce::Graphics& g) override;
        std::function<void()> onClick;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };
    LfoLinkButton lfoLinkBtn;

    // LFO learn button
    juce::TextButton lfoLearnBtn { "LEARN" };
    bool lfoLearning = false;
    std::map<juce::AudioProcessorGraph::NodeID, std::vector<float>> learnSnapshot;
    void startLfoLearn();
    void stopLfoLearn();
    void checkLfoLearn();

    struct TargetRow {
        juce::Label label;
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
    };
    juce::OwnedArray<TargetRow> targetRows;

    void setActiveLfo (int index);
    void showTargetMenu (int lfoIndex);
    void showLfoLinkMenu();
    void refreshTargetRows();
    void syncControlsToLfo();
};
