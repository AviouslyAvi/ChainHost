#pragma once
// Colors — V2 Daylight palette. Off-white surfaces, near-black text,
// muted slate-blue accent, teal modulation. Pro-Q / Ableton-light feel.
#include <juce_gui_basics/juce_gui_basics.h>

namespace Colors
{
    // Surfaces — warm-neutral light tones
    const juce::Colour bgDeep       (0xffe4e4e8);
    const juce::Colour bg           (0xfff2f2f4);
    const juce::Colour surface      (0xfffafafc);
    const juce::Colour surfaceRaised(0xffffffff);
    const juce::Colour surfaceHover (0xfff6f6fa);

    // Edges
    const juce::Colour border       (0xffcfcfd4);
    const juce::Colour borderSubtle (0xffe6e6ea);
    const juce::Colour slotBg       (0xffffffff);
    const juce::Colour slotBorder   (0xffcfcfd4);

    // Text — near-black on light
    const juce::Colour text         (0xff1c1d20);
    const juce::Colour textMid      (0xff5a5b60);
    const juce::Colour textDim      (0xff96979d);

    // Accents — muted slate-blue (primary) + teal (modulation)
    const juce::Colour accent       (0xff3272a8);
    const juce::Colour accentBright (0xff4a8dc8);

    // Semantic
    const juce::Colour remove       (0xffb9534a);
    const juce::Colour bypass       (0xff5a9a68);
    const juce::Colour wet          (0xff2f9a8c);  // dry/wet uses teal
    const juce::Colour chainAccent  (0xff7a8ba0);
    const juce::Colour learn        (0xffc98236);

    // Modulation halo — teal
    const juce::Colour lfoBlue      (0xff2f9a8c);

    // 8-color accent ramp for macro knobs / chain rows. Cool-to-warm gradient
    // so each macro stays visually distinct against the light surface.
    const juce::Colour macroAccents[8] = {
        juce::Colour (0xff3272a8), juce::Colour (0xff4a8dc8),
        juce::Colour (0xff2f9a8c), juce::Colour (0xff5ca37a),
        juce::Colour (0xff8f9f6a), juce::Colour (0xffc98236),
        juce::Colour (0xffb9724a), juce::Colour (0xff9e5a82)
    };
}
