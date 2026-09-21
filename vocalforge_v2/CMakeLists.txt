cmake_minimum_required(VERSION 3.22)
project(VocalForgeOne VERSION 2.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.3
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocalForgeOne
  COMPANY_NAME "Golden Light Audio"
  BUNDLE_ID "com.goldenlightaudio.vocalforgeone.v22"
  PLUGIN_MANUFACTURER_CODE GlAu
  PLUGIN_CODE Vf22
  FORMATS VST3
  PRODUCT_NAME "VocalForge ONE 2.2"
  COPY_PLUGIN_AFTER_BUILD FALSE
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

juce_generate_juce_header(VocalForgeOne)

target_sources(VocalForgeOne PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/VocalEngine.h
)

target_compile_definitions(VocalForgeOne
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VocalForgeOne
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_add_gui_app(VocalForgeOne_Standalone
  PRODUCT_NAME "VocalForge ONE"
  COMPANY_NAME "Golden Light Audio"
  BUNDLE_ID "com.goldenlightaudio.vocalforgeone.standalone"
)

juce_generate_juce_header(VocalForgeOne_Standalone)

target_sources(VocalForgeOne_Standalone PRIVATE
  Source/StandaloneMain.cpp
  Source/VocalEngine.h
)

target_compile_definitions(VocalForgeOne_Standalone
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(VocalForgeOne_Standalone
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
