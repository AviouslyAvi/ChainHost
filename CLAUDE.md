# CLAUDE.md — Instructions for Claude Code

## Project Overview
ChainHost is a JUCE 8 C++ VST3 plugin host. See CONTEXT.md for full architecture.

## Build
```
cd build && cmake .. && cmake --build .
```

## File Layout
- `src/` — Core engine (processor, graph, macros, LFO, scanner). All message-thread except processBlock.
- `src/ui/` — All UI components. Each class is its own .h/.cpp pair. Message-thread only.
- `src/ui/Colors.h` is the shared color palette. All UI files include it.

## Critical Rules — DO NOT VIOLATE
1. NEVER add std::mutex, locks, or blocking calls to any code path reachable from processBlock()
2. NEVER reorder member declarations in PluginProcessor.h (destruction order matters)
3. NEVER hardcode hex color values in UI code — use Colors:: namespace constants
4. NEVER modify graph/macro/LFO state from the audio thread
5. ALWAYS follow the DryCaptureProcessor -> Plugin -> DryWetProcessor pattern for new slots
6. ALWAYS update CMakeLists.txt target_sources when adding/removing .cpp files
7. ALWAYS update serialization (toXml/fromXml) when adding persistent state

## Thread Safety
- Audio thread: ONLY touches atomics via LfoEngine::prepareBlock(). Nothing else.
- Message thread: ALL graph mutations, UI, macro changes, LFO processing.
- No mutexes exist in this codebase. This is intentional.

## When modifying UI components
- Each component is self-contained in src/ui/
- Read only the specific component's .h/.cpp + Colors.h
- ChainHostEditor.h/cpp is the top-level orchestrator — read it to understand layout
- FabKnob is used everywhere for knobs — do not replace with raw juce::Slider

## When modifying engine code
- Read CONTEXT.md sections on Thread Safety and Ownership Hierarchy first
- LfoEngine.cpp is the most complex file — read the LFO Engine Architecture section in CONTEXT.md
- parameterValueChanged may fire from audio thread in some hosts (known trade-off)

## Testing
No automated tests exist. Test manually in a DAW or Standalone mode.
Build must succeed with zero warnings on macOS (Apple Clang).
