#include "core/engine.h"

#include <windows.h>

#include "common/log.h"
#include "common/ncb_scheme.h"
#include "core/api_router.h"
#include "include/cef_app.h"
#include "include/cef_cookie.h"
#include "include/cef_version.h"

namespace nc {

namespace {

class BrowserApp final : public CefApp, public CefBrowserProcessHandler {
public:
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override { register_ncb_scheme(registrar); }
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> cmd) override {
        if (!process_type.empty()) return;
        // Software rendering: frames are copied into a vgui texture anyway, and it keeps the
        // GPU process away from the game's OpenGL context and driver.
        cmd->AppendSwitch("disable-gpu");
        cmd->AppendSwitch("disable-gpu-compositing");
        cmd->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
        cmd->AppendSwitch("disable-extensions");
    }

    IMPLEMENT_REFCOUNTING(BrowserApp);
};

void set_str(cef_string_t& dst, const std::wstring& v) {
    CefString str(&dst);
    str = v;
}

}  // namespace

Engine& Engine::get() {
    static Engine engine;
    return engine;
}

bool Engine::init(const EngineOptions& opts) {
    if (running_) return true;
    std::wstring cef_dir = opts.mod_dir + L"\\cef";
    if (!load_cef_library(cef_dir)) return false;

    CefMainArgs args(GetModuleHandleW(nullptr));
    CefSettings s;
    s.no_sandbox = true;
    s.multi_threaded_message_loop = true;
    s.windowless_rendering_enabled = true;
    s.persist_session_cookies = true;
    s.remote_debugging_port = opts.config.debug_port;
    s.log_severity = LOGSEVERITY_WARNING;
    set_str(s.browser_subprocess_path, cef_dir + L"\\ncb_subprocess.exe");
    set_str(s.resources_dir_path, cef_dir);
    set_str(s.locales_dir_path, cef_dir + L"\\locales");
    set_str(s.root_cache_path, opts.mod_dir + L"\\cache");
    set_str(s.cache_path, opts.mod_dir + L"\\cache\\default");
    set_str(s.log_file, opts.mod_dir + L"\\cef.log");
    CefString(&s.accept_language_list) = "ru-RU,ru,en-US,en";
    CefString(&s.locale) = "ru";

    CefRefPtr<CefApp> app = new BrowserApp();
    if (!CefInitialize(args, s, app, nullptr)) {
        log("CefInitialize failed (exit code %d)", CefGetExitCode());
        return false;
    }
    running_ = true;
    config_ = opts.config;
    register_api_scheme_handler();
    std::wstring game_dir = opts.mod_dir.substr(0, opts.mod_dir.find_last_of(L"\\/"));
    if (!domains_.load(game_dir + L"\\platform\\accepted_domains.txt"))
        log("no platform\\accepted_domains.txt: privileged nextclient.* calls are denied");
    bridge_.attach();
    log("CEF %s initialized", CEF_VERSION);
    return true;
}

void Engine::shutdown() {
    if (!running_) return;
    for (auto& [key, b] : browsers_) {
        b->close();
        closing_.push_back(b);
    }
    browsers_.clear();
    // CefShutdown requires every browser to be closed first.
    for (int i = 0; i < 300; ++i) {
        std::erase_if(closing_, [](auto& b) { return b->finished(); });
        if (closing_.empty()) break;
        Sleep(10);
    }
    queue_.drain();
    CefShutdown();
    running_ = false;
    log("CEF shut down");
}

void Engine::run_frame() {
    queue_.drain();
    if (!bridge_.attached() && ++frames_ % 120 == 0) bridge_.attach();
    for (const GameEvent& ev : bridge_.poll()) {
        std::string js = dispatch_script(ev.target, ev.type, ev.detail_json);
        for (auto& [key, b] : browsers_) {
            CefRefPtr<CefBrowser> cb = b->cef_browser();
            if (!cb) continue;
            std::vector<CefString> ids;
            cb->GetFrameIdentifiers(ids);
            for (auto& id : ids)
                if (auto f = cb->GetFrameByIdentifier(id)) f->ExecuteJavaScript(js, f->GetURL(), 0);
        }
    }
    // The game may remove panels from inside BrowserNeedsPaint, so iterate over a copy.
    std::vector<CefRefPtr<Browser>> live;
    live.reserve(browsers_.size());
    for (auto& [key, b] : browsers_) live.push_back(b);
    for (auto& b : live) b->deliver_frame();
    std::erase_if(closing_, [](auto& b) { return b->finished(); });
}

Browser* Engine::find(const void* key) {
    auto it = browsers_.find(key);
    return it == browsers_.end() ? nullptr : it->second.get();
}

Browser* Engine::get_or_create(const void* key, IHTMLResponses_HL1* target) {
    if (Browser* b = find(key)) return b;
    if (!running_ || !target) return nullptr;
    CefRefPtr<Browser> b = new Browser(next_id_++, target, queue_);
    browsers_[key] = b;
    log("browser %d: new for serializer %p, target %p", next_id_ - 1, key, target);
    b->create(config_.frame_rate, zoom_level(config_.zoom_percent), config_.fit_width);
    return b.get();
}

Browser* Engine::create_for_panel(const void* key, IHTMLResponses_HL1* target) {
    panels_.created(key);
    return get_or_create(key, target);
}

Browser* Engine::lazy_for_panel(const void* key, IHTMLResponses_HL1* target) {
    if (Browser* b = find(key)) return b;
    return panels_.lazy_allowed(key) ? get_or_create(key, target) : nullptr;
}

void Engine::remove(const void* key) {
    panels_.removed(key);
    auto it = browsers_.find(key);
    if (it == browsers_.end()) return;
    it->second->close();
    closing_.push_back(it->second);
    browsers_.erase(it);
}

void Engine::set_cookie(const char* host, const char* key, const char* value, const char* path, uint32_t expires) {
    if (!running_ || !host || !key) return;
    CefCookie c;
    CefString(&c.name) = key;
    CefString(&c.value) = value ? value : "";
    CefString(&c.domain) = host;
    CefString(&c.path) = path && *path ? path : "/";
    if (expires) {
        c.has_expires = true;
        CefBaseTime t;
        t.val = (static_cast<int64_t>(expires) + 11644473600LL) * 1000000LL;  // unix -> windows epoch, us
        c.expires = t;
    }
    std::string url = std::string("https://") + (host[0] == '.' ? host + 1 : host) + "/";
    CefCookieManager::GetGlobalManager(nullptr)->SetCookie(url, c, nullptr);
}

ApiResult Engine::handle_api(const ApiCall& call, const std::string& frame_url) {
    return route_api(call, frame_url, domains_, bridge_);
}

void Engine::set_frame_rate(int fps) {
    for (auto& [key, b] : browsers_)
        if (auto cb = b->cef_browser()) cb->GetHost()->SetWindowlessFrameRate(fps);
}

}  // namespace nc
