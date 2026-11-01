#include "core/engine.h"

#include <windows.h>

#include "common/log.h"
#include "include/cef_app.h"
#include "include/cef_cookie.h"
#include "include/cef_version.h"

namespace nc {

namespace {

class BrowserApp final : public CefApp, public CefBrowserProcessHandler {
public:
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
        std::erase_if(closing_, [](auto& b) { return !b->cef_browser(); });
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
    // The game may remove panels from inside BrowserNeedsPaint, so iterate over a copy.
    std::vector<CefRefPtr<Browser>> live;
    live.reserve(browsers_.size());
    for (auto& [key, b] : browsers_) live.push_back(b);
    for (auto& b : live) b->deliver_frame();
    std::erase_if(closing_, [](auto& b) { return !b->cef_browser(); });
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
    b->create("");
    return b.get();
}

void Engine::remove(const void* key) {
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

void Engine::set_frame_rate(int fps) {
    for (auto& [key, b] : browsers_)
        if (auto cb = b->cef_browser()) cb->GetHost()->SetWindowlessFrameRate(fps);
}

}  // namespace nc
