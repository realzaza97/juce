cmake_minimum_required(VERSION 3.22)
project(Vocal2Instrument VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(V2I_BUILD_PLUGIN "Build the JUCE VST3 / Standalone plugin (downloads JUCE)" ON)
option(V2I_BUILD_TESTS  "Build core tests and the CLI" ON)

# ---------------------------------------------------------------- core (pure C++17, no JUCE)
add_library(v2i_core STATIC
  core/src/WavReader.cpp
  core/src/PitchTracker.cpp
  core/src/NoteSegmenter.cpp
  core/src/Converter.cpp
  core/src/MidiFile.cpp)
target_include_directories(v2i_core PUBLIC core/include)
if(MSVC)
  target_compile_options(v2i_core PRIVATE /W4 /O2 /fp:fast)
else()
  target_compile_options(v2i_core PRIVATE -Wall -Wextra -O2)
endif()
set_target_properties(v2i_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(V2I_BUILD_TESTS)
  enable_testing()
  add_executable(v2i_cli cli/main.cpp)
  target_link_libraries(v2i_cli PRIVATE v2i_core)
  add_executable(v2i_tests tests/test_main.cpp)
  target_link_libraries(v2i_tests PRIVATE v2i_core)
  add_test(NAME core_synthetic_voice COMMAND v2i_tests)
endif()

# ---------------------------------------------------------------- plugin
if(V2I_BUILD_PLUGIN)
  add_subdirectory(plugin)
endif()
