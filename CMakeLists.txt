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

add_library(surfpanel_core STATIC
  src/action_manager.cpp
  src/action_manager.h
  src/app_logging.cpp
  src/app_logging.h
  src/app_paths.cpp
  src/app_paths.h
  src/atomic_file.cpp
  src/atomic_file.h
  src/config.cpp
  src/config.h
  src/config_watcher.cpp
  src/config_watcher.h
  src/item.h
  src/recent_items_store.cpp
  src/recent_items_store.h
  src/search_engine.cpp
  src/search_engine.h
  src/variable_resolver.cpp
  src/variable_resolver.h
)
target_include_directories(surfpanel_core PUBLIC src)
target_link_libraries(surfpanel_core PUBLIC Qt6::Gui toml11::toml11)
add_library(SurfPanel::Core ALIAS surfpanel_core)

add_library(surfpanel_plugin_runtime STATIC
  src/plugin/plugin.h
  src/plugin/plugin_manager.cpp
  src/plugin/plugin_manager.h
)
target_include_directories(surfpanel_plugin_runtime PUBLIC src)
target_link_libraries(surfpanel_plugin_runtime PUBLIC Qt6::Core)
add_library(SurfPanel::PluginRuntime ALIAS surfpanel_plugin_runtime)

add_library(surfpanel_clipboard_plugin STATIC
  src/plugins/clipboard_filter/clipboard_filter_plugin.cpp
  src/plugins/clipboard_filter/clipboard_filter_plugin.h
  src/plugins/clipboard_filter/text_transformer.cpp
  src/plugins/clipboard_filter/text_transformer.h
)
target_include_directories(surfpanel_clipboard_plugin PUBLIC src)
target_link_libraries(surfpanel_clipboard_plugin
  PRIVATE
  toml11::toml11
  PUBLIC
  SurfPanel::PluginRuntime
)
add_library(SurfPanel::ClipboardPlugin ALIAS surfpanel_clipboard_plugin)

add_library(surfpanel_ui STATIC
  src/window_effects.cpp
  src/window_effects.h
  src/palette_search_input.cpp
  src/palette_search_input.h
  src/palette_geometry.h
  src/fluent_panel.cpp
  src/fluent_panel.h
  src/mainwindow.cpp
  src/mainwindow.h
  src/plugin/builtin_plugins.cpp
  src/plugin/builtin_plugins.h
  src/search_result_view.cpp
  src/search_result_view.h
)
target_include_directories(surfpanel_ui PUBLIC src)
target_link_libraries(surfpanel_ui
  PUBLIC
  Qt6::Widgets
  SurfPanel::Core
  SurfPanel::PluginRuntime
  SurfPanel::ClipboardPlugin
)
add_library(SurfPanel::Ui ALIAS surfpanel_ui)

add_executable(SurfPanel WIN32
  src/main.cpp
  src/resources.qrc
)
target_link_libraries(SurfPanel PRIVATE SurfPanel::Ui)
if(WIN32)
  target_sources(SurfPanel PRIVATE src/app_icon.rc)
  target_include_directories(SurfPanel PRIVATE assets)
  set_property(SOURCE src/app_icon.rc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/SurfPanel.ico")
endif()
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
