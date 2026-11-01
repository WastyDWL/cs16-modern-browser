add_executable(nc_tests main.cpp test_input.cpp test_settings.cpp test_engine_logic.cpp)
target_link_libraries(nc_tests PRIVATE nc_core)
SET_EXECUTABLE_TARGET_PROPERTIES(nc_tests)
