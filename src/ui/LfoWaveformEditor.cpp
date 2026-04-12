#include "LfoWaveformEditor.h"

LfoWaveformEditor::LfoWaveformEditor()
{
    addKeyListener (this);
    setWantsKeyboardFocus (true);
}

void LfoWaveformEditor::pushUndo()
{
    if (! breakpoints) return;
    undoStack.push_back (*breakpoints);
    if ((int) undoStack.size() > maxUndoSteps)
        undoStack.erase (undoStack.begin());
}

float LfoWaveformEditor::snapToGrid (float val, int divisions, bool snap) const
{
    if (! snap) return val;
    float step = 1.0f / (float) divisions;
    return std::round (val / step) * step;
}

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

bool LfoWaveformEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // Ctrl+Z for undo (not Cmd, which conflicts with DAW shortcuts)
    if (key.getModifiers().isCtrlDown() && key.getKeyCode() == 'Z')
    {
        if (! undoStack.empty() && breakpoints)
        {
            *breakpoints = undoStack.back();
            undoStack.pop_back();
            selectedIndices.clear();
            if (onChanged) onChanged();
            repaint();
            return true;
        }
    }

    // Delete/Backspace: delete selected points (keep minimum 2)
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (breakpoints && ! selectedIndices.empty())
        {
            pushUndo();
            // Delete from highest index first to preserve lower indices
            std::vector<int> toDelete (selectedIndices.begin(), selectedIndices.end());
            std::sort (toDelete.rbegin(), toDelete.rend());
            for (int idx : toDelete)
            {
                if ((int) breakpoints->size() <= 2) break;
                breakpoints->erase (breakpoints->begin() + idx);
            }
            selectedIndices.clear();
            if (onChanged) onChanged();
            repaint();
            return true;
        }
    }

    return false;
}

void LfoWaveformEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Background
    g.setColour (juce::Colour (0xff0e0e18));
    g.fillRect (b);

    // Grid lines
    g.setColour (juce::Colour (0x0cffffff));
    for (int gx = 1; gx < gridX; ++gx)
    {
        float xp = b.getX() + b.getWidth() * (float) gx / (float) gridX;
        g.drawLine (xp, b.getY(), xp, b.getBottom(), 0.5f);
    }
    for (int gy = 1; gy < gridY; ++gy)
    {
        float yp = b.getY() + b.getHeight() * (float) gy / (float) gridY;
        g.drawLine (b.getX(), yp, b.getRight(), yp, 0.5f);
    }

    float w = b.getWidth(), h = b.getHeight();
    bool hasPoints = breakpoints && ! breakpoints->empty();

    // Draw waveform
    {
        juce::Path wave;
        for (int px = 0; px <= (int) w; ++px)
        {
            float phase = (float) px / w;
            float val = hasPoints
                ? LfoEngine::customWaveformAt (*breakpoints, phase)
                : 0.5f;
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

    // Draw breakpoint handles
    if (hasPoints)
    {
        for (int i = 0; i < (int) breakpoints->size(); ++i)
        {
            auto pt = bpToScreen ((*breakpoints)[(size_t) i], w, h);
            pt += b.getTopLeft();
            bool isDragging = (dragIndex == i);
            bool isSelected = selectedIndices.count (i) > 0;

            if (isSelected)
                g.setColour (Colors::accentBright);
            else if (isDragging)
                g.setColour (Colors::accentBright);
            else
                g.setColour (Colors::lfoBlue);

            float radius = isSelected ? 5.0f : 4.0f;
            g.fillEllipse (pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (Colors::text);
            g.drawEllipse (pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
        }

        // Tension handles — small circles at midpoint of each segment on the waveform
        for (int i = 0; i < (int) breakpoints->size() - 1; ++i)
        {
            auto& bp0 = (*breakpoints)[(size_t) i];
            auto& bp1 = (*breakpoints)[(size_t) i + 1];
            float midPhase = (bp0.x + bp1.x) * 0.5f;
            float midVal = LfoEngine::customWaveformAt (*breakpoints, midPhase);
            float cx = b.getX() + midPhase * w;
            float cy = b.getY() + (1.0f - midVal) * h;

            bool isActiveCurve = (draggingCurve && curveSegIndex == i);
            float r = isActiveCurve ? 4.0f : 3.0f;
            g.setColour (isActiveCurve ? Colors::accent : Colors::textDim.withAlpha (0.6f));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour (Colors::text.withAlpha (0.5f));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);
        }
    }

    // Rubber-band selection rectangle
    if (draggingSelection)
    {
        g.setColour (Colors::lfoBlue.withAlpha (0.15f));
        g.fillRect (selectRect);
        g.setColour (Colors::lfoBlue.withAlpha (0.5f));
        g.drawRect (selectRect, 1.0f);
    }

    // Phase cursor
    if (lfoEnabled && hasPoints)
    {
        float val = LfoEngine::customWaveformAt (*breakpoints, currentPhase);
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
    bool snapEnabled = ! e.mods.isAltDown();

    // Eraser tool: remove point under cursor
    if (currentTool == EraserTool)
    {
        int idx = hitTestBreakpoint (mx, my, b.getWidth(), b.getHeight());
        if (idx >= 0 && (int) breakpoints->size() > 2)
        {
            pushUndo();
            breakpoints->erase (breakpoints->begin() + idx);
            if (onChanged) onChanged();
            repaint();
        }
        isDrawing = true;
        return;
    }

    // Pencil tool: step draw (or freehand with Alt)
    if (currentTool == PencilTool)
    {
        pushUndo();
        isDrawing = true;
        float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
        float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());

        if (! e.mods.isAltDown())
        {
            // Step draw: snap to grid column, place flat step
            nx = snapToGrid (nx, gridX, true);
            ny = snapToGrid (ny, gridY, true);
        }

        // Add or move existing point at this x
        bool found = false;
        for (auto& bp : *breakpoints)
        {
            if (std::abs (bp.x - nx) < 0.5f / (float) gridX)
            {
                bp.y = ny;
                found = true;
                break;
            }
        }
        if (! found)
        {
            LfoBreakpoint newBp { nx, ny, 0.0f };
            auto it = std::lower_bound (breakpoints->begin(), breakpoints->end(), newBp,
                [] (const LfoBreakpoint& a, const LfoBreakpoint& rhs) { return a.x < rhs.x; });
            breakpoints->insert (it, newBp);
        }
        if (onChanged) onChanged();
        repaint();
        return;
    }

    // Cmd/Ctrl+drag: rubber-band selection
    if (e.mods.isCommandDown())
    {
        draggingSelection = true;
        selectStart = { mx, my };
        selectRect = {};
        return;
    }

    // Right-click: delete selected points (or single clicked point), min 2 remaining
    if (e.mods.isPopupMenu())
    {
        int idx = hitTestBreakpoint (mx, my, b.getWidth(), b.getHeight());

        // If we right-clicked a selected point, delete all selected
        if (idx >= 0 && selectedIndices.count (idx) > 0 && ! selectedIndices.empty())
        {
            pushUndo();
            std::vector<int> toDelete (selectedIndices.begin(), selectedIndices.end());
            std::sort (toDelete.rbegin(), toDelete.rend());
            for (int di : toDelete)
            {
                if ((int) breakpoints->size() <= 2) break;
                breakpoints->erase (breakpoints->begin() + di);
            }
            selectedIndices.clear();
            if (onChanged) onChanged();
            repaint();
        }
        else if (idx >= 0 && (int) breakpoints->size() > 2)
        {
            pushUndo();
            breakpoints->erase (breakpoints->begin() + idx);
            selectedIndices.clear();
            if (onChanged) onChanged();
            repaint();
        }
        else
        {
            // Right-click on tension handle: reset curve to linear
            float w = b.getWidth(), h = b.getHeight();
            for (int i = 0; i < (int) breakpoints->size() - 1; ++i)
            {
                auto& bp0 = (*breakpoints)[(size_t) i];
                auto& bp1 = (*breakpoints)[(size_t) i + 1];
                float midPhase = (bp0.x + bp1.x) * 0.5f;
                float midVal = LfoEngine::customWaveformAt (*breakpoints, midPhase);
                float cx2 = b.getX() + midPhase * w;
                float cy2 = b.getY() + (1.0f - midVal) * h;
                if (std::abs (mx - cx2) < 8.0f && std::abs (my - cy2) < 8.0f)
                {
                    pushUndo();
                    bp0.curve = 0.0f;
                    if (onChanged) onChanged();
                    repaint();
                    break;
                }
            }
        }
        return;
    }

    dragIndex = hitTestBreakpoint (mx, my, b.getWidth(), b.getHeight());

    // Clicked on a selected point: start group move
    if (dragIndex >= 0 && selectedIndices.count (dragIndex) > 0)
    {
        pushUndo();
        movingSelected = true;
        moveDragStartX = mx;
        moveDragStartY = my;
        return;
    }

    // Clicked on an unselected point: clear selection, drag single
    if (dragIndex >= 0)
    {
        pushUndo();
        selectedIndices.clear();
        return;
    }

    // Click on a tension handle (small circle at segment midpoint)
    {
        float w = b.getWidth(), h = b.getHeight();
        for (int i = 0; i < (int) breakpoints->size() - 1; ++i)
        {
            auto& bp0 = (*breakpoints)[(size_t) i];
            auto& bp1 = (*breakpoints)[(size_t) i + 1];
            float midPhase = (bp0.x + bp1.x) * 0.5f;
            float midVal = LfoEngine::customWaveformAt (*breakpoints, midPhase);
            float cx = b.getX() + midPhase * w;
            float cy = b.getY() + (1.0f - midVal) * h;
            if (std::abs (mx - cx) < 8.0f && std::abs (my - cy) < 8.0f)
            {
                pushUndo();
                draggingCurve = true;
                curveSegIndex = i;
                return;
            }
        }
    }

    // Click on empty space: add new point
    {
        pushUndo();
        float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
        float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());
        nx = snapToGrid (nx, gridX, snapEnabled);
        ny = snapToGrid (ny, gridY, snapEnabled);
        LfoBreakpoint newBp { nx, ny, 0.0f };

        auto it = std::lower_bound (breakpoints->begin(), breakpoints->end(), newBp,
            [] (const LfoBreakpoint& a, const LfoBreakpoint& rhs) { return a.x < rhs.x; });
        int insertIdx = (int) (it - breakpoints->begin());
        breakpoints->insert (it, newBp);
        dragIndex = insertIdx;
        selectedIndices.clear();
        if (onChanged) onChanged();
        repaint();
    }
}

void LfoWaveformEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! breakpoints) return;
    auto b = getLocalBounds().toFloat();
    float mx = (float) e.x, my = (float) e.y;
    bool snapEnabled = ! e.mods.isAltDown();

    // Eraser drag: keep removing points under cursor
    if (isDrawing && currentTool == EraserTool)
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

    // Pencil drag: continuous drawing
    if (isDrawing && currentTool == PencilTool)
    {
        float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
        float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());

        if (! e.mods.isAltDown())
        {
            nx = snapToGrid (nx, gridX, true);
            ny = snapToGrid (ny, gridY, true);
        }

        bool found = false;
        for (auto& bp : *breakpoints)
        {
            if (std::abs (bp.x - nx) < 0.5f / (float) gridX)
            {
                bp.y = ny;
                found = true;
                break;
            }
        }
        if (! found)
        {
            LfoBreakpoint newBp { nx, ny, 0.0f };
            auto it = std::lower_bound (breakpoints->begin(), breakpoints->end(), newBp,
                [] (const LfoBreakpoint& a, const LfoBreakpoint& rhs) { return a.x < rhs.x; });
            breakpoints->insert (it, newBp);
        }
        if (onChanged) onChanged();
        repaint();
        return;
    }

    // Rubber-band selection
    if (draggingSelection)
    {
        float x1 = std::min (selectStart.x, mx);
        float y1 = std::min (selectStart.y, my);
        float x2 = std::max (selectStart.x, mx);
        float y2 = std::max (selectStart.y, my);
        selectRect = { x1, y1, x2 - x1, y2 - y1 };

        selectedIndices.clear();
        float w = b.getWidth(), h = b.getHeight();
        for (int i = 0; i < (int) breakpoints->size(); ++i)
        {
            auto pt = bpToScreen ((*breakpoints)[(size_t) i], w, h) + b.getTopLeft();
            if (selectRect.contains (pt))
                selectedIndices.insert (i);
        }
        repaint();
        return;
    }

    // Curve adjustment mode
    if (draggingCurve && curveSegIndex >= 0 && curveSegIndex < (int) breakpoints->size())
    {
        auto& p0 = (*breakpoints)[(size_t) curveSegIndex];
        auto& p1 = (*breakpoints)[(size_t) curveSegIndex + 1];

        // Mouse position in normalised coords
        float mouseNY = 1.0f - juce::jlimit (0.0f, 1.0f, (my - b.getY()) / b.getHeight());
        // Linear midpoint of the segment
        float midY = (p0.y + p1.y) * 0.5f;
        // How far the mouse is from the linear midpoint, scaled
        float deviation = -(mouseNY - midY) * 3.0f;
        // Flip sign based on segment direction so dragging feels consistent
        if (p1.y < p0.y) deviation = -deviation;

        p0.curve = juce::jlimit (-1.0f, 1.0f, deviation);
        if (onChanged) onChanged();
        repaint();
        return;
    }

    // Group move of selected points
    if (movingSelected && ! selectedIndices.empty())
    {
        float dx = (mx - moveDragStartX) / b.getWidth();
        float dy = -(my - moveDragStartY) / b.getHeight();
        moveDragStartX = mx;
        moveDragStartY = my;

        for (int idx : selectedIndices)
        {
            auto& bp = (*breakpoints)[(size_t) idx];
            float newX = juce::jlimit (0.0f, 1.0f, bp.x + dx);
            float newY = juce::jlimit (0.0f, 1.0f, bp.y + dy);
            bp.x = snapToGrid (newX, gridX, snapEnabled);
            bp.y = snapToGrid (newY, gridY, snapEnabled);
        }

        // Re-sort breakpoints and update selectedIndices
        std::vector<LfoBreakpoint> sorted = *breakpoints;
        std::vector<int> order ((size_t) sorted.size());
        std::iota (order.begin(), order.end(), 0);
        std::sort (order.begin(), order.end(), [&] (int a, int ab) {
            return sorted[(size_t) a].x < sorted[(size_t) ab].x;
        });

        std::set<int> oldSelected = selectedIndices;
        selectedIndices.clear();
        std::vector<LfoBreakpoint> reordered ((size_t) sorted.size());
        for (int i = 0; i < (int) order.size(); ++i)
        {
            reordered[(size_t) i] = sorted[(size_t) order[(size_t) i]];
            if (oldSelected.count (order[(size_t) i]) > 0)
                selectedIndices.insert (i);
        }
        *breakpoints = reordered;

        if (onChanged) onChanged();
        repaint();
        return;
    }

    // Single point drag
    if (dragIndex < 0 || dragIndex >= (int) breakpoints->size()) return;
    auto& bp = (*breakpoints)[(size_t) dragIndex];

    float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
    float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());

    nx = snapToGrid (nx, gridX, snapEnabled);
    ny = snapToGrid (ny, gridY, snapEnabled);

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
    draggingCurve = false;
    curveSegIndex = -1;
    draggingSelection = false;
    movingSelected = false;
    isDrawing = false;
}
