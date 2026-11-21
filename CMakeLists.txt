cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)   # universal binary
endif()

project(OPT2A VERSION 1.0.0)

# JUCE: -DJUCE_DIR=/path/to/JUCE, ./JUCE, or fetched automatically
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.6 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(OPT_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    list(REMOVE_ITEM OPT_FORMATS AU)
endif()

juce_add_plugin(OPT2A
    COMPANY_NAME                "Herigama Audio"
    PRODUCT_NAME                "OPT-2A"
    BUNDLE_ID                   "com.herigama.opt2a"
    PLUGIN_MANUFACTURER_CODE    Hrgm
    PLUGIN_CODE                 Opt2
    FORMATS                     ${OPT_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_add_binary_data(OPT2AAssets SOURCES
    Assets/background.jpg
    Assets/knob_silver.png
    Assets/screw_knob.png
    Assets/selector.png
    Assets/toggle_up.png
    Assets/toggle_down.png
    Assets/lamp_on.png
    Assets/lamp_off.png
    Assets/vu_cut.png
    Assets/vu_glass.png
    Assets/scope_glass.png
    docs/manual.html)
set_target_properties(OPT2AAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(OPT2A PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_features(OPT2A PRIVATE cxx_std_17)
target_compile_definitions(OPT2A PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0)
target_link_libraries(OPT2A
    PRIVATE OPT2AAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(UNIX AND NOT APPLE)
    target_compile_definitions(OPT2A PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()

# DSP checks against the measured reference (no JUCE needed)
add_executable(dsp_test tests/dsp_test.cpp)
target_compile_features(dsp_test PRIVATE cxx_std_17)

# renders the editor to a PNG without a DAW (interface checks)
option(OPT_TOOLS "Build the snapshot tool" OFF)
if(OPT_TOOLS)
    juce_add_console_app(opt_snapshot PRODUCT_NAME "opt_snapshot")
    target_sources(opt_snapshot PRIVATE tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_features(opt_snapshot PRIVATE cxx_std_17)
    target_compile_definitions(opt_snapshot PRIVATE JucePlugin_Name="OPT-2A"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(opt_snapshot PRIVATE OPT2AAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
