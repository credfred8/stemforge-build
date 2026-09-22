cmake_minimum_required(VERSION 3.24)
project(MasterForgeOne VERSION 2.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.2
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MasterForgeOne
  COMPANY_NAME "Golden Light Audio"
  BUNDLE_ID "com.goldenlightaudio.masterforgeone"
  PLUGIN_MANUFACTURER_CODE GlAu
  PLUGIN_CODE Mf01
  FORMATS VST3 Standalone
  PRODUCT_NAME "MasterForge ONE"
  COPY_PLUGIN_AFTER_BUILD FALSE
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  NEEDS_WEBVIEW2 TRUE
)

juce_generate_juce_header(MasterForgeOne)

juce_add_binary_data(MasterForgeWebAssets
  HEADER_NAME MasterForgeWebAssets.h
  NAMESPACE MasterForgeWeb
  SOURCES
    WebUI/index.html
    WebUI/style.css
    WebUI/app.js
)

target_sources(MasterForgeOne PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/MasterEngine.h
)

target_compile_definitions(MasterForgeOne
  PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MasterForgeOne
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    MasterForgeWebAssets
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(MSVC)
  target_compile_options(MasterForgeOne PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

juce_add_console_app(MasterForgeSmoke
  PRODUCT_NAME "MasterForgeSmoke"
)

juce_generate_juce_header(MasterForgeSmoke)

target_sources(MasterForgeSmoke PRIVATE
  tests/engine_smoke.cpp
  Source/MasterEngine.h
)

target_compile_definitions(MasterForgeSmoke PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(MasterForgeSmoke
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(MSVC)
  target_compile_options(MasterForgeSmoke PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
