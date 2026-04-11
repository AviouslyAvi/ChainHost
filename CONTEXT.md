# ChainHost — Architecture & Context for AI Code Review

> **Read this before modifying any code.** This document describes the design
> decisions, thread-safety contracts, and ownership model that hold the plugin
> together. Breaking any of these invariants will cause crashes, data races, or
> subtle audio glitches.

---

## What Is ChainHost?

A **JUCE 8.0.4 VST3/Standalone plugin host** that lets you load third-party
VST3 plugins into parallel signal chains. Features:

- Multiple parallel chains with per-chain volume
- Per-slot bypass + dry/wet blend (via DryCaptureProcessor / DryWetProcessor pair)
- Drag-and-drop plugin reordering
- 8 macro knobs with parameter mapping (min/max range per mapping)
- MIDI-learn for macros
- Preset save/load (binary state wrapped in XML)
- 4 LFOs (Serum 2-style): breakpoint waveforms, tempo sync, retrigger,
  unipolar/bipolar, envelope (one-shot) mode, rise time, smoothing
- FabFilter-inspired UI via Gin's CopperLookAndFeel + Melatonin Blur shadows

---

## Build System (CMakeLists.txt)

- **C++20** required.
- Dependencies fetched via `FetchContent`: JUCE 8.0.4, Gin (FigBug),
  Melatonin Blur, Melatonin Inspector.
- **Gin is added module-by-module** (not via top-level `add_subdirectory`)
  because that would double-add JUCE. Do not change this — it will break the
  build.
- Melatonin modules are **symlinked** under `${CMAKE_BINARY_DIR}/_modules/`
  because JUCE requires directory name = module name. Do not restructure this.
- `JUCE_PLUGINHOST_VST3=1` enables hosting third-party VST3s.
- Formats: VST3 + Standalone only. Plugin copies itself after build.

---

## Thread Safety Model — CRITICAL

There are **two threads** that matter:

### Audio Thread (real-time, called by the DAW)
- Runs `ChainHostProcessor::processBlock()`
- Delegates to `juce::AudioProcessorGraph::processBlock()`
- **ONLY** writes to two atomics via `LfoEngine::prepareBlock()`:
  - `std::atomic<double> currentBpm` — read from `AudioPlayHead`
  - `std::atomic<bool> noteOnFlag` — set on MIDI note-on detection
- **Must not**: allocate memory, lock mutexes, call UI code, mutate vectors,
  or touch any non-atomic shared state.

### Message Thread (UI + timer callbacks)
- All graph mutations (add/remove plugins, rewire connections)
- All macro manager mutations (add/remove mappings)
- All LFO state mutations (shape, rate, targets, breakpoints)
- All UI rendering and event handling
- `LfoEngine::timerCallback()` → `process()` runs here at ~60 Hz

### The Contract
- `LfoEngine::prepareBlock()` is the **only** function safe to call from the
  audio thread. It touches only atomics.
- Everything else runs on the message thread. There are **no mutexes** in this
  codebase — thread safety relies on this separation.
- `parameterValueChanged()` may fire from the audio thread in some hosts. It
  calls `MacroManager::setMacroValue()` which is not lock-protected. This is a
  known trade-off. Do not add locking without understanding the real-time
  constraints.

### DO NOT:
- Add `std::mutex` or blocking calls to any code path reachable from `processBlock()`
- Move graph/macro/LFO mutations off the message thread without adding proper synchronization
- Call `repaint()`, `setBounds()`, or any UI method from the audio thread

---

## Ownership Hierarchy

```
ChainHostProcessor (owns everything by value — destruction order matters!)
  ├── juce::AudioProcessorGraph graph       ← owns all Node instances
  ├── ChainGraph chainGraph                 ← owns structural metadata (NodeIDs, slot state)
  ├── MacroManager macroManager             ← owns mapping tables (vector per macro)
  ├── PluginScanner scanner                 ← owns KnownPluginList + FormatManager
  └── LfoEngine lfoEngine                   ← owns LFO state, timer-based processing
       └── raw ptrs pendingMacros*, pendingGraph* ← set at start(), valid until stop()

ChainHostEditor (raw reference to processor — JUCE owns the editor's lifetime)
  ├── OwnedArray<PluginWindow>              ← floating plugin editor windows
  ├── OwnedArray<ChainViewRow>              ← chain row UI components
  ├── MappingPanel                          ← macro mapping UI
  ├── LfoPanel                              ← LFO control UI
  │   └── LfoWaveformEditor                 ← raw ptr to LfoEngine::Lfo::breakpoints vector
  └── PresetBrowser                         ← preset list + save/load
```

**Destructor ordering matters:** `ChainHostProcessor::~ChainHostProcessor()`
calls `lfoEngine.stop()` first, which stops the timer and nullifies the raw
pointers before `macroManager` and `graph` are destroyed. Do not reorder the
member declarations in `PluginProcessor.h`.

