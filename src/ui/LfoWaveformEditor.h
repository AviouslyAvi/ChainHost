#pragma once
// LfoWaveformEditor — interactive breakpoint waveform editor with curved segments
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"
#include "../LfoEngine.h"

class LfoWaveformEditor : public juce::Component
{
public:
    LfoWaveformEditor();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setBreakpoints (std::vector<LfoBreakpoint>* bp) { breakpoints = bp; repaint(); }
    void setPhase (float p) { currentPhase = p; }
    void setEnabled (bool e) { lfoEnabled = e; }
    void setPresetShape (LfoEngine::Shape s) { presetShape = s; repaint(); }
    std::function<void()> onChanged;

private:
    std::vector<LfoBreakpoint>* breakpoints = nullptr;
    int dragIndex = -1;
    float currentPhase = 0.0f;
    bool lfoEnabled = false;
    LfoEngine::Shape presetShape = LfoEngine::Sine;

    int hitTestBreakpoint (float mx, float my, float w, float h) const;
    juce::Point<float> bpToScreen (const LfoBreakpoint& bp, float w, float h) const;
};
