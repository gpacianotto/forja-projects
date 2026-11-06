cmake_minimum_required(VERSION 3.22)

project(@PRODUCT_NAME@ VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE compartilhado do workspace (uma cópia para todos os projetos)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../_libs/JUCE" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(@PRODUCT_NAME@
    COMPANY_NAME "Guilherme"
    PLUGIN_MANUFACTURER_CODE Gmsy
    PLUGIN_CODE @PLUGIN_CODE@
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/Program Files/Common Files/VST3/Dev"
    PRODUCT_NAME "@PRODUCT_NAME@")

target_sources(@PRODUCT_NAME@ PRIVATE
    src/PluginProcessor.cpp)

target_include_directories(@PRODUCT_NAME@ PRIVATE src)

target_compile_definitions(@PRODUCT_NAME@ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(@PRODUCT_NAME@
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
