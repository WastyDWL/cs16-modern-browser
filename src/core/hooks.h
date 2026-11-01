#pragma once

#include <windows.h>

namespace nc {

// Verifies the chromehtml.dll build looks like the one the hooks were written against.
bool check_compatible(void* controller, void** controller_vtbl, HMODULE chromehtml);
// Points the controller singleton and the HTMLSerializer class at the Engine.
bool install_hooks(void* controller, void** controller_vtbl, HMODULE chromehtml);

}  // namespace nc
