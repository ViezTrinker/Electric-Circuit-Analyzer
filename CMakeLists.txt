cmake_minimum_required(VERSION 3.24)

project(electric_circuit_analyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(eca_core STATIC
   src/core/analysis_settings.cpp
   src/core/analysis_settings.h
   src/core/cancel_token.cpp
   src/core/cancel_token.h
   src/core/circuit.cpp
   src/core/circuit.h
   src/core/component.h
   src/core/component_access.cpp
   src/core/component_catalog.cpp
   src/core/grid_point.cpp
   src/core/grid_point.h
   src/core/netlist.h
   src/core/netlist_builder.cpp
   src/core/netlist_builder.h
   src/core/result.cpp
   src/core/result.h
   src/core/si_parser.cpp
   src/core/si_parser.h
   src/digital/logic_kernel.cpp
   src/digital/logic_kernel.h
   src/solver/analog_solver.cpp
   src/solver/analog_solver.h
   src/solver/analysis_result.h
   src/solver/dense_system.h
   src/solver/device_model.cpp
   src/solver/device_model.h
   src/solver/simulator.cpp
   src/solver/simulator.h
)

target_include_directories(eca_core PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/src
   ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(MSVC)
   target_compile_options(eca_core PRIVATE /W4 /permissive- /utf-8)
else()
   target_compile_options(eca_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(eca_tests
   tests/test_main.cpp
   tests/test_analysis.cpp
   tests/test_digital.cpp
)

target_link_libraries(eca_tests PRIVATE eca_core)

if(MSVC)
   target_compile_options(eca_tests PRIVATE /W4 /permissive- /utf-8)
else()
   target_compile_options(eca_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME analysis COMMAND eca_tests)
