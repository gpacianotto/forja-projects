cmake_minimum_required(VERSION 3.22)

project(Matiza VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE compartilhado do workspace (uma cópia para todos os projetos)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../_libs/JUCE" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(Matiza
    COMPANY_NAME "Forja Instruments"
    PLUGIN_MANUFACTURER_CODE Forj
    PLUGIN_CODE Mtza
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/Program Files/Common Files/VST3/Dev"
    BUNDLE_ID com.forjainstruments.matiza
    PRODUCT_NAME "Matiza")

target_sources(Matiza PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Graphs.cpp
    src/ModulationUI.cpp
    src/EnvelopeTabs.cpp
    src/ShapeEditor.cpp
    src/Controls.cpp
    src/MicroVoice.cpp
    src/Oscillator.cpp
    src/Wavetables.cpp
    src/ShapePoints.cpp
    src/CustomShapes.cpp
    src/TuningEngine.cpp)

target_include_directories(Matiza PRIVATE src)

target_compile_definitions(Matiza PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Matiza
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Testes unitários (lógica pura, sem JUCE) com Catch2
# Rodar: ctest --test-dir build -C Debug
# ---------------------------------------------------------------------------
option(MATIZA_BUILD_TESTS "Compilar os testes unitários" ON)

if(MATIZA_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(MatizaTests
        tests/TuningEngineTests.cpp
        tests/OscillatorTests.cpp
        tests/VoiceLogicTests.cpp
        tests/GraphMathTests.cpp
        tests/ModMatrixTests.cpp
        tests/LfoTests.cpp
        tests/UnisonTests.cpp
        tests/WavetableTests.cpp
        tests/ShapeTests.cpp
        src/TuningEngine.cpp
        src/Oscillator.cpp
        src/Wavetables.cpp
        src/ShapePoints.cpp
        src/CustomShapes.cpp)
    target_include_directories(MatizaTests PRIVATE src)
    target_link_libraries(MatizaTests PRIVATE Catch2::Catch2WithMain)

    enable_testing()
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(MatizaTests)
endif()
