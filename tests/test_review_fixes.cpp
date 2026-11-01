// Regression tests for the final review findings.
#include <cstring>

#include "core/api_router.h"
#include "core/browser.h"
#include "core/frame_buffer.h"
#include "core/js_api.h"
#include "core/main_thread_queue.h"
#include "core/navigation_gate.h"
#include "core/panel_registry.h"
#include "testing.h"

using namespace nc;

// Critical: ncb://api must only answer XHR/fetch, never a navigation (redirect or top.location),
// whose frame URL is still the previous, possibly trusted, page.
TEST(api_only_for_xhr_and_fetch) {
    CHECK(api_resource_type_allowed(RT_XHR));
    CHECK(!api_resource_type_allowed(RT_MAIN_FRAME));
    CHECK(!api_resource_type_allowed(RT_SUB_FRAME));
    CHECK(!api_resource_type_allowed(RT_IMAGE));
    CHECK(!api_resource_type_allowed(RT_SCRIPT));
}

TEST(nav_ncb_scheme_never_navigable) {
    NavigationGate g;
    g.allow_once("x");
    CHECK(!g.on_before_browse("ncb://api/exec?a=quit", false, /*is_redirect=*/true));
    CHECK(!g.on_before_browse("NCB://api/exec?a=quit", true));
    CHECK(!g.on_before_browse("ncb://api/exec?a=quit", false, false, /*main=*/false));
    CHECK_EQ(g.pending(), std::string(""));  // never offered to the game either
}

// Important: shutdown must wait for browsers still being created.
TEST(browser_not_finished_until_before_close) {
    MainThreadQueue q;
    CefRefPtr<Browser> b = new Browser(1, nullptr, q);
    CHECK(!b->finished());
    b->close();
    CHECK(!b->finished());  // CEF may still be creating it
    b->OnBeforeClose(nullptr);
    CHECK(b->finished());
}

// Important: a popup rect outside the view must not produce a negative-length copy.
TEST(paint_popup_outside_view_ignored) {
    FrameBuffer fb;
    uint32_t view[4] = {1, 1, 1, 1};  // 2x2
    uint32_t popup[4] = {9, 9, 9, 9};
    fb.store(view, 2, 2);
    fb.store_popup(popup, 2, 2);
    fb.set_popup_rect(5, 0, 2, 2);  // entirely to the right of the view
    uint32_t got[4] = {};
    fb.deliver_if_ready([&](int, int, int, const unsigned char* p) { std::memcpy(got, p, 16); });
    CHECK_EQ(got[0], 1u);
    CHECK_EQ(got[3], 1u);
}

// Important: a panel removed by the game must not get a new browser from a late serializer call.
TEST(registry_no_lazy_browser_after_remove) {
    PanelRegistry r;
    int panel = 0;
    CHECK(r.lazy_allowed(&panel));  // panels created before nextchromium loaded
    r.removed(&panel);
    CHECK(!r.lazy_allowed(&panel));
    r.created(&panel);  // the allocator may reuse the address for a new panel
    CHECK(r.lazy_allowed(&panel));
}

// Re-graded: secrets must not be readable by any page through getCvarValue.
namespace {
struct SecretGame : GameApi {
    std::optional<std::string> cvar(const std::string&) override { return std::string("hunter2"); }
    void client_cmd(const std::string&) override {}
    std::optional<ConnectedHost> host() override { return std::nullopt; }
    bool gameui_shown() override { return false; }
};
}  // namespace

TEST(router_secret_cvars_hidden) {
    SecretGame g;
    Domains d;
    for (const char* name : {"rcon_password", "RCON_Password", "password", "sv_password", "_pw", "setinfo_pw"})
        CHECK_EQ(route_api({"getCvarValue", name}, "", d, g).json, std::string("null"));
    CHECK_EQ(route_api({"getCvarValue", "name"}, "", d, g).json, std::string("\"hunter2\""));
}
