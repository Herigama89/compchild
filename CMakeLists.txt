cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)   # universal binary
endif()

project(Compchild VERSION 1.0.0)

set(CC_COMPANY     "Herigama Audio")
set(CC_PRODUCT     "Compchild")
set(CC_BUNDLE_ID   "com.herigama.compchild")
set(CC_MANUF_CODE  "Hrgm")
set(CC_PLUGIN_CODE "Cmch")

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

set(CC_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    list(REMOVE_ITEM CC_FORMATS AU)
endif()

juce_add_plugin(Compchild
    COMPANY_NAME                "${CC_COMPANY}"
    PRODUCT_NAME                "${CC_PRODUCT}"
    BUNDLE_ID                   "${CC_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE    ${CC_MANUF_CODE}
    PLUGIN_CODE                 ${CC_PLUGIN_CODE}
    FORMATS                     ${CC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_add_binary_data(CompchildAssets SOURCES
    Assets/background.png
    Assets/knob_big.png
    Assets/knob_pointer.png
    Assets/power.png
    Assets/vu_dome.png)
set_target_properties(CompchildAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(Compchild PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(Compchild PRIVATE cxx_std_17)
target_compile_definitions(Compchild PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_XINPUT=0
    JUCE_ALSA=0
    JUCE_JACK=0)

target_link_libraries(Compchild
    PRIVATE
        CompchildAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(UNIX AND NOT APPLE)
    target_compile_definitions(Compchild PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()

# DSP checks (no JUCE needed)
add_executable(dsp_test tests/dsp_test.cpp)
target_compile_features(dsp_test PRIVATE cxx_std_17)

# Renders the editor to a PNG without a DAW (interface checks)
option(CC_TOOLS "Build the snapshot tool" OFF)
if(CC_TOOLS)
    juce_add_console_app(cc_snapshot PRODUCT_NAME "cc_snapshot")
    target_sources(cc_snapshot PRIVATE tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_features(cc_snapshot PRIVATE cxx_std_17)
    target_compile_definitions(cc_snapshot PRIVATE JucePlugin_Name="Compchild"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(cc_snapshot PRIVATE CompchildAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
