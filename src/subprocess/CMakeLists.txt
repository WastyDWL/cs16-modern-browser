# Chromium helper process (renderer/GPU/network). Lives next to the new libcef.dll.
add_executable(ncb_subprocess WIN32 main.cpp ncb_subprocess.manifest)
target_link_libraries(ncb_subprocess PRIVATE nc_common libcef_dll_wrapper ${CEF_LIB_RELEASE} delayimp)
target_include_directories(ncb_subprocess PRIVATE ${CEF_INCLUDE_PATH})
SET_EXECUTABLE_TARGET_PROPERTIES(ncb_subprocess)
