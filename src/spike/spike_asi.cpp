// Spike: learn load order and the real call pattern between the game and chromehtml.dll.
#include <windows.h>
#include <shlwapi.h>

#include <intrin.h>

#include <atomic>
#include <string>

#include "../common/log.h"
#include "../common/valve_html.h"
#include "MinHook.h"
#include "logging_proxy.h"

namespace {

std::atomic<bool> g_chromehtml_hooked{false};
CreateInterfaceFn g_orig_create_interface = nullptr;
IHTMLChromeController* g_proxy = nullptr;

std::string module_of(void* addr) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(addr), &m))
        GetModuleFileNameA(m, name, MAX_PATH);
    const char* base = strrchr(name, '\\');
    return base ? base + 1 : name;
}

void* hk_create_interface(const char* name, int* rc) {
    void* r = g_orig_create_interface(name, rc);
    nc::log("chromehtml!CreateInterface(%s) from %s -> %p", name ? name : "(null)",
            module_of(_ReturnAddress()).c_str(), r);
    if (r && name && strcmp(name, CHROMEHTML_CONTROLLER_INTERFACE_VERSION) == 0) {
        if (!g_proxy) g_proxy = make_logging_controller(static_cast<IHTMLChromeController*>(r));
        return g_proxy;
    }
    return r;
}

// --- vtable patch of the already-created controller (the .asi loads after hw.dll got it) ---
void** g_orig_vtbl = nullptr;
template <class Fn>
Fn orig(int i) {
    return reinterpret_cast<Fn>(g_orig_vtbl[i]);
}
bool __fastcall v_init(void* self, void*, const char* a, const char* b) {
    bool r = orig<bool(__thiscall*)(void*, const char*, const char*)>(1)(self, a, b);
    nc::log("V.Init %s %s -> %d", a ? a : "", b ? b : "", r);
    return r;
}
void __fastcall v_shutdown(void* self, void*) {
    nc::log("V.Shutdown");
    orig<void(__thiscall*)(void*)>(2)(self);
}
bool __fastcall v_runframe(void* self, void*) {
    static std::atomic<int> n{0};
    int c = ++n;
    if (c <= 3 || c % 1000 == 0) nc::log("V.RunFrame #%d", c);
    return orig<bool(__thiscall*)(void*)>(3)(self);
}
void __fastcall v_create_browser(void* self, void*, IHTMLResponses* b, bool popup, const char* ua) {
    nc::log("V.CreateBrowser %p popup=%d ua=%s from %s", b, popup, ua ? ua : "", module_of(_ReturnAddress()).c_str());
    orig<void(__thiscall*)(void*, IHTMLResponses*, bool, const char*)>(8)(self, b, popup, ua);
}
void __fastcall v_remove_browser(void* self, void*, IHTMLResponses* b) {
    nc::log("V.RemoveBrowser %p", b);
    orig<void(__thiscall*)(void*, IHTMLResponses*)>(9)(self, b);
}
void __fastcall v_set_fps(void* self, void*, uint32_t f) {
    nc::log("V.SetCefThreadTargetFrameRate %u", f);
    orig<void(__thiscall*)(void*, uint32_t)>(13)(self, f);
}
IHTMLSerializer* __fastcall v_create_serializer(void* self, void*, IHTMLResponses_HL1* t) {
    auto* ser = orig<IHTMLSerializer*(__thiscall*)(void*, IHTMLResponses_HL1*)>(14)(self, make_log_responses(t));
    nc::log("V.CreateSerializer target=%p from %s -> %p", t, module_of(_ReturnAddress()).c_str(), ser);
    return ser ? make_log_serializer(ser) : nullptr;
}

void patch_existing_controller() {
    void* ctl = g_orig_create_interface(CHROMEHTML_CONTROLLER_INTERFACE_VERSION, nullptr);
    nc::log("existing controller %p", ctl);
    if (!ctl) return;
    g_orig_vtbl = *static_cast<void***>(ctl);
    int count = 0;
    while (count < 64) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(g_orig_vtbl[count], &mbi, sizeof(mbi)) || !(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE)))
            break;
        nc::log("  vtbl[%d] = %p (%s)", count, g_orig_vtbl[count], module_of(g_orig_vtbl[count]).c_str());
        ++count;
    }
    auto** vt = new void*[count];
    memcpy(vt, g_orig_vtbl, sizeof(void*) * count);
    vt[1] = reinterpret_cast<void*>(&v_init);
    vt[2] = reinterpret_cast<void*>(&v_shutdown);
    vt[3] = reinterpret_cast<void*>(&v_runframe);
    vt[8] = reinterpret_cast<void*>(&v_create_browser);
    vt[9] = reinterpret_cast<void*>(&v_remove_browser);
    vt[13] = reinterpret_cast<void*>(&v_set_fps);
    vt[14] = reinterpret_cast<void*>(&v_create_serializer);
    *static_cast<void***>(ctl) = vt;
    nc::log("patched controller vptr (%d entries)", count);
}

