cmake_minimum_required(VERSION 3.24)

project(StemForge VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/JUCE" CACHE PATH "Path to JUCE source")
set(ORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/onnxruntime" CACHE PATH "Path to ONNX Runtime Windows x64 package")
set(STEMFORGE_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/deps/models/htdemucs_6s_fp16weights.onnx" CACHE FILEPATH "HT-Demucs 6-stem ONNX model")

if(NOT EXISTS "${JUCE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_ROOT}. Run scripts/bootstrap.ps1 first.")
endif()
if(NOT EXISTS "${ORT_ROOT}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found at ${ORT_ROOT}. Run scripts/bootstrap.ps1 first.")
endif()
if(NOT EXISTS "${ORT_ROOT}/lib/onnxruntime.dll")
    message(FATAL_ERROR "ONNX Runtime DLL not found at ${ORT_ROOT}/lib/onnxruntime.dll")
endif()
if(NOT EXISTS "${STEMFORGE_MODEL}")
    message(FATAL_ERROR "Stem model not found at ${STEMFORGE_MODEL}. Run scripts/bootstrap.ps1 first.")
endif()

add_subdirectory("${JUCE_ROOT}" JUCE)

juce_add_plugin(StemForge
    COMPANY_NAME "StemForge Audio"
    BUNDLE_ID "audio.stemforge.vst3"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE SfAu
    PLUGIN_CODE StFg
    FORMATS VST3
    PRODUCT_NAME "StemForge"
    VST3_CATEGORIES Fx
)

juce_generate_juce_header(StemForge)

target_sources(StemForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StemEngine.cpp
    Source/StemEngine.h
    Source/StemTypes.h
)

target_include_directories(StemForge PRIVATE
    "${ORT_ROOT}/include"
)

target_compile_definitions(StemForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    ORT_API_MANUAL_INIT=1
)

target_link_libraries(StemForge PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)

if(MSVC)
    target_compile_options(StemForge PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

add_custom_command(TARGET StemForge_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:StemForge_VST3>/../Resources/runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:StemForge_VST3>/../Resources/runtime/onnxruntime.dll"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:StemForge_VST3>/../Resources/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${STEMFORGE_MODEL}"
        "$<TARGET_FILE_DIR:StemForge_VST3>/../Resources/models/htdemucs_6s_fp16weights.onnx"
    VERBATIM
)
