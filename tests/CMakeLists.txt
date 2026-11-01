add_executable(nc_tests main.cpp test_input.cpp test_settings.cpp test_engine_logic.cpp test_vgui_panel.cpp test_js_api.cpp test_api_router.cpp)
target_link_libraries(nc_tests PRIVATE nc_core)
SET_EXECUTABLE_TARGET_PROPERTIES(nc_tests)
# The fake vgui2 classes need RTTI like the game's client.dll (CEF's flags turn it off).
set_source_files_properties(test_vgui_panel.cpp PROPERTIES COMPILE_OPTIONS "/GR")
