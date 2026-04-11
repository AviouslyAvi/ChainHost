# ChainHost

A VST3 plugin that hosts other VST3 plugins inside your DAW.

---

## What It Does

ChainHost sits on a track in your DAW and lets you load third-party VST3 effects (or instruments) inside of it. Instead of stacking plugins one after another on your DAW's channel strip, you build and manage your signal chains visually inside ChainHost's own window.

The key idea: **parallel signal chains with macro control and modulation.**

### Parallel Chains

Most DAWs only give you a serial plugin chain — audio flows through plugin A, then B, then C. ChainHost lets you split the signal into multiple parallel paths that process simultaneously and sum together at the output.

For example, you could run a clean compression chain alongside a distorted chain alongside a reverb chain — all in parallel, each with independent volume control. This is the kind of routing that normally requires complex DAW bussing or dedicated tools like BlueCat PatchWork.

### Per-Slot Controls

Every plugin slot has:
- **Bypass** — disable the plugin without removing it
- **Dry/Wet** — blend between the original (dry) and processed (wet) signal, even if the plugin itself doesn't have a mix knob
- **Drag-and-drop** — reorder plugins within or across chains

### 8 Macro Knobs

Macros let you control multiple parameters across different plugins with a single knob. Map a macro to your compressor's threshold, your EQ's high shelf, and your reverb's decay — then automate just the one macro in your DAW.

Each mapping has a min/max range, so one macro turn can push one parameter up while pulling another down. Macros are exposed as automatable parameters to the host DAW.

MIDI Learn is supported: click LEARN, move a parameter in any loaded plugin, and it auto-maps.

### 4 LFOs (Serum 2-style)

Each LFO can modulate any macro or any parameter on any loaded plugin. Features:
- **7 waveform shapes**: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold, Sample & Glide
- **Custom waveform editor**: click to add breakpoints, drag to reshape, with curved segments between points
- **Tempo sync**: lock to your DAW's BPM with 21 note divisions (8 bars down to 1/64, including dotted and triplet)
- **Retrigger**: reset the LFO phase on every MIDI note-on
- **Unipolar / Bipolar**: modulate in one direction (0 to 1) or both (-1 to +1)
- **Envelope mode**: one-shot — the LFO plays once and holds, like an envelope
- **Rise time**: fade the modulation in gradually
- **Smoothing**: low-pass filter on the LFO output for gentler movement

### Preset System

Save and recall your entire setup — all chains, all plugins with their states, all macro mappings, all LFO configurations — as a single preset file.

---

## Who It's For

- **Sound designers** who want complex modulation routing without leaving their DAW
- **Mixing engineers** who want parallel processing chains in a single plugin slot
- **Live performers** who want macro knob control over multi-plugin setups
- **Anyone** who's thought "I wish I could run these plugins in parallel without six aux sends"

---

## Technical Details

- **Format**: VST3 + Standalone
- **Framework**: JUCE 8.0.4
- **UI**: FabFilter-inspired dark theme with arc knobs (via Gin CopperLookAndFeel)
- **Platform**: macOS (builds on Apple Silicon and Intel)
- **C++ Standard**: C++20

---

## Project Structure

```
src/
  PluginProcessor.h/cpp   — Audio engine coordinator, state serialization
  PluginEditor.h/cpp      — All UI components and layout
  ChainGraph.h/cpp        — Parallel chain routing, per-slot dry/wet
  MacroManager.h/cpp      — 8 macros with parameter mapping
  LfoEngine.h/cpp         — 4 LFOs with breakpoint waveforms and tempo sync
  PluginScanner.h/cpp     — VST3 plugin discovery and caching
CONTEXT.md                — Architecture guide for AI-assisted code review
reports/                  — Code review outputs
```

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

The VST3 is automatically copied to `~/Library/Audio/Plug-Ins/VST3/` after build. The standalone app is in `build/ChainHost_artefacts/Standalone/`.
