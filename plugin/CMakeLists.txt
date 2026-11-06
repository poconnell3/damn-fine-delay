cmake_minimum_required(VERSION 3.22)

project(DamnFineDelay VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(DFD_BUILD_PLUGIN "Build the JUCE plug-in (VST3/AU/Standalone)" ON)
option(DFD_BUILD_CLAP "Also build a CLAP target via clap-juce-extensions" ON)
option(DFD_COPY_AFTER_BUILD "Install the plug-ins into your user plug-in folders after each build" ON)
option(DFD_BUILD_UI_SNAPSHOT "Build dfd_ui_snapshot, which renders the editor to a PNG (development tool)" OFF)
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
        COPY_PLUGIN_AFTER_BUILD ${DFD_COPY_AFTER_BUILD}
        VST3_CATEGORIES Fx Delay)

    set(DFD_PLUGIN_SOURCES
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/ui/Controls.cpp
        src/plugin/ui/Faceplate.cpp
        src/plugin/ui/Meters.cpp)
    target_sources(DamnFineDelay PRIVATE ${DFD_PLUGIN_SOURCES})
    target_compile_definitions(DamnFineDelay PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(DamnFineDelay
        PRIVATE dfd_dsp juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    # Logic caches Audio Unit scans; restarting the registrar makes it see a rebuilt AU without a reboot.
    if(APPLE AND DFD_COPY_AFTER_BUILD)
        add_custom_command(TARGET DamnFineDelay_AU POST_BUILD
            COMMAND sh -c "killall -9 AudioComponentRegistrar 2>/dev/null || true"
            VERBATIM)
    endif()

    if(DFD_BUILD_UI_SNAPSHOT)
        juce_add_console_app(dfd_ui_snapshot PRODUCT_NAME "dfd_ui_snapshot")
        target_sources(dfd_ui_snapshot PRIVATE tools/ui_snapshot.cpp ${DFD_PLUGIN_SOURCES})
        target_compile_definitions(dfd_ui_snapshot PRIVATE
            JucePlugin_Name="Damn Fine Delay"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(dfd_ui_snapshot PRIVATE dfd_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
    endif()

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
