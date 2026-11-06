cmake_minimum_required(VERSION 3.22)

project(DamnFineDelay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(DFD_BUILD_PLUGIN "Build the JUCE plug-in (VST3/AU/Standalone)" ON)
option(DFD_BUILD_CLAP "Also build a CLAP target via clap-juce-extensions" ON)
set(DFD_JUCE_PATH "" CACHE PATH "Local JUCE checkout; downloaded when empty")

# ---------------------------------------------------------------- DSP core (no JUCE)
add_library(dfd_dsp STATIC src/dsp/Line.cpp src/dsp/Engine.cpp)
target_include_directories(dfd_dsp PUBLIC src/dsp)
if(MSVC)
    target_compile_options(dfd_dsp PRIVATE /W4)
else()
    target_compile_options(dfd_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(dfd_tests tests/EngineTests.cpp)
target_link_libraries(dfd_tests PRIVATE dfd_dsp)

add_executable(dfd_cli tools/dfd_cli.cpp)
target_link_libraries(dfd_cli PRIVATE dfd_dsp)

enable_testing()
add_test(NAME engine COMMAND dfd_tests)

# ---------------------------------------------------------------- plug-in
if(DFD_BUILD_PLUGIN)
    include(FetchContent)
    if(DFD_JUCE_PATH)
        add_subdirectory(${DFD_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(DFD_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND DFD_FORMATS AU)
    endif()

    juce_add_plugin(DamnFineDelay
        PRODUCT_NAME "Damn Fine Delay"
        COMPANY_NAME "Damn Fine Audio"
        BUNDLE_ID "com.damnfineaudio.damnfinedelay"
        PLUGIN_MANUFACTURER_CODE Dfau
        PLUGIN_CODE Df16
        FORMATS ${DFD_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_CATEGORIES Fx Delay)

    target_sources(DamnFineDelay PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
    target_compile_definitions(DamnFineDelay PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(DamnFineDelay
        PRIVATE dfd_dsp juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    if(DFD_BUILD_CLAP)
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG main
            GIT_SHALLOW ON)
        FetchContent_MakeAvailable(clap-juce-extensions)
        clap_juce_extensions_plugin(TARGET DamnFineDelay
            CLAP_ID "com.damnfineaudio.damnfinedelay"
            CLAP_FEATURES audio-effect delay stereo)
    endif()
endif()
