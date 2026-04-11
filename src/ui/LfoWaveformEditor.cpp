#include "LfoWaveformEditor.h"

LfoWaveformEditor::LfoWaveformEditor() {}

juce::Point<float> LfoWaveformEditor::bpToScreen (const LfoBreakpoint& bp, float w, float h) const
{
    return { bp.x * w, (1.0f - bp.y) * h };
}

int LfoWaveformEditor::hitTestBreakpoint (float mx, float my, float w, float h) const
{
    if (! breakpoints) return -1;
    for (int i = 0; i < (int) breakpoints->size(); ++i)
    {
        auto pt = bpToScreen ((*breakpoints)[(size_t) i], w, h);
        if (std::abs (mx - pt.x) < 7.0f && std::abs (my - pt.y) < 7.0f)
            return i;
    }
    return -1;
}

void LfoWaveformEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0e0e18));
    g.fillRect (b);

    g.setColour (juce::Colour (0x0cffffff));
    g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 0.5f);
    for (int q = 1; q <= 3; ++q)
    {
        float qx = b.getX() + b.getWidth() * q / 4.0f;
        g.drawLine (qx, b.getY(), qx, b.getBottom(), 0.5f);
    }

    float w = b.getWidth(), h = b.getHeight();
    bool isCustom = breakpoints && ! breakpoints->empty();

    {
        juce::Path wave;
        for (int px = 0; px <= (int) w; ++px)
        {
            float phase = (float) px / w;
            float val = isCustom
                ? LfoEngine::customWaveformAt (*breakpoints, phase)
                : LfoEngine::waveformAt (presetShape, phase, 0.5f);
            float py = b.getY() + (1.0f - val) * h;
            if (px == 0) wave.startNewSubPath (b.getX(), py);
            else wave.lineTo (b.getX() + (float) px, py);
        }

        juce::Path fill (wave);
        fill.lineTo (b.getRight(), b.getBottom());
        fill.lineTo (b.getX(), b.getBottom());
        fill.closeSubPath();

        auto col = lfoEnabled ? Colors::lfoBlue : Colors::textDim;
        g.setGradientFill (juce::ColourGradient (
            col.withAlpha (0.25f), 0, b.getY(),
            col.withAlpha (0.02f), 0, b.getBottom(), false));
        g.fillPath (fill);

        g.setColour (lfoEnabled ? Colors::lfoBlue : Colors::textDim.withAlpha (0.5f));
        g.strokePath (wave, juce::PathStrokeType (2.0f));
    }

    if (isCustom)
    {
        for (int i = 0; i < (int) breakpoints->size(); ++i)
        {
            auto pt = bpToScreen ((*breakpoints)[(size_t) i], w, h);
            pt += b.getTopLeft();
            bool dragging = (dragIndex == i);
            g.setColour (dragging ? Colors::accentBright : Colors::lfoBlue);
            g.fillEllipse (pt.x - 4.0f, pt.y - 4.0f, 8.0f, 8.0f);
            g.setColour (Colors::text);
            g.drawEllipse (pt.x - 4.0f, pt.y - 4.0f, 8.0f, 8.0f, 1.0f);
        }
    }

    if (lfoEnabled)
    {
        float val = isCustom
            ? LfoEngine::customWaveformAt (*breakpoints, currentPhase)
            : LfoEngine::waveformAt (presetShape, currentPhase, 0.5f);
        float px = b.getX() + currentPhase * w;
        float py = b.getY() + (1.0f - val) * h;

        g.setColour (Colors::lfoBlue.withAlpha (0.3f));
        g.drawLine (px, b.getY(), px, b.getBottom(), 1.0f);
        g.setColour (Colors::lfoBlue);
        g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
    }

    g.setColour (Colors::border.withAlpha (0.5f));
    g.drawRect (b, 1.0f);
}

void LfoWaveformEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! breakpoints) return;
    auto b = getLocalBounds().toFloat();
    float mx = (float) e.x, my = (float) e.y;

    if (e.mods.isPopupMenu())
    {
        int idx = hitTestBreakpoint (mx, my, b.getWidth(), b.getHeight());
        if (idx >= 0 && (int) breakpoints->size() > 2)
        {
            breakpoints->erase (breakpoints->begin() + idx);
            if (onChanged) onChanged();
            repaint();
        }
        return;
    }

    dragIndex = hitTestBreakpoint (mx, my, b.getWidth(), b.getHeight());

    if (dragIndex < 0)
    {
        float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
        float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());
        LfoBreakpoint newBp { nx, ny, 0.0f };

        auto it = std::lower_bound (breakpoints->begin(), breakpoints->end(), newBp,
            [] (const LfoBreakpoint& a, const LfoBreakpoint& rhs) { return a.x < rhs.x; });
        int insertIdx = (int) (it - breakpoints->begin());
        breakpoints->insert (it, newBp);
        dragIndex = insertIdx;
        if (onChanged) onChanged();
        repaint();
    }
}

void LfoWaveformEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! breakpoints || dragIndex < 0 || dragIndex >= (int) breakpoints->size()) return;
    auto b = getLocalBounds().toFloat();
    auto& bp = (*breakpoints)[(size_t) dragIndex];

    float nx = juce::jlimit (0.0f, 1.0f, ((float) e.x - b.getX()) / b.getWidth());
    float ny = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - b.getY()) / b.getHeight());

    if (dragIndex > 0)
        nx = juce::jmax (nx, (*breakpoints)[(size_t) dragIndex - 1].x + 0.001f);
    if (dragIndex < (int) breakpoints->size() - 1)
        nx = juce::jmin (nx, (*breakpoints)[(size_t) dragIndex + 1].x - 0.001f);

    bp.x = nx;
    bp.y = ny;
    if (onChanged) onChanged();
    repaint();
}

void LfoWaveformEditor::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
}
