// Redirects the game's chromehtml.dll objects to the Engine (see docs/spike-results.md):
//  - the CHTMLController singleton gets a copied vtable with our slots;
//  - the HTMLSerializer class vtable (IHTMLSerializer subobject at +4) is patched in place,
//    which also covers panels created before nextchromium was loaded.
#include "core/hooks.h"

#include <windows.h>

#include <cstring>

#include "common/log.h"
#include "common/rtti.h"
#include "core/engine.h"

namespace nc {
namespace {

constexpr int kControllerSlots = 15;
constexpr int kSerializerSlots = 28;
constexpr uint32_t kSerializerTargetOffset = 0xC;  // IHTMLResponses_HL1* inside the IHTMLSerializer subobject

void** g_ctl_orig = nullptr;  // original CHTMLController vtable
void** g_ser_orig = nullptr;  // copy of the original HTMLSerializer (IHTMLSerializer) slots

template <class Fn>
Fn ctl_orig(int slot) {
    return reinterpret_cast<Fn>(g_ctl_orig[slot]);
}

IHTMLResponses_HL1* target_of(void* ser) {
    auto* slot = reinterpret_cast<IHTMLResponses_HL1**>(static_cast<char*>(ser) + kSerializerTargetOffset);
    if (!readable(slot, sizeof(*slot))) return nullptr;
    IHTMLResponses_HL1* t = *slot;
    if (!readable(t, sizeof(void*)) || !in_module_image(*reinterpret_cast<void**>(t))) return nullptr;
    return t;
}

Browser* browser_of(void* ser) { return Engine::get().lazy_for_panel(ser, target_of(ser)); }

// IHTMLResponses* handed to CreateBrowser/RemoveBrowser is the HTMLSerializer object (+0);
// our key is its IHTMLSerializer subobject (+4).
void* key_of(IHTMLResponses* r) { return reinterpret_cast<char*>(r) + 4; }

// --- CHTMLController slots (thiscall emulated with fastcall: ecx = this, edx unused) ---

void __fastcall c_shutdown(void* self, void*) {
    Engine::get().shutdown();
    ctl_orig<void(__thiscall*)(void*)>(2)(self);
}
bool __fastcall c_run_frame(void*, void*) {
    static int n = 0;
    if (++n == 1 || n % 2000 == 0) log("RunFrame #%d", n);
    Engine::get().run_frame();
    return true;
}
void __fastcall c_set_cookie(void*, void*, const char* host, const char* key, const char* value, const char* path,
                             RTime32 expires) {
    Engine::get().set_cookie(host, key, value, path, expires);
}
void __fastcall c_create_browser(void*, void*, IHTMLResponses* r, bool, const char*) {
    void* key = key_of(r);
    Engine::get().create_for_panel(key, target_of(key));
}
void __fastcall c_remove_browser(void*, void*, IHTMLResponses* r) { Engine::get().remove(key_of(r)); }
void __fastcall c_set_fps(void*, void*, uint32_t fps) { Engine::get().set_frame_rate(static_cast<int>(fps)); }

// --- HTMLSerializer slots ---

#define BROWSER(self)                     \
    Browser* b = browser_of(self);        \
    if (!b) return

int __fastcall s_get_index(void* self, void*) {
    Browser* b = browser_of(self);
    return b ? b->index() : -1;
}
void __fastcall s_position(void* self, void*, int x, int y) { BROWSER(self); b->set_position(x, y); }
void __fastcall s_post_url(void* self, void*, const char* url, const char* post) { BROWSER(self); b->post_url(url, post); }
void __fastcall s_error_strings(void* self, void*, const char* title, const char* header, const char* cache_miss,
                                const char* bad_url, const char* conn, const char* proxy, const char* unknown) {
    BROWSER(self);
    auto s = [](const char* p) { return std::string(p ? p : ""); };
    b->set_error_strings({s(title), s(header), s(cache_miss), s(bad_url), s(conn), s(proxy), s(unknown)});
}
void __fastcall s_stop(void* self, void*) { BROWSER(self); b->stop_load(); }
void __fastcall s_reload(void* self, void*) { BROWSER(self); b->reload(); }
void __fastcall s_back(void* self, void*) { BROWSER(self); b->go_back(); }
void __fastcall s_forward(void* self, void*) { BROWSER(self); b->go_forward(); }
void __fastcall s_mouse_down(void* self, void*, int c) { BROWSER(self); b->mouse_down(c); }
void __fastcall s_mouse_up(void* self, void*, int c) { BROWSER(self); b->mouse_up(c); }
void __fastcall s_mouse_wheel(void* self, void*, int d) { BROWSER(self); b->mouse_wheel(d); }
void __fastcall s_mouse_move(void* self, void*, int x, int y) { BROWSER(self); b->mouse_move(x, y); }
void __fastcall s_mouse_dbl(void* self, void*, int c) { BROWSER(self); b->mouse_double_click(c); }
void __fastcall s_size(void* self, void*, int w, int h) { BROWSER(self); b->set_size(w, h); }
void __fastcall s_key_up(void* self, void*, int k, int m) { BROWSER(self); b->key_up(k, m); }
void __fastcall s_key_down(void* self, void*, int k, int m) { BROWSER(self); b->key_down(k, m); }
void __fastcall s_key_char(void* self, void*, int c) { BROWSER(self); b->key_char(c); }
void __fastcall s_run_js(void* self, void*, const char* js) { BROWSER(self); b->run_js(js); }
void __fastcall s_hscroll(void*, void*, int) {}
void __fastcall s_vscroll(void*, void*, int) {}
void __fastcall s_focus(void* self, void*, bool f) { BROWSER(self); b->set_focus(f); }
void __fastcall s_add_header(void* self, void*, const char* n, const char* v) { BROWSER(self); b->add_header(n, v); }
void __fastcall s_paint_response(void* self, void*, int t) {
    static int n = 0;
    if (++n <= 3) log("NeedsPaintResponse %d from the game", t);
    BROWSER(self);
    b->needs_paint_response(t);
}
void __fastcall s_start_request_response(void* self, void*, bool r) { BROWSER(self); b->start_request_response(r); }
void __fastcall s_request_sizes(void* self, void*) { BROWSER(self); b->request_sizes(); }
void __fastcall s_js_dialog_response(void* self, void*, int r) { BROWSER(self); b->js_dialog_response(r); }
void __fastcall s_link_at(void* self, void*, int x, int y) { BROWSER(self); b->get_link_at(x, y); }

bool vtable_in_module(void** vt, int slots, HMODULE m) {
    for (int i = 0; i < slots; ++i) {
        HMODULE owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCWSTR>(vt[i]), &owner) ||
            owner != m) {
            log("vtable %p slot %d = %p is outside chromehtml.dll (%p)", vt, i, vt[i], owner);
            return false;
        }
    }
    return true;
}

}  // namespace

