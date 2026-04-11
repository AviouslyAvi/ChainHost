# AGENTS.md — Instructions for Codex

Read CONTEXT.md for full architecture. Key rules:

- Do not add mutexes or blocking calls on audio-thread paths
- Do not reorder members in PluginProcessor.h
- Use Colors:: namespace for all color values
- Update CMakeLists.txt target_sources when adding .cpp files
- Update toXml/fromXml when adding persistent state
- UI code is in src/ui/, one class per file pair
- Build: `cd build && cmake .. && cmake --build .`
