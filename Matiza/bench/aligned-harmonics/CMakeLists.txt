# Benchmark do estudo de harmônicos alinhados à escala
# (ver .kanban/docs/Estudo — Harmônicos alinhados à escala.md).
# Projeto separado do plugin. Rodar dentro desta pasta:
#   cmake -B build
#   cmake --build build --config Release
#   build/Release/AlignedHarmonicsBench.exe
# Os áudios de comparação saem em build/audio/.
cmake_minimum_required(VERSION 3.22)
project(AlignedHarmonicsBench CXX)
set(CMAKE_CXX_STANDARD 20)

set(MATIZA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
add_executable(AlignedHarmonicsBench AlignedHarmonicsBench.cpp "${MATIZA_SRC}/Wavetables.cpp" "${MATIZA_SRC}/Oscillator.cpp")
target_include_directories(AlignedHarmonicsBench PRIVATE "${MATIZA_SRC}")
target_compile_definitions(AlignedHarmonicsBench PRIVATE "AUDIO_DIR=\"${CMAKE_BINARY_DIR}/audio\"")
