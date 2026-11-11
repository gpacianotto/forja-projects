# Benchmark do estudo de wavetable (ver .kanban/docs/Estudo — Wavetable nos osciladores.md).
# Projeto separado do plugin. Rodar dentro desta pasta:
#   cmake -B build
#   cmake --build build --config Release
#   build/Release/WavetableBench.exe
cmake_minimum_required(VERSION 3.22)
project(WavetableBench CXX)
set(CMAKE_CXX_STANDARD 20)

set(MATIZA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
add_executable(WavetableBench WavetableBench.cpp "${MATIZA_SRC}/Oscillator.cpp" "${MATIZA_SRC}/Wavetables.cpp")
target_include_directories(WavetableBench PRIVATE "${MATIZA_SRC}")
