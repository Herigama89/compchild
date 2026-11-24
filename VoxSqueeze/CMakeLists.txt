# VoxSqueeze: one-fader vocal leveler (added from the root CMakeLists, which provides JUCE)
set(VX_VERSION 1.0.0)

juce_add_plugin(VoxSqueeze
    VERSION                     ${VX_VERSION}
    COMPANY_NAME                "${CC_COMPANY}"
    PRODUCT_NAME                "VoxSqueeze"
    BUNDLE_ID                   "com.herigama.voxsqueeze"
    PLUGIN_MANUFACTURER_CODE    ${CC_MANUF_CODE}
    PLUGIN_CODE                 Vxsq
    FORMATS                     ${CC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_add_binary_data(VoxSqueezeAssets SOURCES
    Assets/panel.png
    Assets/fader_cap.png
    docs/manual.html)
set_target_properties(VoxSqueezeAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(VoxSqueeze PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(VoxSqueeze PRIVATE cxx_std_17)
target_compile_definitions(VoxSqueeze PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_XINPUT=0
    JUCE_ALSA=0
    JUCE_JACK=0)

target_link_libraries(VoxSqueeze
    PRIVATE
        VoxSqueezeAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(UNIX AND NOT APPLE)
    target_compile_definitions(VoxSqueeze PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()

# DSP checks (no JUCE needed)
add_executable(vx_dsp_test tests/dsp_test.cpp)
target_compile_features(vx_dsp_test PRIVATE cxx_std_17)

# Renders the editor to a PNG without a DAW (interface checks, manual pictures)
if(CC_TOOLS)
    juce_add_console_app(vx_snapshot PRODUCT_NAME "vx_snapshot")
    target_sources(vx_snapshot PRIVATE tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_features(vx_snapshot PRIVATE cxx_std_17)
    target_compile_definitions(vx_snapshot PRIVATE JucePlugin_Name="VoxSqueeze"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(vx_snapshot PRIVATE VoxSqueezeAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
