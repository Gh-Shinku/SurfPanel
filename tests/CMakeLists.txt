add_executable(test_config
	test_config.cpp
	../src/config.cpp
)
target_include_directories(test_config PRIVATE ../src)
target_link_libraries(test_config PRIVATE Qt6::Core toml11::toml11)

add_executable(test_search_engine
	test_search_engine.cpp
	../src/search_engine.cpp
)
target_include_directories(test_search_engine PRIVATE ../src)
target_link_libraries(test_search_engine PRIVATE Qt6::Core)

add_executable(test_action_manager
	test_action_manager.cpp
	../src/action_manager.cpp
)
target_include_directories(test_action_manager PRIVATE ../src)
target_link_libraries(test_action_manager PRIVATE Qt6::Core Qt6::Gui)

add_executable(test_mainwindow
	test_mainwindow.cpp
	../src/mainwindow.cpp
	../src/search_engine.cpp
	../src/action_manager.cpp
	../src/config.cpp
)
target_include_directories(test_mainwindow PRIVATE ../src)
target_link_libraries(test_mainwindow PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets toml11::toml11)

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_search_engine COMMAND test_search_engine)
add_test(NAME test_action_manager COMMAND test_action_manager)
add_test(NAME test_mainwindow COMMAND test_mainwindow)