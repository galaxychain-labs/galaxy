add_executable( test_state_history session_test.cpp plugin_config_test.cpp)
target_link_libraries(test_state_history state_history_plugin Boost::unit_test_framework)
target_include_directories( test_state_history PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include" )

add_test(test_state_history test_state_history)