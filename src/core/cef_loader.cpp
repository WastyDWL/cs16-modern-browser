// The game already has CEF1's libcef.dll loaded under the same name, so a normal import of
// libcef.dll would bind to it. nextchromium links libcef.dll as delay-loaded and resolves it
// here to the full path of the new copy.
#include <windows.h>

#include <delayimp.h>

#include <string>

#include "common/log.h"
#include "core/engine.h"

namespace {

HMODULE g_libcef = nullptr;

FARPROC WINAPI delay_hook(unsigned notify, PDelayLoadInfo info) {
    if (notify == dliNotePreLoadLibrary && _stricmp(info->szDll, "libcef.dll") == 0 && g_libcef)
        return reinterpret_cast<FARPROC>(g_libcef);
    return nullptr;
}

}  // namespace

extern "C" const PfnDliHook __pfnDliNotifyHook2 = delay_hook;

namespace nc {

bool load_cef_library(const std::wstring& cef_dir) {
    if (g_libcef) return true;
    std::wstring path = cef_dir + L"\\libcef.dll";
    g_libcef = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_libcef) {
        log("cannot load %ls: error %lu", path.c_str(), GetLastError());
        return false;
    }
    log("loaded %ls at %p (old libcef.dll at %p)", path.c_str(), g_libcef, GetModuleHandleW(L"libcef.dll"));
    return true;
}

}  // namespace nc
