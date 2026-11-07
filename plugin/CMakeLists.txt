cmake_minimum_required(VERSION 3.22)

project(RedRoomDelay VERSION 0.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RRD_BUILD_PLUGIN "Build the JUCE plug-in (VST3/AU/Standalone)" ON)
option(RRD_BUILD_CLAP "Also build a CLAP target via clap-juce-extensions" ON)
option(RRD_COPY_AFTER_BUILD "Install the plug-ins into your user plug-in folders after each build" ON)
option(RRD_BUILD_UI_SNAPSHOT "Build rrd_ui_snapshot, which renders the editor to a PNG (development tool)" OFF)
set(RRD_JUCE_PATH "" CACHE PATH "Local JUCE checkout; downloaded when empty")

# ---------------------------------------------------------------- DSP core (no JUCE)
add_library(rrd_dsp STATIC src/dsp/Line.cpp src/dsp/Engine.cpp)
target_include_directories(rrd_dsp PUBLIC src/dsp)
if(MSVC)
    target_compile_options(rrd_dsp PRIVATE /W4)
else()
    target_compile_options(rrd_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(rrd_tests tests/EngineTests.cpp)
target_link_libraries(rrd_tests PRIVATE rrd_dsp)

add_executable(rrd_cli tools/rrd_cli.cpp)
target_link_libraries(rrd_cli PRIVATE rrd_dsp)

enable_testing()
add_test(NAME engine COMMAND rrd_tests)

# ---------------------------------------------------------------- plug-in
if(RRD_BUILD_PLUGIN)
    include(FetchContent)
    if(RRD_JUCE_PATH)
        add_subdirectory(${RRD_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(RRD_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND RRD_FORMATS AU)
    endif()

    juce_add_plugin(RedRoomDelay
        PRODUCT_NAME "Red Room Delay"
        COMPANY_NAME "Firewalk Audio"
        BUNDLE_ID "com.firewalkaudio.redroomdelay"
        PLUGIN_MANUFACTURER_CODE Fwau
        PLUGIN_CODE Rr16
        FORMATS ${RRD_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${RRD_COPY_AFTER_BUILD}
        VST3_CATEGORIES Fx Delay)

    set(RRD_PLUGIN_SOURCES
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/ui/Controls.cpp
        src/plugin/ui/Faceplate.cpp
        src/plugin/ui/Meters.cpp)
    target_sources(RedRoomDelay PRIVATE ${RRD_PLUGIN_SOURCES})
    target_compile_definitions(RedRoomDelay PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(RedRoomDelay
        PRIVATE rrd_dsp juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    # Logic caches Audio Unit scans; restarting the registrar makes it see a rebuilt AU without a reboot.
    if(APPLE AND RRD_COPY_AFTER_BUILD)
        add_custom_command(TARGET RedRoomDelay_AU POST_BUILD
            COMMAND sh -c "killall -9 AudioComponentRegistrar 2>/dev/null || true"
            VERBATIM)
    endif()

    if(RRD_BUILD_UI_SNAPSHOT)
        juce_add_console_app(rrd_ui_snapshot PRODUCT_NAME "rrd_ui_snapshot")
        target_sources(rrd_ui_snapshot PRIVATE tools/ui_snapshot.cpp ${RRD_PLUGIN_SOURCES})
        target_compile_definitions(rrd_ui_snapshot PRIVATE
            JucePlugin_Name="Red Room Delay"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(rrd_ui_snapshot PRIVATE rrd_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
    endif()

    if(RRD_BUILD_CLAP)
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG main
            GIT_SHALLOW ON)
        FetchContent_MakeAvailable(clap-juce-extensions)
        clap_juce_extensions_plugin(TARGET RedRoomDelay
            CLAP_ID "com.firewalkaudio.redroomdelay"
            CLAP_FEATURES audio-effect delay stereo)
    endif()
endif()
