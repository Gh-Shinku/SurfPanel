add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE Qt6::Core toml11::toml11)

add_test(NAME test_config COMMAND test_config)