// --- RTTI: find the vtable of class `name` (at subobject offset `off`) inside module m ---
void** find_vtable(HMODULE m, const char* name, uint32_t off) {
    auto* base = reinterpret_cast<uint8_t*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    uint32_t size = nt->OptionalHeader.SizeOfImage;
    size_t nlen = strlen(name);
    uint8_t* td = nullptr;
    for (uint32_t i = 8; i + nlen < size; ++i)
        if (memcmp(base + i, name, nlen + 1) == 0) { td = base + i - 8; break; }
    if (!td) return nullptr;
    for (uint32_t i = 0; i + 20 <= size; i += 4) {
        auto* col = reinterpret_cast<uint32_t*>(base + i);
        if (col[0] == 0 && col[1] == off && col[3] == reinterpret_cast<uint32_t>(td)) {
            for (uint32_t j = 0; j + 4 <= size; j += 4)
                if (*reinterpret_cast<uint32_t*>(base + j) == reinterpret_cast<uint32_t>(col))
                    return reinterpret_cast<void**>(base + j + 4);
        }
    }
    return nullptr;
}

void** g_ser_vtbl_orig = nullptr;  // copy of original entries
template <class Fn>
Fn sorig(int i) {
    return reinterpret_cast<Fn>(g_ser_vtbl_orig[i]);
}
void dump_object(void* self) {
    auto* d = static_cast<uint32_t*>(self);
    for (int i = 0; i < 48; ++i) {
        MEMORY_BASIC_INFORMATION mbi{};
        std::string extra;
        if (d[i] > 0x10000 && VirtualQuery(reinterpret_cast<void*>(d[i]), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
            !(mbi.Protect & PAGE_GUARD) && (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE))) {
            uint32_t v = *reinterpret_cast<uint32_t*>(d[i]);
            if (VirtualQuery(reinterpret_cast<void*>(v), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT)
                extra = " -> *=" + module_of(reinterpret_cast<void*>(v));
        }
        nc::log("    obj[%2d] +%03x = %08x %s%s", i, i * 4, d[i], module_of(reinterpret_cast<void*>(d[i])).c_str(),
                extra.c_str());
    }
}
int __fastcall s_get_index(void* self, void*) {
    int r = sorig<int(__thiscall*)(void*)>(1)(self);
    static std::atomic<int> n{0};
    if (++n <= 10) nc::log("SV.BrowserGetIndex %p -> %d from %s", self, r, module_of(_ReturnAddress()).c_str());
    return r;
}
void __fastcall s_post_url(void* self, void*, const char* u, const char* p) {
    nc::log("SV.PostURL %p %s from %s", self, u ? u : "", module_of(_ReturnAddress()).c_str());
    static bool dumped = false;
    if (!dumped) { dumped = true; dump_object(self); }
    sorig<void(__thiscall*)(void*, const char*, const char*)>(3)(self, u, p);
}
void __fastcall s_size(void* self, void*, int w, int h) {
    nc::log("SV.BrowserSize %p %dx%d", self, w, h);
    sorig<void(__thiscall*)(void*, int, int)>(14)(self, w, h);
}
void __fastcall s_paint_resp(void* self, void*, int t) {
    static std::atomic<int> n{0};
    if (++n <= 5) nc::log("SV.NeedsPaintResponse %p %d", self, t);
    sorig<void(__thiscall*)(void*, int)>(24)(self, t);
}
void __fastcall s_keychar(void* self, void*, int c) {
    nc::log("SV.KeyChar %p %d", self, c);
    sorig<void(__thiscall*)(void*, int)>(17)(self, c);
}
void __fastcall s_run_js(void* self, void*, const char* js) {
    nc::log("SV.RunJS %p %.150s", self, js ? js : "");
    sorig<void(__thiscall*)(void*, const char*)>(18)(self, js);
}

void patch_serializer_class(HMODULE m) {
    void** vt = find_vtable(m, ".?AVHTMLSerializer@@", 4);  // IHTMLSerializer subobject; +0 is IHTMLResponses
    nc::log("HTMLSerializer IHTMLSerializer vtable(off 4) = %p", vt);
    for (uint32_t off : {4u, 8u, 12u}) nc::log("  vtable(off %u) = %p", off, find_vtable(m, ".?AVHTMLSerializer@@", off));
    if (!vt) return;
    g_ser_vtbl_orig = new void*[29];
    memcpy(g_ser_vtbl_orig, vt, sizeof(void*) * 29);
    DWORD old;
    VirtualProtect(vt, sizeof(void*) * 29, PAGE_READWRITE, &old);
    vt[1] = reinterpret_cast<void*>(&s_get_index);
    vt[3] = reinterpret_cast<void*>(&s_post_url);
    vt[14] = reinterpret_cast<void*>(&s_size);
    vt[17] = reinterpret_cast<void*>(&s_keychar);
    vt[18] = reinterpret_cast<void*>(&s_run_js);
    vt[24] = reinterpret_cast<void*>(&s_paint_resp);
    VirtualProtect(vt, sizeof(void*) * 29, old, &old);
    nc::log("patched HTMLSerializer class vtable");
}

void try_hook_chromehtml() {
    HMODULE m = GetModuleHandleW(L"chromehtml.dll");
    if (!m || g_chromehtml_hooked.exchange(true)) return;
    void* target = reinterpret_cast<void*>(GetProcAddress(m, "CreateInterface"));
    MH_STATUS a = MH_CreateHook(target, reinterpret_cast<void*>(&hk_create_interface),
                                reinterpret_cast<void**>(&g_orig_create_interface));
    MH_STATUS b = MH_EnableHook(target);
    nc::log("hooked chromehtml!CreateInterface %p: create=%d enable=%d", target, a, b);
    patch_existing_controller();
    patch_serializer_class(m);
}

using LoadLibraryExW_t = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);
using LoadLibraryExA_t = HMODULE(WINAPI*)(LPCSTR, HANDLE, DWORD);
using LoadLibraryW_t = HMODULE(WINAPI*)(LPCWSTR);
using LoadLibraryA_t = HMODULE(WINAPI*)(LPCSTR);
LoadLibraryExW_t o_llexw;
LoadLibraryExA_t o_llexa;
LoadLibraryW_t o_llw;
LoadLibraryA_t o_lla;

