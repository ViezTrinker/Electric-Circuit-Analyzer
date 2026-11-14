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

if(DEFINED Qt6_DIR AND NOT EXISTS "${Qt6_DIR}/Qt6Config.cmake")
   unset(Qt6_DIR CACHE)
endif()

if(WIN32 AND NOT Qt6_DIR)
   set(QT_SELECTED_VERSION "0")
   set(QT_SELECTED_KIT "")
   file(GLOB QT_VERSION_DIRECTORIES "C:/Qt/*")
   foreach(QT_VERSION_DIRECTORY IN LISTS QT_VERSION_DIRECTORIES)
      if(NOT EXISTS "${QT_VERSION_DIRECTORY}/msvc2022_64/lib/cmake/Qt6/Qt6Config.cmake")
         continue()
      endif()
      get_filename_component(QT_VERSION_NAME "${QT_VERSION_DIRECTORY}" NAME)
      if(QT_VERSION_NAME VERSION_GREATER QT_SELECTED_VERSION)
         set(QT_SELECTED_VERSION "${QT_VERSION_NAME}")
         set(QT_SELECTED_KIT "${QT_VERSION_DIRECTORY}/msvc2022_64")
      endif()
   endforeach()
   if(QT_SELECTED_KIT)
      list(APPEND CMAKE_PREFIX_PATH "${QT_SELECTED_KIT}")
   endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(eca_analyzer
   WIN32
   src/main.cpp
   src/ui/main_window.cpp
   src/ui/main_window.h
   src/ui/schematic_view.cpp
   src/ui/schematic_view.h
)

target_link_libraries(eca_analyzer PRIVATE eca_core Qt6::Widgets)

if(MSVC)
   target_compile_options(eca_analyzer PRIVATE /W4 /permissive- /utf-8)
else()
   target_compile_options(eca_analyzer PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT eca_analyzer)

if(WIN32)
   get_filename_component(QT_BIN_DIRECTORY "${Qt6_DIR}/../../../bin" ABSOLUTE)
   set_target_properties(eca_analyzer PROPERTIES
      VS_DEBUGGER_ENVIRONMENT "PATH=${QT_BIN_DIRECTORY};$ENV{PATH}"
   )
   set(VISUAL_STUDIO_INSTALL_DIRECTORY "")
   execute_process(
      COMMAND "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe"
         -latest
         -products *
         -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
         -property installationPath
      OUTPUT_VARIABLE VISUAL_STUDIO_INSTALL_DIRECTORY
      OUTPUT_STRIP_TRAILING_WHITESPACE
   )
   find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIRECTORY}")
   if(WINDEPLOYQT_EXECUTABLE)
      set(DEPLOY_COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:eca_analyzer>")
      if(VISUAL_STUDIO_INSTALL_DIRECTORY)
         add_custom_command(TARGET eca_analyzer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env "VCINSTALLDIR=${VISUAL_STUDIO_INSTALL_DIRECTORY}/VC/"
               ${DEPLOY_COMMAND}
            COMMENT "Deploying Qt runtime"
         )
      else()
         add_custom_command(TARGET eca_analyzer POST_BUILD
            COMMAND ${DEPLOY_COMMAND}
            COMMENT "Deploying Qt runtime"
         )
      endif()
   endif()
endif()
