cmake_minimum_required(VERSION 3.21)
project(nextchromium LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "nextchromium must be built for 32-bit (use tools\\build.cmd)")
endif()

set(TP ${CMAKE_SOURCE_DIR}/third_party)

# CEF binary distribution (sets /MT and other compiler flags via its macros)
set(CEF_ROOT ${TP}/cef)
list(APPEND CMAKE_MODULE_PATH ${CEF_ROOT}/cmake)
find_package(CEF REQUIRED)
# The installed Windows SDK is 10.0.19041, which predates NTDDI_WIN10_FE that CEF asks for.
list(REMOVE_ITEM CEF_COMPILER_DEFINES "NTDDI_VERSION=NTDDI_WIN10_FE")
list(APPEND CEF_COMPILER_DEFINES "NTDDI_VERSION=0x0A000008")
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
add_compile_options(/utf-8)

# MinHook (static)
file(GLOB MINHOOK_SRC ${TP}/minhook/src/*.c ${TP}/minhook/src/hde/hde32.c)
add_library(minhook STATIC ${MINHOOK_SRC})
target_include_directories(minhook PUBLIC ${TP}/minhook/include)

add_library(nc_common STATIC src/common/log.cpp src/common/rtti.cpp)
target_include_directories(nc_common PUBLIC src)
target_compile_definitions(nc_common PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)

add_subdirectory(src/core)
add_subdirectory(src/subprocess)
add_subdirectory(src/loader)
add_subdirectory(tests)
add_subdirectory(tools/harness)

# Spike: logging proxy around the original chromehtml.dll (kept for diagnostics)
add_library(nextchromium_spike SHARED src/spike/spike_asi.cpp src/spike/logging_proxy.cpp src/spike/spike.def)
set_target_properties(nextchromium_spike PROPERTIES SUFFIX ".asi")
target_link_libraries(nextchromium_spike PRIVATE nc_common minhook shlwapi)