void after_load(const char* what, HMODULE r, void* ret) {
    if (what && (StrStrIA(what, "chromehtml") || StrStrIA(what, "libcef") || StrStrIA(what, "GameUI") ||
                 StrStrIA(what, "client.dll") || StrStrIA(what, "next_")))
        nc::log("LoadLibrary(%s) from %s -> %p", what, module_of(ret).c_str(), r);
    if (r && !g_chromehtml_hooked) try_hook_chromehtml();
}

std::string narrow(LPCWSTR w) {
    if (!w) return {};
    char buf[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), nullptr, nullptr);
    return buf;
}

HMODULE WINAPI hk_llexw(LPCWSTR n, HANDLE f, DWORD fl) {
    HMODULE r = o_llexw(n, f, fl);
    after_load(narrow(n).c_str(), r, _ReturnAddress());
    return r;
}
HMODULE WINAPI hk_llexa(LPCSTR n, HANDLE f, DWORD fl) {
    HMODULE r = o_llexa(n, f, fl);
    after_load(n, r, _ReturnAddress());
    return r;
}
HMODULE WINAPI hk_llw(LPCWSTR n) {
    HMODULE r = o_llw(n);
    after_load(narrow(n).c_str(), r, _ReturnAddress());
    return r;
}
HMODULE WINAPI hk_lla(LPCSTR n) {
    HMODULE r = o_lla(n);
    after_load(n, r, _ReturnAddress());
    return r;
}

void start(HMODULE self) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(self, path, MAX_PATH);
    *wcsrchr(path, L'\\') = 0;
    std::wstring log_path = std::wstring(path) + L"\\nextchromium_spike.log";
    nc::log_open(log_path.c_str(), true);
    nc::log("spike asi loaded; cmdline=%s", GetCommandLineA());
    for (const wchar_t* m : {L"hw.dll", L"chromehtml.dll", L"libcef.dll", L"client.dll", L"GameUI.dll",
                             L"next_engine_mini.dll", L"vgui2.dll", L"nitro_api2.dll"})
        nc::log("  already loaded %ls: %p", m, GetModuleHandleW(m));

    MH_Initialize();
    MH_CreateHookApi(L"kernel32", "LoadLibraryExW", &hk_llexw, reinterpret_cast<void**>(&o_llexw));
    MH_CreateHookApi(L"kernel32", "LoadLibraryExA", &hk_llexa, reinterpret_cast<void**>(&o_llexa));
    MH_CreateHookApi(L"kernel32", "LoadLibraryW", &hk_llw, reinterpret_cast<void**>(&o_llw));
    MH_CreateHookApi(L"kernel32", "LoadLibraryA", &hk_lla, reinterpret_cast<void**>(&o_lla));
    nc::log("enable LoadLibrary hooks: %d", MH_EnableHook(MH_ALL_HOOKS));
    try_hook_chromehtml();
}

}  // namespace

// Miles Sound System calls this for every *.asi it loads; we register no providers.
extern "C" int __stdcall RIB_Main(void*, unsigned, void*, void*, void*) {
    nc::log("RIB_Main called");
    return 1;
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        start(self);
    }
    return TRUE;
}

