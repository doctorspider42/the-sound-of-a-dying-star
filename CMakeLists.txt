cmake_minimum_required(VERSION 3.22)

project(DyingStar
    VERSION 0.3.0
    DESCRIPTION "The Sound of a Dying Star - cosmic shimmer reverb"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Static MSVC runtime: without it the .vst3 refuses to load on any machine that has
# not installed the matching Visual C++ redistributable.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(DYINGSTAR_COPY_AFTER_BUILD "Install the built plug-in into the user VST3 folder" ON)
option(DYINGSTAR_BUILD_TOOLS      "Build the developer tool (offline checks / renderer)" OFF)

# Off by default, and deliberately. Link-time optimisation buys a few per cent of CPU in
# the finished binary and costs minutes on every single link - the plug-in, the
# standalone and the tool each re-optimise the whole of JUCE, so changing one line of DSP
# used to be a five-minute wait. Release builds turn it on; nobody working on the code
# should have to.
option(DYINGSTAR_LTO "Link-time optimisation (slow links, for release builds)" OFF)

set(DYINGSTAR_BUILD_ID "" CACHE STRING "Build identifier shown in the UI (e.g. a commit SHA)")

# ---------------------------------------------------------------------------
# JUCE, pinned so any binary can be traced back to an exact framework revision.
# ---------------------------------------------------------------------------
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found in libs/JUCE.\n"
        "Run: git submodule update --init --recursive")
endif()

add_subdirectory(libs/JUCE)

# ---------------------------------------------------------------------------
# The plug-in
# ---------------------------------------------------------------------------
juce_add_plugin(DyingStar
    PRODUCT_NAME                "The Sound of a Dying Star"
    COMPANY_NAME                "doctorspider42"
    BUNDLE_ID                   "com.doctorspider42.dyingstar"
    PLUGIN_MANUFACTURER_CODE    Dyst
    PLUGIN_CODE                 Dstr
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx Reverb
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Both sizes are given, so each icon resolution the platforms ask for is taken from
    # the render closest to it rather than scaled down from one master. Both are drawn by
    # Source/gui/Icon.cpp and regenerated with `./build.sh --icon`; they are committed
    # because CMake needs them at configure time, which is before anything that could
    # draw them has been built.
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-small.png"

    COPY_PLUGIN_AFTER_BUILD     ${DYINGSTAR_COPY_AFTER_BUILD})

target_sources(DyingStar PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/dsp/NebulaEngine.cpp
    Source/dsp/VoidDelay.cpp
    Source/gui/Backdrop.cpp
    Source/gui/Controls.cpp
    Source/gui/CosmicKnob.cpp
    Source/gui/Icon.cpp
    Source/gui/Skin.cpp
    Source/gui/StarView.cpp)

target_include_directories(DyingStar PRIVATE Source)

if (DYINGSTAR_BUILD_ID)
    string(SUBSTRING "${DYINGSTAR_BUILD_ID}" 0 7 DYINGSTAR_BUILD_ID_SHORT)
    target_compile_definitions(DyingStar PRIVATE
        DYINGSTAR_BUILD_ID="${DYINGSTAR_BUILD_ID_SHORT}")
endif()

target_compile_definitions(DyingStar PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(DyingStar
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if (DYINGSTAR_LTO)
    target_link_libraries(DyingStar PUBLIC juce::juce_recommended_lto_flags)

    # GCC runs its link-time passes serially unless it is told how many it may run at
    # once, which is most of where the minutes go - "using serial compilation of 79
    # LTRANS jobs" in the build log is the machine saying so out loud.
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(DyingStar PUBLIC $<$<CONFIG:Release>:-flto=auto>)
        target_link_options   (DyingStar PUBLIC $<$<CONFIG:Release>:-flto=auto>)
    endif()
endif()

# -ffast-math is deliberately absent: it breaks denormal handling and NaN semantics,
# both of which this engine depends on.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(DyingStar PRIVATE
        $<$<CONFIG:Release>:-O3 -fno-math-errno>)
endif()

# ---------------------------------------------------------------------------
# Developer tool: offline DSP checks and a headless panel renderer. Neither mode
# needs an audio device or a display, so both run in CI.
# ---------------------------------------------------------------------------
if (DYINGSTAR_BUILD_TOOLS)
    juce_add_console_app(DyingStarDevTool PRODUCT_NAME "DyingStarDevTool")

    target_sources(DyingStarDevTool PRIVATE tools/DevTool.cpp)
    target_include_directories(DyingStarDevTool PRIVATE Source)

    # The modules have to be listed again: the plug-in target links them PRIVATE, so
    # their include directories do not propagate to consumers.
    target_link_libraries(DyingStarDevTool PRIVATE
        DyingStar
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    target_compile_definitions(DyingStarDevTool PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
endif()
