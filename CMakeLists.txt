cmake_minimum_required(VERSION 3.22)
project(RELINKAITimingAlign VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Provide -DJUCE_DIR=C:/path/to/JUCE, or place JUCE next to this project.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif (DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
else()
    message(FATAL_ERROR "JUCE not found. Supply -DJUCE_DIR=path/to/JUCE")
endif()

juce_add_plugin(RELINKAITimingAlign
    COMPANY_NAME "RELINK Audio"
    BUNDLE_ID "audio.relink.timingalign"
    PLUGIN_MANUFACTURER_CODE Rlnk
    PLUGIN_CODE RtA1
    FORMATS VST3
    PRODUCT_NAME "RELINK AI Timing Align"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)

juce_generate_juce_header(RELINKAITimingAlign)

target_sources(RELINKAITimingAlign PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h)
target_compile_definitions(RELINKAITimingAlign PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(RELINKAITimingAlign PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
