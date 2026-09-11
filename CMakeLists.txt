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

add_executable(SurfPanel WIN32)

target_sources(SurfPanel
  PRIVATE
  src/action_manager.cpp
  src/app_paths.cpp
  src/atomic_file.cpp
  src/clipboard_filter.cpp
  src/variable_resolver.cpp
  src/main.cpp
  src/mainwindow.cpp
  src/resources.qrc
  src/config.cpp
  src/fluent_panel.cpp
  src/recent_items_store.cpp
  src/search_result_view.cpp
  src/search_engine.cpp
  src/app_logging.cpp
)

target_link_libraries(SurfPanel PRIVATE Qt6::Widgets toml11::toml11)
if(MINGW)
  target_link_options(SurfPanel PRIVATE $<$<CONFIG:Release>:-s>)
endif()

find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
if(WINDEPLOYQT_EXECUTABLE)
  set(WINDEPLOYQT_ARGS
    --no-opengl-sw
    --no-system-d3d-compiler
    --no-system-dxc-compiler
    --no-compiler-runtime
    --no-translations
    --skip-plugin-types generic,networkinformation,tls
    --exclude-plugins qgif,qjpeg
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
