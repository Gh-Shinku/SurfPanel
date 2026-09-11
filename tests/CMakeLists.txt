add_executable(test_config
	test_config.cpp
	../src/app_paths.cpp
	../src/atomic_file.cpp
	../src/config.cpp
)
target_include_directories(test_config PRIVATE ../src)
target_link_libraries(test_config PRIVATE Qt6::Core toml11::toml11)

add_executable(test_clipboard_filter
	test_clipboard_filter.cpp
	../src/plugins/clipboard_filter/clipboard_filter_plugin.cpp
)
target_include_directories(test_clipboard_filter PRIVATE ../src)
target_link_libraries(test_clipboard_filter PRIVATE Qt6::Core toml11::toml11)

add_executable(test_plugin_manager
	test_plugin_manager.cpp
	../src/plugin/plugin_manager.cpp
)
target_include_directories(test_plugin_manager PRIVATE ../src)
target_link_libraries(test_plugin_manager PRIVATE Qt6::Core)

add_executable(test_search_engine
	test_search_engine.cpp
	../src/search_engine.cpp
)
target_include_directories(test_search_engine PRIVATE ../src)
target_link_libraries(test_search_engine PRIVATE Qt6::Core)

add_executable(test_action_manager
	test_action_manager.cpp
	../src/action_manager.cpp
	../src/variable_resolver.cpp
)
target_include_directories(test_action_manager PRIVATE ../src)
target_link_libraries(test_action_manager PRIVATE Qt6::Core Qt6::Gui)

add_executable(test_variable_resolver
	test_variable_resolver.cpp
	../src/variable_resolver.cpp
)
target_include_directories(test_variable_resolver PRIVATE ../src)
target_link_libraries(test_variable_resolver PRIVATE Qt6::Core)

add_executable(test_recent_items_store
	test_recent_items_store.cpp
	../src/app_paths.cpp
	../src/atomic_file.cpp
	../src/recent_items_store.cpp
)
target_include_directories(test_recent_items_store PRIVATE ../src)
target_link_libraries(test_recent_items_store PRIVATE Qt6::Core toml11::toml11)

add_executable(test_mainwindow
	test_mainwindow.cpp
	../src/app_paths.cpp
	../src/atomic_file.cpp
	../src/plugin/builtin_plugins.cpp
	../src/plugin/plugin_manager.cpp
	../src/plugins/clipboard_filter/clipboard_filter_plugin.cpp
	../src/mainwindow.cpp
	../src/search_engine.cpp
	../src/action_manager.cpp
	../src/variable_resolver.cpp
	../src/config.cpp
	../src/fluent_panel.cpp
	../src/recent_items_store.cpp
	../src/search_result_view.cpp
)
target_include_directories(test_mainwindow PRIVATE ../src)
target_link_libraries(test_mainwindow PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets toml11::toml11)

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
