#include <windows.h>

#include <string>

#include "common/log.h"
#include "core/config.h"
#include "core/engine.h"
#include "core/hooks.h"

namespace {

bool windows10_or_later() {
    using RtlGetVersion_t = LONG(WINAPI*)(OSVERSIONINFOW*);
    auto fn = reinterpret_cast<RtlGetVersion_t>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW v{sizeof(v)};
    return fn && fn(&v) == 0 && v.dwMajorVersion >= 10;
}

}  // namespace

// Called by nextchromium.asi on the game's main thread (first RunFrame after load).
// controller: the CHTMLController singleton; controller_vtbl: its original vtable.
// Returns true when the new browser took over; false leaves the original untouched.
extern "C" __declspec(dllexport) bool nc_activate(void* controller, void** controller_vtbl, const wchar_t* mod_dir) {
    std::wstring dir = mod_dir;
    nc::log_open((dir + L"\\nextchromium.log").c_str(), false);

    nc::Config cfg = nc::load_config(dir + L"\\nextchromium.ini", GetCommandLineW());
    if (!cfg.enabled) {
        nc::log("disabled by nextchromium.ini or -nonextchromium");
        return false;
    }
    if (!windows10_or_later()) {
        nc::log("Windows 10 or later is required; staying on the original browser");
        return false;
    }
    HMODULE chromehtml = GetModuleHandleW(L"chromehtml.dll");
    if (!nc::check_compatible(controller, controller_vtbl, chromehtml)) return false;
    if (!nc::Engine::get().init({dir, cfg})) return false;
    if (!nc::install_hooks(controller, controller_vtbl, chromehtml)) {
        nc::Engine::get().shutdown();
        return false;
    }
    nc::log("controller: nextchromium active");
    return true;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
