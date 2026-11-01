// One game HTML panel backed by a windowless CEF browser.
// Main-thread methods mirror IHTMLSerializer; CEF callbacks queue work for the main thread.
#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include "common/valve_html.h"
#include "core/frame_buffer.h"
#include "core/navigation_gate.h"
#include "include/cef_client.h"

namespace nc {

class MainThreadQueue;

struct ErrorStrings {
    std::string title, header, cache_miss, bad_url, connection, proxy, unknown;
};

class Browser final : public CefClient,
                      public CefRenderHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefDisplayHandler,
                      public CefRequestHandler,
                      public CefResourceRequestHandler,
                      public CefJSDialogHandler,
                      public CefContextMenuHandler {
public:
    Browser(int id, IHTMLResponses_HL1* target, MainThreadQueue& queue);

    // --- main thread ---
    void create(int frame_rate, double zoom_level, bool fit_width);
    void close();  // the panel is gone: no more callbacks into the game
    int index() const;  // -1 until the CEF browser exists
    void post_url(const char* url, const char* post_data);
    void set_size(int w, int h);
    void set_position(int x, int y);
    void set_error_strings(ErrorStrings s);
    void stop_load();
    void reload();
    void go_back();
    void go_forward();
    void mouse_down(int code);
    void mouse_up(int code);
    void mouse_double_click(int code);
    void mouse_wheel(int delta);
    void mouse_move(int x, int y);
    void key_down(int vk, int mods);
    void key_up(int vk, int mods);
    void key_char(int ch);
    void run_js(const char* js);
    void set_focus(bool focus);
    void add_header(const char* name, const char* value);
    void needs_paint_response(int texture_id);
    void start_request_response(bool allow);
    void js_dialog_response(int result);
    void get_link_at(int x, int y);
    void request_sizes();
    void deliver_frame();  // called every RunFrame
    bool closed() const { return closed_.load(); }
    CefRefPtr<CefBrowser> cef_browser();

    // --- CefClient ---
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                  CefProcessId source, CefRefPtr<CefProcessMessage> message) override;

    // --- CefRenderHandler ---
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int vx, int vy, int& sx, int& sy) override;
    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirty, const void* buffer,
                 int width, int height) override;
    void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
    void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;

    // --- CefLifeSpanHandler ---
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id,
                       const CefString& target_url, const CefString& target_frame_name,
                       CefLifeSpanHandler::WindowOpenDisposition disposition, bool user_gesture, const CefPopupFeatures& features,
                       CefWindowInfo& window_info, CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
                       CefRefPtr<CefDictionaryValue>& extra_info, bool* no_javascript_access) override;
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // --- CefLoadHandler ---
    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool loading, bool back, bool forward) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int http_status) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode code,
                     const CefString& text, const CefString& failed_url) override;

    // --- CefDisplayHandler ---
    void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url) override;
    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    bool OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) override;
    void OnStatusMessage(CefRefPtr<CefBrowser> browser, const CefString& value) override;
    bool OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor, cef_cursor_type_t type,
                        const CefCursorInfo& custom) override;

    // --- CefRequestHandler / CefResourceRequestHandler ---
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request,
                        bool user_gesture, bool is_redirect) override;
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool is_navigation,
        bool is_download, const CefString& request_initiator, bool& disable_default_handling) override;
    cef_return_value_t OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                            CefRefPtr<CefRequest> request,
                                            CefRefPtr<CefCallback> callback) override;

    // --- CefJSDialogHandler ---
    bool OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& origin_url, JSDialogType type,
                    const CefString& message, const CefString& default_prompt,
                    CefRefPtr<CefJSDialogCallback> callback, bool& suppress) override;
    bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString& message, bool is_reload,
                              CefRefPtr<CefJSDialogCallback> callback) override;

    // --- CefContextMenuHandler: no native context menu in OSR ---
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                             CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) override {
        model->Clear();
    }

private:
    // Runs fn(target) on the main thread unless the panel was removed meanwhile.
    void to_game(std::function<void(IHTMLResponses_HL1*)> fn);
    CefRefPtr<CefBrowserHost> host();
    void send_click(int code, bool up, int count);
    void load(const std::string& url, const std::string& post_data);

    const int id_;
    IHTMLResponses_HL1* target_;  // main thread only
    MainThreadQueue& queue_;
    std::atomic<bool> closed_{false};

    mutable std::mutex mutex_;  // guards everything below
    CefRefPtr<CefBrowser> browser_;
    int width_ = 640, height_ = 480;
    int screen_x_ = 0, screen_y_ = 0;
    std::string pending_url_, pending_post_;
    std::map<std::string, std::string> headers_;
    ErrorStrings errors_;
    NavigationGate gate_;
    CefRefPtr<CefJSDialogCallback> dialog_;

    FrameBuffer frame_;
    int mouse_x_ = 0, mouse_y_ = 0;  // main thread
    int mods_ = 0;
    double zoom_level_ = 0;  // set before create(), read on the CEF UI thread
    bool fit_width_ = false;
    bool buttons_[3] = {};

    IMPLEMENT_REFCOUNTING(Browser);
};

}  // namespace nc