bool check_compatible(void* controller, void** controller_vtbl, HMODULE chromehtml) {
    if (!controller || !controller_vtbl || !vtable_in_module(controller_vtbl, kControllerSlots, chromehtml)) {
        log("incompatible: CHTMLController vtable does not look like chromehtml's");
        return false;
    }
    void** ser = find_vtable(chromehtml, ".?AVHTMLSerializer@@", 4);
    log("HTMLSerializer IHTMLSerializer vtable: %p", ser);
    if (!ser || !vtable_in_module(ser, kSerializerSlots, chromehtml)) {
        log("incompatible: HTMLSerializer (IHTMLSerializer at +4) not found");
        return false;
    }
    return true;
}

bool install_hooks(void* controller, void** controller_vtbl, HMODULE chromehtml) {
    g_ctl_orig = controller_vtbl;
    void** ser = find_vtable(chromehtml, ".?AVHTMLSerializer@@", 4);
    if (!ser) return false;

    g_ser_orig = new void*[kSerializerSlots];
    std::memcpy(g_ser_orig, ser, sizeof(void*) * kSerializerSlots);
    void* ser_slots[] = {
        g_ser_orig[0],  // GetResponseTarget: the original returns the object's IHTMLResponses (+0)
        &s_get_index, &s_position, &s_post_url, &s_error_strings, &s_stop, &s_reload, &s_back, &s_forward,
        &s_mouse_down, &s_mouse_up, &s_mouse_wheel, &s_mouse_move, &s_mouse_dbl, &s_size, &s_key_up, &s_key_down,
        &s_key_char, &s_run_js, &s_hscroll, &s_vscroll, &s_focus, &s_add_header, &s_paint_response,
        &s_start_request_response, &s_request_sizes, &s_js_dialog_response, &s_link_at,
    };
    static_assert(sizeof(ser_slots) / sizeof(ser_slots[0]) == kSerializerSlots);
    DWORD old;
    if (!VirtualProtect(ser, sizeof(void*) * kSerializerSlots, PAGE_READWRITE, &old)) return false;
    std::memcpy(ser, ser_slots, sizeof(ser_slots));
    VirtualProtect(ser, sizeof(void*) * kSerializerSlots, old, &old);

    auto** ctl = new void*[kControllerSlots];
    std::memcpy(ctl, controller_vtbl, sizeof(void*) * kControllerSlots);
    ctl[2] = reinterpret_cast<void*>(&c_shutdown);
    ctl[3] = reinterpret_cast<void*>(&c_run_frame);
    ctl[4] = reinterpret_cast<void*>(&c_set_cookie);
    ctl[8] = reinterpret_cast<void*>(&c_create_browser);
    ctl[9] = reinterpret_cast<void*>(&c_remove_browser);
    ctl[13] = reinterpret_cast<void*>(&c_set_fps);
    *static_cast<void***>(controller) = ctl;
    log("hooks installed: controller %p, HTMLSerializer vtable %p", controller, ser);
    return true;
}

}  // namespace nc
