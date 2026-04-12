#include "ArcKnob.h"

ArcKnob::ArcKnob (const juce::String& lbl, juce::Colour arcCol)
    : label (lbl), arcColour (arcCol)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 1.0, 0.001);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, arcColour);
    slider.setColour (juce::Slider::trackColourId, Colors::border);
    slider.setPopupMenuEnabled (false);
    slider.onValueChange = [this]() { repaint(); if (onValueChange) onValueChange(); };
    addAndMakeVisible (slider);

    // Overlay sits on top of the slider to intercept halo ring clicks
    addAndMakeVisible (haloOverlay);
}

void ArcKnob::resized()
{
    auto b = getLocalBounds();
    int labelH = label.isEmpty() ? 0 : 14;
    int valueH = showPercentage ? 12 : 0;
    // Reserve 8px top and 6px each side for the halo ring
    auto sliderBounds = b.withTrimmedBottom (labelH + valueH).withTrimmedTop (8).reduced (6, 0);
    int sz = juce::jmin (sliderBounds.getWidth(), sliderBounds.getHeight());
    auto centred = sliderBounds.withSizeKeepingCentre (sz, sz);
    slider.setBounds (centred);
    haloOverlay.setBounds (b.withTrimmedBottom (labelH + valueH));
}

//==============================================================================
// Arc geometry — matches JUCE's default rotary slider angles
void ArcKnob::getArcGeometry (float& cx, float& cy, float& radius,
                               float& startAngle, float& arcRange) const
{
    auto rp = slider.getRotaryParameters();
    startAngle = rp.startAngleRadians;
    arcRange = rp.endAngleRadians - rp.startAngleRadians;

    auto sb = slider.getBounds().toFloat();
    cx = sb.getCentreX();
    cy = sb.getCentreY();
    radius = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f - 2.0f;
}

bool ArcKnob::hitTestHalo (float mx, float my) const
{
    if (std::abs (modDepth) < 0.001f) return false;

    float cx, cy, radius, startAngle, arcRange;
    getArcGeometry (cx, cy, radius, startAngle, arcRange);

    float haloR = radius + 3.0f;
    float dx = mx - cx, dy = my - cy;
    float dist = std::sqrt (dx * dx + dy * dy);

    // Hit if within 6px of the halo ring radius
    return dist > haloR - 6.0f && dist < haloR + 6.0f;
}

//==============================================================================
void ArcKnob::paint (juce::Graphics& g)
{
    // ── Blue Halo: short arc from current value showing mod range ──
    if (std::abs (modDepth) > 0.001f)
    {
        float cx, cy, radius, startAngle, arcRange;
        getArcGeometry (cx, cy, radius, startAngle, arcRange);

        float haloR = radius + 4.0f;

        float norm = (float) ((slider.getValue() - slider.getMinimum())
                              / (slider.getMaximum() - slider.getMinimum()));
        float valueAngle = startAngle + norm * arcRange;
        float modAngle = valueAngle + modDepth * arcRange;
        modAngle = juce::jlimit (startAngle, startAngle + arcRange, modAngle);

        float fromA = juce::jmin (valueAngle, modAngle);
        float toA   = juce::jmax (valueAngle, modAngle);

        if (toA - fromA > 0.01f)
        {
            juce::Path modArc;
            modArc.addCentredArc (cx, cy, haloR, haloR, 0.0f,
                                  fromA, toA, true);

            // Thin blue line
            g.setColour (Colors::lfoBlue.withAlpha (0.85f));
            g.strokePath (modArc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }
    }

    if (showPercentage)
    {
        float norm = (float) slider.getValue();
        if (slider.getMaximum() > 1.1)
        {
            g.setColour (Colors::text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            auto valStr = juce::String (slider.getValue(), 2) + suffix;
            g.drawText (valStr, 0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
        else
        {
            g.setColour (Colors::text.withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (juce::String (juce::roundToInt (norm * 100.0f)) + suffix,
                        0, slider.getBottom(), getWidth(), 12, juce::Justification::centred);
        }
    }

    if (label.isNotEmpty())
    {
        g.setColour (Colors::textDim);
        g.setFont (juce::Font (juce::FontOptions (8.5f)));
        g.drawText (label, 0, getHeight() - 12, getWidth(), 12, juce::Justification::centred);
    }

    if (macroDragHover)
    {
        g.setColour (Colors::accent.withAlpha (0.2f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (Colors::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1), 4.0f, 1.5f);
    }
}

//==============================================================================
void ArcKnob::setValue (float v, bool notify)
{
    slider.setValue (v, notify ? juce::sendNotificationAsync : juce::dontSendNotification);
}

float ArcKnob::getValue() const { return (float) slider.getValue(); }

void ArcKnob::setRange (float mn, float mx)
{
    slider.setRange (mn, mx, (mx - mn) < 2.0f ? 0.001 : 0.01);
}

void ArcKnob::setArcColour (juce::Colour c)
{
    arcColour = c;
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    repaint();
}

//==============================================================================
// Right-click context menu
void ArcKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onBuildContextMenu)
    {
        juce::PopupMenu menu;
        onBuildContextMenu (menu);
        if (menu.getNumItems() > 0)
        {
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                { e.getScreenX(), e.getScreenY(), 1, 1 }),
                [this] (int result) {
                    if (result > 0 && onContextMenuResult)
                        onContextMenuResult (result);
                });
        }
    }
}

//==============================================================================
// HaloOverlay — catches mouse events on the halo ring

bool ArcKnob::HaloOverlay::hitTest (int x, int y)
{
    // Convert overlay-local coords to ArcKnob-local coords
    float fx = (float) x + (float) getX();
    float fy = (float) y + (float) getY();
    return owner.hitTestHalo (fx, fy);
}

void ArcKnob::HaloOverlay::mouseDown (const juce::MouseEvent& e)
{
    owner.draggingModDepth = true;
    owner.modDragStartDepth = owner.modDepth;
    owner.modDragStartY = (float) e.y;
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ArcKnob::HaloOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (owner.draggingModDepth)
    {
        float delta = (owner.modDragStartY - (float) e.y) / 150.0f;
        float newDepth = juce::jlimit (0.0f, 1.0f, owner.modDragStartDepth + delta);
        owner.modDepth = newDepth;
        if (owner.onModDepthChanged)
            owner.onModDepthChanged (newDepth);
        owner.repaint();
    }
}

void ArcKnob::HaloOverlay::mouseUp (const juce::MouseEvent&)
{
    owner.draggingModDepth = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ArcKnob::HaloOverlay::mouseMove (const juce::MouseEvent&)
{
    if (owner.modDepth > 0.001f)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

//==============================================================================
// Macro drag-drop

bool ArcKnob::isInterestedInDragSource (const SourceDetails& details)
{
    auto desc = details.description.toString();
    return desc.startsWith ("macro:") || desc.startsWith ("lfo:");
}

void ArcKnob::itemDropped (const SourceDetails& details)
{
    macroDragHover = false; repaint();
    auto desc = details.description.toString();
    if (desc.startsWith ("macro:") && onMacroDropped)
    {
        int macroIdx = desc.fromFirstOccurrenceOf ("macro:", false, false).getIntValue();
        onMacroDropped (macroIdx);
    }
    else if (desc.startsWith ("lfo:") && onLfoDropped)
    {
        int lfoIdx = desc.fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
        onLfoDropped (lfoIdx);
    }
}
