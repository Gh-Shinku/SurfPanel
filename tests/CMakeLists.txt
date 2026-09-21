function(add_surfpanel_test target source)
  add_executable(${target} ${source})
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(${target} PRIVATE ${ARGN})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_surfpanel_test(test_config
  core/config/test_config.cpp
  SurfPanel::Core)
add_surfpanel_test(test_config_watcher
  core/config/test_config_watcher.cpp
  SurfPanel::Core)
add_surfpanel_test(test_clipboard_filter
  plugins/clipboard_filter/test_clipboard_filter.cpp
  Qt6::Gui
  SurfPanel::ClipboardPlugin)
add_surfpanel_test(test_plugin_manager
  plugins/host/test_plugin_manager.cpp
  SurfPanel::PluginRuntime)
add_surfpanel_test(test_search_engine
  core/search/test_search_engine.cpp
  SurfPanel::Core)
add_surfpanel_test(test_action_manager
  core/action/test_action_manager.cpp
  SurfPanel::Core)
add_surfpanel_test(test_variable_resolver
  core/action/test_variable_resolver.cpp
  SurfPanel::Core)
add_surfpanel_test(test_recent_items_store
  core/storage/test_recent_items_store.cpp
  SurfPanel::Core)
add_surfpanel_test(test_mainwindow
  ui/window/test_mainwindow.cpp
  SurfPanel::Ui)

if(WIN32)
  get_filename_component(QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
  get_filename_component(COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(TEST_RUNTIME_PATH "${QT_PREFIX}/bin;${COMPILER_BIN};$ENV{PATH}")
  string(REPLACE ";" "\\;" TEST_RUNTIME_PATH "${TEST_RUNTIME_PATH}")
  set_tests_properties(
    test_config
    test_config_watcher
    test_clipboard_filter
    test_plugin_manager
    test_search_engine
    test_action_manager
    test_variable_resolver
    test_recent_items_store
    test_mainwindow
    PROPERTIES ENVIRONMENT "PATH=${TEST_RUNTIME_PATH}"
  )
endif()
