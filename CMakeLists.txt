cmake_minimum_required(VERSION 3.21)
project(nextchromium LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "nextchromium must be built for Win32 (configure with -A Win32)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/utf-8 /W3)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

set(TP ${CMAKE_SOURCE_DIR}/third_party)

# MinHook (static)
file(GLOB MINHOOK_SRC ${TP}/minhook/src/*.c ${TP}/minhook/src/hde/hde32.c)
add_library(minhook STATIC ${MINHOOK_SRC})
target_include_directories(minhook PUBLIC ${TP}/minhook/include)

add_library(nc_common STATIC src/common/log.cpp)
target_include_directories(nc_common PUBLIC src)

# Spike: logging proxy around the original chromehtml.dll
add_library(nextchromium_spike SHARED src/spike/spike_asi.cpp src/spike/logging_proxy.cpp src/spike/spike.def)
set_target_properties(nextchromium_spike PROPERTIES SUFFIX ".asi")
target_link_libraries(nextchromium_spike PRIVATE nc_common minhook shlwapi)
