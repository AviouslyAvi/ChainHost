#pragma once
// LfoWaveformEditor — interactive breakpoint waveform editor with curved segments, grid snap,
//                      multi-select (Cmd/Ctrl+drag), and undo (Cmd+Z)
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"
#include "../LfoEngine.h"

class LfoWaveformEditor : public juce::Component,
                          public juce::KeyListener
{
public:
    LfoWaveformEditor();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;

    void setBreakpoints (std::vector<LfoBreakpoint>* bp) { breakpoints = bp; repaint(); }
    void setPhase (float p) { currentPhase = p; }
    void setDirection (LfoEngine::Direction d) { direction = d; }
    void setEnabled (bool e) { lfoEnabled = e; }
    std::function<void()> onChanged;

    enum Tool { PointerTool, FlatTool, RampUpTool, RampDownTool };
    Tool currentTool = PointerTool;
    void setTool (Tool t) { currentTool = t; repaint(); }
    Tool getTool() const { return currentTool; }

    std::function<bool()> isEnvelopeMode;

    int gridX = 16;
    int gridY = 8;
    void setGridX (int v) { gridX = v; repaint(); }
    void setGridY (int v) { gridY = v; repaint(); }
    static constexpr int maxUndoSteps = 50;

private:
    std::vector<LfoBreakpoint>* breakpoints = nullptr;
    int dragIndex = -1;
    bool draggingCurve = false;
    int curveSegIndex = -1;
    float currentPhase = 0.0f;
    bool lfoEnabled = false;
    LfoEngine::Direction direction = LfoEngine::Forward;

    // Multi-select
    std::set<int> selectedIndices;
    bool draggingSelection = false;    // Cmd/Ctrl rubber-band select
    juce::Point<float> selectStart;
    juce::Rectangle<float> selectRect;
    bool movingSelected = false;       // dragging selected group
    float moveDragStartX = 0.0f, moveDragStartY = 0.0f;

    // Drawing
    bool isDrawing = false;

    int hoverCurveIndex = -1;  // curve handle index under mouse

    int hitTestCurveHandle (float mx, float my, float w, float h) const;

    // Undo
    std::vector<std::vector<LfoBreakpoint>> undoStack;
    void pushUndo();

    float snapToGrid (float val, int divisions, bool snap) const;
    int hitTestBreakpoint (float mx, float my, float w, float h) const;
    juce::Point<float> bpToScreen (const LfoBreakpoint& bp, float w, float h) const;
};
