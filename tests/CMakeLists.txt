add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE SurfPanel::Core)

add_executable(test_clipboard_filter test_clipboard_filter.cpp)
target_link_libraries(test_clipboard_filter PRIVATE SurfPanel::ClipboardPlugin)

add_executable(test_plugin_manager test_plugin_manager.cpp)
target_link_libraries(test_plugin_manager PRIVATE SurfPanel::PluginRuntime)

add_executable(test_search_engine test_search_engine.cpp)
target_link_libraries(test_search_engine PRIVATE SurfPanel::Core)

add_executable(test_action_manager test_action_manager.cpp)
target_link_libraries(test_action_manager PRIVATE SurfPanel::Core)

add_executable(test_variable_resolver test_variable_resolver.cpp)
target_link_libraries(test_variable_resolver PRIVATE SurfPanel::Core)

add_executable(test_recent_items_store test_recent_items_store.cpp)
target_link_libraries(test_recent_items_store PRIVATE SurfPanel::Core)

add_executable(test_mainwindow test_mainwindow.cpp)
target_link_libraries(test_mainwindow PRIVATE SurfPanel::Ui)

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_clipboard_filter COMMAND test_clipboard_filter)
add_test(NAME test_plugin_manager COMMAND test_plugin_manager)
add_test(NAME test_search_engine COMMAND test_search_engine)
add_test(NAME test_action_manager COMMAND test_action_manager)
add_test(NAME test_variable_resolver COMMAND test_variable_resolver)
add_test(NAME test_recent_items_store COMMAND test_recent_items_store)
add_test(NAME test_mainwindow COMMAND test_mainwindow)

if(WIN32)
  get_filename_component(QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
  get_filename_component(COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(TEST_RUNTIME_PATH "${QT_PREFIX}/bin;${COMPILER_BIN};$ENV{PATH}")
  string(REPLACE ";" "\\;" TEST_RUNTIME_PATH "${TEST_RUNTIME_PATH}")
  set_tests_properties(
    test_config
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
