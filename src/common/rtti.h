#pragma once

#include <windows.h>

#include <cstdint>

namespace nc {

// Finds the vtable of class `type_name` (MSVC decorated, e.g. ".?AVHTMLSerializer@@") for the
// base subobject at `offset` inside module `m`, via the RTTI Complete Object Locator.
void** find_vtable(HMODULE m, const char* type_name, uint32_t offset);
// True if p points into a loaded module image (used to sanity-check object pointers).
bool in_module_image(const void* p);
// True if [p, p+size) is committed readable memory.
bool readable(const void* p, size_t size);

}  // namespace nc
