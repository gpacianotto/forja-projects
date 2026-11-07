cmake_minimum_required(VERSION 3.22)

project(MicroSynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE compartilhado do workspace (uma cópia para todos os projetos)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../_libs/JUCE" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(MicroSynth
    COMPANY_NAME "Guilherme"
    PLUGIN_MANUFACTURER_CODE Gmsy
    PLUGIN_CODE Mcsy
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/Program Files/Common Files/VST3/Dev"
    PRODUCT_NAME "MicroSynth")

target_sources(MicroSynth PRIVATE
    src/PluginProcessor.cpp)

target_include_directories(MicroSynth PRIVATE src)

target_compile_definitions(MicroSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MicroSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
