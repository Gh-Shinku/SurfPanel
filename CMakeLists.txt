cmake_minimum_required(VERSION 3.21.0)
project(SurfPanel VERSION 0.1.0 LANGUAGES C CXX)

if(WIN32)
  enable_language(RC)
endif()

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

set(SURFPANEL_QT_ROOT "$ENV{SURFPANEL_QT_ROOT}")
if(SURFPANEL_QT_ROOT STREQUAL "")
  message(FATAL_ERROR
    "SURFPANEL_QT_ROOT is required. Set it to the Qt6 installation prefix "
    "before configuring, for example: C:/Qt/6.8.0/mingw_64")
endif()

file(TO_CMAKE_PATH "${SURFPANEL_QT_ROOT}" SURFPANEL_QT_ROOT)
if(NOT EXISTS "${SURFPANEL_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
  message(FATAL_ERROR
    "SURFPANEL_QT_ROOT does not contain lib/cmake/Qt6/Qt6Config.cmake: "
    "${SURFPANEL_QT_ROOT}")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${SURFPANEL_QT_ROOT}")
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

add_subdirectory(third-party/toml11)
add_subdirectory(src)

add_custom_target(check-layout
  COMMAND "${CMAKE_COMMAND}"
    -DSURFPANEL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_project_layout.cmake"
  COMMENT "Checking SurfPanel source tree layout..."
)

if(BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()
