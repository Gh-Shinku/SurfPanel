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

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_search_engine COMMAND test_search_engine)