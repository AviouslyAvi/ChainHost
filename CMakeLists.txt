cmake_minimum_required(VERSION 3.22)
project(ChainHost VERSION 0.3.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ── JUCE ──────────────────────────────────────────────────────
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Gin (modules only — skip top-level to avoid double-adding JUCE) ──
FetchContent_Declare(gin
    GIT_REPOSITORY https://github.com/FigBug/Gin.git
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(gin)
if(NOT gin_POPULATED)
    FetchContent_Populate(gin)
endif()
# Selectively add only the modules we need
juce_add_module(${gin_SOURCE_DIR}/modules/gin)
juce_add_module(${gin_SOURCE_DIR}/modules/gin_simd)
juce_add_module(${gin_SOURCE_DIR}/modules/gin_dsp)
juce_add_module(${gin_SOURCE_DIR}/modules/gin_graphics)
juce_add_module(${gin_SOURCE_DIR}/modules/gin_gui)
juce_add_module(${gin_SOURCE_DIR}/modules/gin_plugin)

# ── Melatonin Blur (add as JUCE module directly) ──
FetchContent_Declare(melatonin_blur
    GIT_REPOSITORY https://github.com/sudara/melatonin_blur.git
    GIT_TAG        v1.4
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(melatonin_blur)
if(NOT melatonin_blur_POPULATED)
    FetchContent_Populate(melatonin_blur)
endif()
# juce_add_module expects dir name == module name, so create a properly named symlink
set(MELATONIN_MODULES_ROOT "${CMAKE_BINARY_DIR}/_modules")
set(MELATONIN_MODULE_DIR "${MELATONIN_MODULES_ROOT}/melatonin_blur")
file(MAKE_DIRECTORY "${MELATONIN_MODULES_ROOT}")
if(NOT EXISTS "${MELATONIN_MODULE_DIR}/melatonin_blur.h")
    if(WIN32)
        file(COPY "${melatonin_blur_SOURCE_DIR}/" DESTINATION "${MELATONIN_MODULE_DIR}")
    else()
        file(CREATE_LINK "${melatonin_blur_SOURCE_DIR}" "${MELATONIN_MODULE_DIR}" SYMBOLIC)
    endif()
endif()
juce_add_module(${MELATONIN_MODULE_DIR})

# ── Melatonin Inspector (DEBUG UI tool) ───────────────────────
FetchContent_Declare(melatonin_inspector
    GIT_REPOSITORY https://github.com/sudara/melatonin_inspector.git
    GIT_TAG        origin/main
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(melatonin_inspector)
if(NOT melatonin_inspector_POPULATED)
    FetchContent_Populate(melatonin_inspector)
endif()
set(INSPECTOR_MODULE_DIR "${MELATONIN_MODULES_ROOT}/melatonin_inspector")
if(NOT EXISTS "${INSPECTOR_MODULE_DIR}/melatonin_inspector.h")
    if(WIN32)
        file(COPY "${melatonin_inspector_SOURCE_DIR}/" DESTINATION "${INSPECTOR_MODULE_DIR}")
    else()
        file(CREATE_LINK "${melatonin_inspector_SOURCE_DIR}" "${INSPECTOR_MODULE_DIR}" SYMBOLIC)
    endif()
endif()
juce_add_module(${INSPECTOR_MODULE_DIR})

# ── Shared source list ────────────────────────────────────────
set(CHAINHOST_SOURCES
    src/PluginProcessor.cpp
    src/PluginScanner.cpp
    src/ChainGraph.cpp
    src/MacroManager.cpp
    src/LfoEngine.cpp
    src/ui/ChainHostLookAndFeel.cpp
    src/ui/ArcKnob.cpp
    src/ui/PluginWindow.cpp
    src/ui/PluginSlotComponent.cpp
    src/ui/PresetBrowser.cpp
    src/ui/MappingPanel.cpp
    src/ui/LfoWaveformEditor.cpp
    src/ui/LfoPanel.cpp
    src/ui/ScanProgressPanel.cpp
    src/ui/ChainHostEditor.cpp
    src/ui/PluginWindowMac.mm
)

set(CHAINHOST_COMPILE_DEFS
    GIN_HAS_SIMD=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=1
)

set(CHAINHOST_LINK_PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    gin
    gin_simd
    gin_dsp
    gin_gui
    gin_graphics
    gin_plugin
    melatonin_blur
    melatonin_inspector
)

set(CHAINHOST_LINK_PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ── Plugin target (Instrument) ────────────────────────────────
juce_add_plugin(ChainHost
    COMPANY_NAME              "Avious"
    PLUGIN_MANUFACTURER_CODE  Avii
    PLUGIN_CODE               ChHo
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "ChainHost"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
)

target_sources(ChainHost PRIVATE ${CHAINHOST_SOURCES})
target_compile_definitions(ChainHost PUBLIC ${CHAINHOST_COMPILE_DEFS})
target_link_libraries(ChainHost PRIVATE ${CHAINHOST_LINK_PRIVATE} PUBLIC ${CHAINHOST_LINK_PUBLIC})

# ── Plugin target (Effect) ────────────────────────────────────
juce_add_plugin(ChainHostFX
    COMPANY_NAME              "Avious"
    PLUGIN_MANUFACTURER_CODE  Avii
    PLUGIN_CODE               ChFx
    FORMATS                   VST3
    PRODUCT_NAME              "ChainHost FX"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
)

target_sources(ChainHostFX PRIVATE ${CHAINHOST_SOURCES})
target_compile_definitions(ChainHostFX PUBLIC ${CHAINHOST_COMPILE_DEFS})
target_link_libraries(ChainHostFX PRIVATE ${CHAINHOST_LINK_PRIVATE} PUBLIC ${CHAINHOST_LINK_PUBLIC})
