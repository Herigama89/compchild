# Black Tube — optical tube compressor (AU / VST3). Added to the build from the top-level CMakeLists.txt.
set(BT_VERSION 1.1.0)

juce_add_plugin(BlackTube
    VERSION                     ${BT_VERSION}
    COMPANY_NAME                "Herigama Audio"
    PRODUCT_NAME                "Black Tube"
    BUNDLE_ID                   "com.herigama.blacktube"
    PLUGIN_MANUFACTURER_CODE    Hrgm
    PLUGIN_CODE                 Bltb
    FORMATS                     ${CC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_add_binary_data(BlackTubeAssets
    HEADER_NAME BlackTubeData.h
    NAMESPACE   BlackTubeData
    SOURCES
        Assets/panel.jpg
        Assets/knob_ring_main.png
        Assets/knob_ring_small.png
        Assets/knob_body_main.png
        Assets/knob_body_small.png
        Assets/toggle_left.png
        Assets/toggle_center.png
        Assets/toggle_right.png)
set_target_properties(BlackTubeAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BlackTube PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_features(BlackTube PRIVATE cxx_std_17)
target_compile_definitions(BlackTube PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0)
if(UNIX AND NOT APPLE)
    target_compile_definitions(BlackTube PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()
target_link_libraries(BlackTube
    PRIVATE  BlackTubeAssets juce::juce_audio_utils
    PUBLIC   juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# DSP checks and offline renderer (no JUCE needed)
add_executable(bt_dsp_test tests/bt_dsp_test.cpp)
target_compile_features(bt_dsp_test PRIVATE cxx_std_17)
add_executable(bt_render tests/bt_render.cpp)
target_compile_features(bt_render PRIVATE cxx_std_17)

if(CC_TOOLS)
    juce_add_console_app(bt_snapshot PRODUCT_NAME "bt_snapshot")
    target_sources(bt_snapshot PRIVATE tools/bt_snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_features(bt_snapshot PRIVATE cxx_std_17)
    target_compile_definitions(bt_snapshot PRIVATE JucePlugin_Name="Black Tube"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(bt_snapshot PRIVATE BlackTubeAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
