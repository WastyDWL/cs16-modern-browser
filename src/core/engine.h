// Process-wide owner of CEF and of the Browser objects, keyed by the game's serializer pointer.
// All methods are main-thread only.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "common/valve_html.h"
#include "core/api_scheme.h"
#include "core/browser.h"
#include "core/domains.h"
#include "core/game_bridge.h"
#include "core/config.h"
#include "core/main_thread_queue.h"

namespace nc {

struct EngineOptions {
    std::wstring mod_dir;  // ...\nextchromium
    Config config;
};

class Engine {
public:
    static Engine& get();

    // Loads nextchromium\cef\libcef.dll and starts CEF. False = stay on the original browser.
    bool init(const EngineOptions& opts);
    void shutdown();
    bool running() const { return running_; }

    // Drains CEF -> game callbacks and delivers new frames.
    void run_frame();
    Browser* find(const void* key);
    Browser* get_or_create(const void* key, IHTMLResponses_HL1* target);
    void remove(const void* key);
    void set_cookie(const char* host, const char* key, const char* value, const char* path, uint32_t expires);
    void set_frame_rate(int fps);
    // A window.nextclient call from a page whose frame is at frame_url.
    ApiResult handle_api(const ApiCall& call, const std::string& frame_url);

    MainThreadQueue& queue() { return queue_; }

private:
    bool running_ = false;
    Config config_;
    Domains domains_;
    GameBridge bridge_;
    unsigned frames_ = 0;
    int next_id_ = 1;
    MainThreadQueue queue_;
    std::map<const void*, CefRefPtr<Browser>> browsers_;
    std::vector<CefRefPtr<Browser>> closing_;
};

// Points the delay-loaded libcef.dll import at <dir>\libcef.dll and loads it.
bool load_cef_library(const std::wstring& cef_dir);

}  // namespace nc
