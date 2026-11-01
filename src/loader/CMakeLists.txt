# nextchromium.asi: loaded by Miles (Mss32.dll); installs the hooks and loads nextchromium.dll.
add_library(nextchromium_asi SHARED asi_main.cpp asi.def)
set_target_properties(nextchromium_asi PROPERTIES OUTPUT_NAME nextchromium SUFFIX ".asi")
target_link_libraries(nextchromium_asi PRIVATE nc_common)
