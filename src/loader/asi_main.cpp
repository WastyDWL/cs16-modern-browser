// nextchromium.asi — loaded by Miles (Mss32.dll scans the game folder for *.asi).
// By then hw.dll already owns the CHTMLController singleton (docs/spike-results.md), so the
// loader only swaps that object's RunFrame slot for a bootstrap. On the first frame — main
// thread, outside the loader lock — it restores the original vtable, loads
// nextchromium\nextchromium.dll and lets it take over. Any failure leaves the game untouched.
#include <windows.h>

#include <cstring>
#include <string>

#include "common/log.h"
#include "common/valve_html.h"

namespace {

constexpr int kControllerSlots = 15;
constexpr int kRunFrameSlot = 3;

std::wstring g_mod_dir;
void* g_controller = nullptr;
void** g_orig_vtbl = nullptr;
void** g_boot_vtbl = nullptr;

using nc_activate_t = bool (*)(void* controller, void** controller_vtbl, const wchar_t* mod_dir);

void activate() {
    std::wstring dll = g_mod_dir + L"\\nextchromium.dll";
    HMODULE m = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) {
        nc::log("cannot load %ls: error %lu", dll.c_str(), GetLastError());
        return;
    }
    auto fn = reinterpret_cast<nc_activate_t>(GetProcAddress(m, "nc_activate"));
    if (!fn) {
        nc::log("nc_activate not exported by %ls", dll.c_str());
        return;
    }
    bool ok = fn(g_controller, g_orig_vtbl, g_mod_dir.c_str());
    nc::log(ok ? "nextchromium took over the game's browser" : "nextchromium not activated; original browser kept");
}

bool __fastcall boot_run_frame(void* self, void*) {
    *static_cast<void***>(self) = g_orig_vtbl;  // from here on, whatever nc_activate installs
    activate();
    return static_cast<IHTMLChromeController*>(self)->RunFrame();
}

bool slots_in(void** vt, HMODULE m) {
    for (int i = 0; i < kControllerSlots; ++i) {
        HMODULE owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCWSTR>(vt[i]), &owner) ||
            owner != m)
            return false;
    }
    return true;
}

void start(HMODULE self) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(self, path, MAX_PATH);
    *wcsrchr(path, L'\\') = 0;
    g_mod_dir = std::wstring(path) + L"\\nextchromium";
    if (GetFileAttributesW((g_mod_dir + L"\\nextchromium.dll").c_str()) == INVALID_FILE_ATTRIBUTES) return;
    nc::log_open((g_mod_dir + L"\\nextchromium.log").c_str(), true);
    nc::log("nextchromium.asi loaded");

    HMODULE chromehtml = GetModuleHandleW(L"chromehtml.dll");
    auto create = chromehtml ? reinterpret_cast<CreateInterfaceFn>(GetProcAddress(chromehtml, "CreateInterface"))
                             : nullptr;
    if (!create) {
        nc::log("chromehtml.dll is not loaded yet; nextchromium stays inactive");
        return;
    }
    g_controller = create(CHROMEHTML_CONTROLLER_INTERFACE_VERSION, nullptr);
    if (!g_controller) {
        nc::log("no %s; nextchromium stays inactive", CHROMEHTML_CONTROLLER_INTERFACE_VERSION);
        return;
    }
    g_orig_vtbl = *static_cast<void***>(g_controller);
    if (!slots_in(g_orig_vtbl, chromehtml)) {
        nc::log("unexpected CHTMLController layout; nextchromium stays inactive");
        return;
    }
    g_boot_vtbl = new void*[kControllerSlots];
    std::memcpy(g_boot_vtbl, g_orig_vtbl, sizeof(void*) * kControllerSlots);
    g_boot_vtbl[kRunFrameSlot] = reinterpret_cast<void*>(&boot_run_frame);
    *static_cast<void***>(g_controller) = g_boot_vtbl;
    nc::log("waiting for the first RunFrame (controller %p)", g_controller);
}

}  // namespace

// Miles calls RIB_Main for every *.asi it loads; nextchromium registers no sound providers.
extern "C" int __stdcall RIB_Main(void*, unsigned, void*, void*, void*) { return 1; }

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE pinned;  // never unloaded: the controller's vtable points into this module
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&RIB_Main), &pinned);
        start(self);
    }
    return TRUE;
}
