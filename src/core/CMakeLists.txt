# nc_core: browser engine on CEF; static so tests and the harness can link it.
add_library(nc_core STATIC
  keymap.cpp view_math.cpp domains.cpp config.cpp
  main_thread_queue.cpp frame_buffer.cpp navigation_gate.cpp
  browser.cpp engine.cpp cef_loader.cpp vgui_panel.cpp
)
target_include_directories(nc_core PUBLIC ${CMAKE_SOURCE_DIR}/src ${CEF_INCLUDE_PATH})
target_link_libraries(nc_core PUBLIC nc_common libcef_dll_wrapper ${CEF_LIB_RELEASE} delayimp)
SET_LIBRARY_TARGET_PROPERTIES(nc_core)
# libcef.dll is delay-loaded: the game already has an old libcef.dll loaded, so the new one is
# loaded by full path from nextchromium\cef\ (see cef_loader.cpp).
target_link_options(nc_core INTERFACE /DELAYLOAD:libcef.dll)

add_library(nextchromium SHARED exports.cpp hooks.cpp)
target_link_libraries(nextchromium PRIVATE nc_core)
SET_LIBRARY_TARGET_PROPERTIES(nextchromium)
