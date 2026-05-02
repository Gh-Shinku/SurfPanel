cmake_minimum_required(VERSION 3.21.0)
project(SurfPanel VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_TESTS "Build and run tests" ON)

find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

add_subdirectory(third-party/toml11)

add_executable(SurfPanel WIN32)

target_sources(SurfPanel
  PRIVATE
  src/action_manager.cpp
  src/main.cpp
  src/mainwindow.cpp
  src/resources.qrc
  src/config.cpp
  src/search_engine.cpp
)

target_link_libraries(SurfPanel PRIVATE Qt6::Widgets toml11::toml11)

find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
if(WINDEPLOYQT_EXECUTABLE)
  set(WINDEPLOYQT_ARGS
    --no-opengl-sw
    --no-system-d3d-compiler
    --no-compiler-runtime
  )
  add_custom_command(TARGET SurfPanel POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${WINDEPLOYQT_ARGS} "$<TARGET_FILE:SurfPanel>"
    COMMENT "Running windeployqt to copy Qt plugins..."
  )
endif()

if(BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()
