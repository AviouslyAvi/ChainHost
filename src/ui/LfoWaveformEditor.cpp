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
    g.setColour (Colors::surfaceRaised);
    g.fillRect (b);

    // Grid lines
    g.setColour (Colors::borderSubtle);
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

            bool isLoopback = (*breakpoints)[(size_t) i].isLoopback;
            float radius = isSelected ? 6.5f : (isLoopback ? 7.0f : 5.5f);
            g.fillEllipse (pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (isLoopback ? Colors::accent : Colors::text);
            g.drawEllipse (pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f, isLoopback ? 1.5f : 1.0f);

            // Loopback arrow indicator
            if (isLoopback)
            {
                g.setColour (Colors::accent);
                g.drawLine (pt.x, b.getY(), pt.x, b.getBottom(), 1.0f);
                g.setFont (juce::Font (juce::FontOptions (8.0f)));
                g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\xa9"), (int)(pt.x - 6), (int)b.getY() + 2, 12, 10, juce::Justification::centred);
            }
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
            float r = isActiveCurve ? 5.5f : 4.0f;
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

    // Phase cursor (direction-aware)
    if (lfoEnabled && hasPoints)
    {
        float effectivePhase = currentPhase;
        if (direction == LfoEngine::Reverse)
            effectivePhase = 1.0f - currentPhase;
        else if (direction == LfoEngine::PingPong)
            effectivePhase = (currentPhase < 0.5f) ? (currentPhase * 2.0f) : (2.0f - currentPhase * 2.0f);

        float val = LfoEngine::customWaveformAt (*breakpoints, effectivePhase);
        float px = b.getX() + effectivePhase * w;
        float py = b.getY() + (1.0f - val) * h;

        g.setColour (Colors::lfoBlue.withAlpha (0.3f));
        g.drawLine (px, b.getY(), px, b.getBottom(), 1.0f);
        g.setColour (Colors::lfoBlue);
        g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
    }

    // Tension percentage tooltip (shown when hovering or dragging a curve handle)
    {
        int showIdx = draggingCurve ? curveSegIndex : hoverCurveIndex;
        if (showIdx >= 0 && breakpoints && showIdx < (int) breakpoints->size())
        {
            float tension = (*breakpoints)[(size_t) showIdx].curve;
            int pct = juce::roundToInt (tension * 100.0f);
            juce::String text = (pct >= 0 ? "+" : "") + juce::String (pct) + "%";

            auto& bp0 = (*breakpoints)[(size_t) showIdx];
            auto& bp1 = (*breakpoints)[(size_t) showIdx + 1];
            float midPhase = (bp0.x + bp1.x) * 0.5f;
            float midVal = LfoEngine::customWaveformAt (*breakpoints, midPhase);
            float tx = b.getX() + midPhase * w;
            float ty = b.getY() + (1.0f - midVal) * h - 14.0f;

            g.setColour (Colors::text.withAlpha (0.9f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (text, (int)(tx - 16), (int)ty, 32, 12, juce::Justification::centred);
        }
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

    // Erase tool: remove breakpoints near click
    if (currentTool == EraseTool)
    {
        pushUndo();
        isDrawing = true;
        float w = b.getWidth(), h = b.getHeight();
        int hit = hitTestBreakpoint (mx, my, w, h);
        if (hit >= 0 && (int) breakpoints->size() > 2)
        {
            breakpoints->erase (breakpoints->begin() + hit);
            selectedIndices.clear();
            if (onChanged) onChanged();
        }
        repaint();
        return;
    }

    // Flat / RampUp / RampDown tools: draw grid-snapped segments
    if (currentTool == FlatTool || currentTool == RampUpTool || currentTool == RampDownTool)
    {
        pushUndo();
        isDrawing = true;
        float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
        float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());
        ny = snapToGrid (ny, gridY, true);

        float colW = 1.0f / (float) gridX;
        float x0 = std::floor (nx / colW) * colW;
        float x1 = x0 + colW;
        x0 = juce::jlimit (0.0f, 1.0f, x0);
        x1 = juce::jlimit (0.0f, 1.0f, x1);

        // Remove existing points in [x0, x1]
        breakpoints->erase (
            std::remove_if (breakpoints->begin(), breakpoints->end(),
                [x0, x1] (const LfoBreakpoint& bp) { return bp.x >= x0 && bp.x <= x1; }),
            breakpoints->end());

        auto insertBp = [&] (float px, float py) {
            LfoBreakpoint bp { px, py, 0.0f };
            auto it = std::lower_bound (breakpoints->begin(), breakpoints->end(), bp,
                [] (const LfoBreakpoint& a, const LfoBreakpoint& rhs) { return a.x < rhs.x; });
            breakpoints->insert (it, bp);
        };

        if (currentTool == FlatTool)
        {
            insertBp (x0, ny);
            insertBp (x1, ny);
        }
        else if (currentTool == RampUpTool)
        {
            float yBottom = snapToGrid (0.0f, gridY, true);
            float yTop = snapToGrid (1.0f, gridY, true);
            insertBp (x0, yBottom);
            insertBp (x1, yTop);
        }
        else // RampDownTool
        {
            float yTop = snapToGrid (1.0f, gridY, true);
            float yBottom = snapToGrid (0.0f, gridY, true);
            insertBp (x0, yTop);
            insertBp (x1, yBottom);
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

    // Right-click: context menu
    if (e.mods.isPopupMenu())
    {
        float w = b.getWidth(), h = b.getHeight();
        int hitIdx = hitTestBreakpoint (mx, my, w, h);
        int curveIdx = hitTestCurveHandle (mx, my, w, h);

        juce::PopupMenu menu;

        if (hitIdx >= 0 && selectedIndices.count (hitIdx) > 0 && ! selectedIndices.empty())
            menu.addItem (1, "Remove Selected Points", (int) breakpoints->size() - (int) selectedIndices.size() >= 2);
        else if (hitIdx >= 0)
            menu.addItem (2, "Remove Point", (int) breakpoints->size() > 2);

        if (hitIdx >= 0 && isEnvelopeMode && isEnvelopeMode())
        {
            menu.addSeparator();
            bool alreadyLoop = (*breakpoints)[(size_t) hitIdx].isLoopback;
            menu.addItem (3, alreadyLoop ? "Clear Loopback Point" : "Set Loopback Point Here");
        }

        if (curveIdx >= 0)
        {
            menu.addSeparator();
            menu.addItem (4, "Reset Curve to Linear");
            menu.addItem (5, "Reset All Curves");
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
            { (int) mx + getScreenX(), (int) my + getScreenY(), 1, 1 }),
            [this, hitIdx, curveIdx] (int result)
            {
                if (result == 1) // Remove selected
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
                else if (result == 2) // Remove single point
                {
                    pushUndo();
                    if (hitIdx >= 0 && hitIdx < (int) breakpoints->size() && (int) breakpoints->size() > 2)
                        breakpoints->erase (breakpoints->begin() + hitIdx);
                    selectedIndices.clear();
                    if (onChanged) onChanged();
                    repaint();
                }
                else if (result == 3) // Toggle loopback
                {
                    pushUndo();
                    bool wasLoop = (*breakpoints)[(size_t) hitIdx].isLoopback;
                    for (auto& bp : *breakpoints) bp.isLoopback = false;
                    if (! wasLoop) (*breakpoints)[(size_t) hitIdx].isLoopback = true;
                    if (onChanged) onChanged();
                    repaint();
                }
                else if (result == 4) // Reset single curve
                {
                    pushUndo();
                    if (curveIdx >= 0 && curveIdx < (int) breakpoints->size())
                        (*breakpoints)[(size_t) curveIdx].curve = 0.0f;
                    if (onChanged) onChanged();
                    repaint();
                }
                else if (result == 5) // Reset all curves
                {
                    pushUndo();
                    for (auto& bp : *breakpoints) bp.curve = 0.0f;
                    if (onChanged) onChanged();
                    repaint();
                }
            });
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

    // Erase tool drag: sweep-delete breakpoints
    if (isDrawing && currentTool == EraseTool)
    {
        float w = b.getWidth(), h = b.getHeight();
        int hit = hitTestBreakpoint (mx, my, w, h);
        if (hit >= 0 && (int) breakpoints->size() > 2)
        {
            breakpoints->erase (breakpoints->begin() + hit);
            selectedIndices.clear();
            if (onChanged) onChanged();
            repaint();
        }
        return;
    }

    // Drawing tool drag: paint segments as mouse moves
    if (isDrawing && (currentTool == FlatTool || currentTool == RampUpTool || currentTool == RampDownTool))
    {
        float nx = juce::jlimit (0.0f, 1.0f, (mx - b.getX()) / b.getWidth());
        float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (my - b.getY()) / b.getHeight());
        ny = snapToGrid (ny, gridY, true);

        float colW = 1.0f / (float) gridX;
        float x0 = std::floor (nx / colW) * colW;
        float x1 = x0 + colW;
        x0 = juce::jlimit (0.0f, 1.0f, x0);
        x1 = juce::jlimit (0.0f, 1.0f, x1);

        breakpoints->erase (
            std::remove_if (breakpoints->begin(), breakpoints->end(),
                [x0, x1] (const LfoBreakpoint& bp) { return bp.x >= x0 && bp.x <= x1; }),
            breakpoints->end());

        auto insertBp = [&] (float px, float py) {
            LfoBreakpoint bp { px, py, 0.0f };
            auto it = std::lower_bound (breakpoints->begin(), breakpoints->end(), bp,
                [] (const LfoBreakpoint& a, const LfoBreakpoint& rhs) { return a.x < rhs.x; });
            breakpoints->insert (it, bp);
        };

        if (currentTool == FlatTool)          { insertBp (x0, ny); insertBp (x1, ny); }
        else if (currentTool == RampUpTool)   { insertBp (x0, 0.0f); insertBp (x1, 1.0f); }
        else                                  { insertBp (x0, 1.0f); insertBp (x1, 0.0f); }

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

        float newCurve = juce::jlimit (-1.0f, 1.0f, deviation);
        if (e.mods.isAltDown())
        {
            // Alt-drag: apply to ALL curves
            for (int i = 0; i < (int) breakpoints->size() - 1; ++i)
                (*breakpoints)[(size_t) i].curve = newCurve;
        }
        else
        {
            p0.curve = newCurve;
        }
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
    repaint();
}

void LfoWaveformEditor::mouseMove (const juce::MouseEvent& e)
{
    if (! breakpoints) return;
    auto b = getLocalBounds().toFloat();
    float mx = (float) e.x, my = (float) e.y;
    int newHover = hitTestCurveHandle (mx, my, b.getWidth(), b.getHeight());
    if (newHover != hoverCurveIndex)
    {
        hoverCurveIndex = newHover;
        repaint();
    }
}

int LfoWaveformEditor::hitTestCurveHandle (float mx, float my, float w, float h) const
{
    if (! breakpoints) return -1;
    auto b = getLocalBounds().toFloat();
    for (int i = 0; i < (int) breakpoints->size() - 1; ++i)
    {
        auto& bp0 = (*breakpoints)[(size_t) i];
        auto& bp1 = (*breakpoints)[(size_t) i + 1];
        float midPhase = (bp0.x + bp1.x) * 0.5f;
        float midVal = LfoEngine::customWaveformAt (*breakpoints, midPhase);
        float cx = b.getX() + midPhase * w;
        float cy = b.getY() + (1.0f - midVal) * h;
        if (std::abs (mx - cx) < 8.0f && std::abs (my - cy) < 8.0f)
            return i;
    }
    return -1;
}