---

## File Responsibilities

| File | What It Does | Safe to Modify? |
|------|-------------|----------------|
| `PluginProcessor.h/cpp` | Central coordinator. Owns all subsystems. processBlock, state serialization. | With care — audio thread entry point |
| `ChainGraph.h/cpp` | Graph routing: parallel chains, per-slot dry/wet, bypass, plugin lifecycle | With care — graph mutations affect audio |
| `MacroManager.h/cpp` | 8 macros × N mappings. Linear scaling from normalised to min/max range | Relatively safe — message thread only |
| `PluginScanner.h/cpp` | VST3 scanning with deadman crash protection, cached plugin list | Safe — isolated subsystem |
| `LfoEngine.h/cpp` | 4 LFOs: waveform generation, tempo sync, breakpoints, target application | With care — audio/message thread boundary |
| `src/ui/Colors.h` | Shared color palette (header-only). All UI files include this. | Safe |
| `src/ui/ChainHostLookAndFeel.h/cpp` | Extends Gin's CopperLookAndFeel with Colors palette | Safe |
| `src/ui/FabKnob.h/cpp` | Arc knob wrapping juce::Slider with value overlay | Safe |
| `src/ui/PluginWindow.h/cpp` | Floating DocumentWindow for hosted plugin editors | Safe |
| `src/ui/PluginSlotComponent.h/cpp` | Plugin slot card with bypass, dry/wet, drag-and-drop | Safe |
| `src/ui/PresetBrowser.h/cpp` | ListBoxModel for .chainhost preset files | Safe |
| `src/ui/MappingPanel.h/cpp` | Macro parameter mapping UI with MIDI learn | Safe |
| `src/ui/LfoWaveformEditor.h/cpp` | Interactive breakpoint editor + preset shape rendering + phase cursor | Safe |
| `src/ui/LfoPanel.h/cpp` | Serum-style LFO control panel for 4 LFOs | Safe |
| `src/ui/ChainHostEditor.h/cpp` | Main editor (940×740). Top-level orchestrator, includes all ui/ files | Safe — message thread only |

---

## Per-Slot Audio Signal Path

Each plugin slot in the graph has **three nodes**, not one:

```
AudioInput → GainProcessor → [DryCaptureProcessor → Plugin → DryWetProcessor] → AudioOutput
             (per-chain vol)   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                        one slot (repeated per plugin)
```

- `DryCaptureProcessor`: copies incoming audio to a `SharedDryBuffer` (shared_ptr)
- `DryWetProcessor`: blends wet (post-plugin) with captured dry signal
- Both use `std::atomic<float>` for their gain/wet parameters (audio-thread safe)

**When removing a plugin**, all three nodes must be removed. `ChainGraph::removePlugin()`
handles this, but if you add new node types, follow the same pattern.

---

## LFO Engine Architecture

### Processing (message thread timer, ~60 Hz)
1. Read atomics: `currentBpm`, `noteOnFlag`
2. Handle retrigger: if `noteOnFlag` and `retrigger` enabled → reset phase to 0
3. Advance phase: `phase += rate * deltaTime` (Hz mode) or `phase += syncDivToHz(div, bpm) * deltaTime`
4. Envelope mode: clamp phase at 1.0 instead of wrapping
5. Generate raw waveform value (0–1): standard shapes or custom breakpoints
6. Apply unipolar/bipolar + depth scaling
7. Apply rise envelope (linear fade-in multiplied against modulation)
8. Apply one-pole lowpass smoothing
9. Push to targets (macro values or direct parameter values)

### Custom Waveform (Breakpoints)
- `std::vector<LfoBreakpoint>` — each has `{x, y, curve}` in 0–1 range
- `curve` parameter: -1 to +1 controls interpolation between points
  - `curve = 0`: linear
  - `curve > 0`: power > 1 (slow start, fast end — exponential feel)
  - `curve < 0`: power < 1 (fast start, slow end — logarithmic feel)
- The power function: `pow(t, 2^(curve * 3))` where t is normalised position within segment
- Minimum 2 breakpoints enforced by the waveform editor

### Tempo Sync Divisions
21 values in `SyncDiv` enum, from `Sync_8Bar` to `Sync_1_64`, including dotted (×1.5)
and triplet (×2/3) variants. `syncDivToBeats()` returns length in quarter-note beats.

---

## Macro System

- 8 macros, each exposed as a `juce::AudioParameterFloat` (automatable by the DAW)
- Each macro has N mappings: `{nodeId, paramIndex, minValue, maxValue}`
- `setMacroValue(index, normalised, graph)` scales linearly: `min + normalised * (max - min)`
- MIDI Learn: snapshots all parameter values, polls for the one that moved most (>5% delta)
- Duplicate mappings silently rejected

---

## UI Architecture

### Color Palette (Colors namespace in src/ui/Colors.h)
All colors are centralised. Do not hardcode hex colors elsewhere — use the
`Colors::` constants. The palette is warm amber on deep charcoal.

### Component Hierarchy
- `ChainHostEditor` (940×740, non-resizable)
  - Header (48px): logo, SCAN, +PLUGIN, +CHAIN, PRESETS
  - Optional PresetBrowser (140px, collapsible)
  - Chain rows (88px each, scrollable area)
  - Bottom section (230px):
    - Left: Macro strip (120px wide, 2 columns × 4 rows of knobs)
    - Right: Tabbed panel (MAPPINGS / LFO)

### Knob Rendering
`FabKnob` wraps `juce::Slider` with Gin's `CopperLookAndFeel` for the arc
rendering. The label and percentage overlay are painted manually. Do not replace
the slider style — the arc rendering comes from Gin and matches the FabFilter aesthetic.

### Plugin Window Management
- `OwnedArray<PluginWindow>` for floating editor windows
- 600ms reopen cooldown via `windowCloseTimestamps` map prevents double-open on rapid clicks
- `clearContentComponent()` is called before the close callback to avoid use-after-free
- On preset load: ALL plugin windows are cleared before graph refresh (old NodeIDs are invalid)

### LFO Waveform Editor
- `LfoWaveformEditor` holds a raw `std::vector<LfoBreakpoint>*` pointer
- When in custom mode: shows breakpoint handles, supports click/drag/right-click-delete
- When in preset mode: renders the preset shape (sine, triangle, etc.) as a static waveform
- Phase cursor animated via `updatePhase()` called from the editor's 50ms timer

---

## Serialization Format

State is XML with four children:
```xml
<ChainHostState>
  <ChainGraph>
    <Chain volume="1.0" gainNode="123">
      <Slot nodeId="456" dryCapture="789" dryWet="012" bypassed="0" dryWetVal="1.0">
        <PluginState format="VST3" id="..." name="...">BASE64_STATE</PluginState>
      </Slot>
    </Chain>
  </ChainGraph>
  <MacroMappings>
    <Mapping macro="0" node="456" param="3" min="0.0" max="1.0"/>
  </MacroMappings>
  <LfoEngine>
    <Lfo index="0" enabled="1" shape="0" rate="1.0" depth="0.5" ...>
      <Target type="0" macroIndex="0"/>
      <Breakpoint x="0.0" y="0.5" curve="0.0"/>
    </Lfo>
  </LfoEngine>
  <MacroValues m0="0.5" m1="0.0" ... m7="0.0"/>
</ChainHostState>
```

`suspendProcessing(true/false)` brackets the load in `setStateInformation`.
Do not remove this — loading rebuilds the entire graph.

---

## Known Design Trade-offs

1. **No mutexes anywhere.** Thread safety relies on the audio thread only
   touching atomics. This is intentional — mutexes in audio code cause priority
   inversion and glitches.

2. **`rebuildConnections()` is not incremental.** It tears down ALL graph
   connections and rewires from scratch on every structural change. Simple but
   not efficient for large chain counts.

3. **`parameterValueChanged` may fire on the audio thread** in some hosts. It
   calls `MacroManager::setMacroValue` which iterates a `std::vector`. This is
   technically a data race if the message thread is mutating the vector
   simultaneously. In practice, hosts usually call this on the message thread.

4. **`LfoEngine` stores raw pointers** (`pendingMacros*`, `pendingGraph*`) set
   at `start()`. These are valid because `lfoEngine.stop()` is called in the
   processor destructor before those objects die. Do not change member
   declaration order in `PluginProcessor.h`.

5. **`LfoWaveformEditor` holds a raw pointer** to a breakpoint vector inside
   `LfoEngine::Lfo`. Safe because both are on the message thread and have
   matching lifetimes, but would break if either is moved off the message thread.

---

## Review Checklist

When reviewing changes to this codebase, verify:

- [ ] No allocations, locks, or UI calls on any path reachable from `processBlock()`
- [ ] Graph structural changes only happen on the message thread
- [ ] New nodes follow the DryCaptureProcessor → Plugin → DryWetProcessor pattern
- [ ] Removed plugins also clean up macro mappings and LFO targets
- [ ] Preset load clears plugin windows before refreshing the graph
- [ ] New LFO/macro state is included in `toXml()`/`fromXml()` serialization
- [ ] Color constants use `Colors::` namespace, not hardcoded hex values
- [ ] Member declaration order in `PluginProcessor.h` is not changed (destruction order)
- [ ] Any new atomics use `std::memory_order_relaxed` (matching existing pattern)
- [ ] Custom waveform minimum 2 breakpoints invariant is maintained
