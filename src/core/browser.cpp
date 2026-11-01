#include "core/browser.h"

#include <windows.h>

#include "common/log.h"
#include "core/keymap.h"
#include "core/main_thread_queue.h"
#include "core/view_math.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"

namespace nc {

namespace {

// vgui2::HTML sends delta * 100 / 1.5 per notch; Chromium expects 120 per notch.
int wheel_to_cef(int delta) { return delta * 9 / 5; }

std::string error_page(const ErrorStrings& e, const std::string& detail) {
    return "<html><head><meta charset=\"utf-8\"><title>" + e.title + "</title></head><body><h1>" + e.header +
           "</h1>" + detail + "</body></html>";
}

}  // namespace

Browser::Browser(int id, IHTMLResponses_HL1* target, MainThreadQueue& queue)
    : id_(id), target_(target), queue_(queue) {}

void Browser::to_game(std::function<void(IHTMLResponses_HL1*)> fn) {
    CefRefPtr<Browser> self(this);
    queue_.post([self, fn = std::move(fn)] {
        if (!self->closed_ && self->target_) fn(self->target_);
    });
}

CefRefPtr<CefBrowserHost> Browser::host() {
    std::lock_guard lock(mutex_);
    return browser_ ? browser_->GetHost() : nullptr;
}

CefRefPtr<CefBrowser> Browser::cef_browser() {
    std::lock_guard lock(mutex_);
    return browser_;
}

void Browser::create(const std::string&) {
    CefWindowInfo info;
    info.SetAsWindowless(nullptr);
    CefBrowserSettings settings;
    settings.windowless_frame_rate = 60;
    settings.background_color = CefColorSetARGB(0, 0, 0, 0);
    CefBrowserHost::CreateBrowser(info, this, "about:blank", settings, nullptr, nullptr);
}

void Browser::close() {
    closed_ = true;
    target_ = nullptr;
    if (auto h = host()) h->CloseBrowser(true);
}

int Browser::index() const {
    std::lock_guard lock(mutex_);
    return browser_ ? id_ : -1;
}

void Browser::load(const std::string& url, const std::string& post_data) {
    CefRefPtr<CefBrowser> b = cef_browser();
    if (!b) return;
    {
        std::lock_guard lock(mutex_);
        gate_.allow_once(url);
    }
    if (post_data.empty()) {
        b->GetMainFrame()->LoadURL(url);
        return;
    }
    CefRefPtr<CefRequest> req = CefRequest::Create();
    CefRefPtr<CefPostData> pd = CefPostData::Create();
    CefRefPtr<CefPostDataElement> el = CefPostDataElement::Create();
    el->SetToBytes(post_data.size(), post_data.data());
    pd->AddElement(el);
    CefRequest::HeaderMap headers;
    headers.emplace("Content-Type", "application/x-www-form-urlencoded");
    req->Set(url, "POST", pd, headers);
    b->GetMainFrame()->LoadRequest(req);
}

void Browser::post_url(const char* url, const char* post_data) {
    std::string u = url ? url : "", p = post_data ? post_data : "";
    log("browser %d: PostURL %s", id_, u.c_str());
    bool ready;
    {
        std::lock_guard lock(mutex_);
        ready = browser_ != nullptr;
        if (!ready) {
            pending_url_ = u;
            pending_post_ = p;
        }
    }
    if (ready) load(u, p);
}

void Browser::set_size(int w, int h) {
    CefSize s = clamp_view_size(w, h);
    {
        std::lock_guard lock(mutex_);
        if (s.width == width_ && s.height == height_) return;
        width_ = s.width;
        height_ = s.height;
    }
    if (auto hst = host()) hst->WasResized();
}

void Browser::set_position(int x, int y) {
    std::lock_guard lock(mutex_);
    screen_x_ = x;
    screen_y_ = y;
}

void Browser::set_error_strings(ErrorStrings s) {
    std::lock_guard lock(mutex_);
    errors_ = std::move(s);
}

void Browser::stop_load() {
    if (auto b = cef_browser()) b->StopLoad();
}
void Browser::reload() {
    if (auto b = cef_browser()) b->Reload();
}
void Browser::go_back() {
    if (auto b = cef_browser()) b->GoBack();
}
void Browser::go_forward() {
    if (auto b = cef_browser()) b->GoForward();
}

void Browser::send_click(int code, bool up, int count) {
    auto h = host();
    if (!h) return;
    cef_mouse_button_type_t btn = mouse_button(code);
    buttons_[btn] = !up;
    CefMouseEvent e;
    e.x = mouse_x_;
    e.y = mouse_y_;
    e.modifiers = mouse_flags(mods_, buttons_[MBT_LEFT], buttons_[MBT_MIDDLE], buttons_[MBT_RIGHT]);
    h->SendMouseClickEvent(e, btn, up, count);
}

void Browser::mouse_down(int code) { send_click(code, false, 1); }
void Browser::mouse_up(int code) { send_click(code, true, 1); }
void Browser::mouse_double_click(int code) {
    send_click(code, false, 2);
    send_click(code, true, 2);
}

void Browser::mouse_wheel(int delta) {
    auto h = host();
    if (!h) return;
    CefMouseEvent e;
    e.x = mouse_x_;
    e.y = mouse_y_;
    e.modifiers = mouse_flags(mods_, buttons_[MBT_LEFT], buttons_[MBT_MIDDLE], buttons_[MBT_RIGHT]);
    h->SendMouseWheelEvent(e, 0, wheel_to_cef(delta));
}

void Browser::mouse_move(int x, int y) {
    mouse_x_ = x;
    mouse_y_ = y;
    auto h = host();
    if (!h) return;
    CefMouseEvent e;
    e.x = x;
    e.y = y;
    e.modifiers = mouse_flags(mods_, buttons_[MBT_LEFT], buttons_[MBT_MIDDLE], buttons_[MBT_RIGHT]);
    h->SendMouseMoveEvent(e, false);
}

void Browser::key_down(int vk, int mods) {
    mods_ = mods;
    if (auto h = host()) h->SendKeyEvent(make_key_event(KeyKind::Down, vk, mods));
}
void Browser::key_up(int vk, int mods) {
    mods_ = mods;
    if (auto h = host()) h->SendKeyEvent(make_key_event(KeyKind::Up, vk, mods));
}
void Browser::key_char(int ch) {
    if (auto h = host()) h->SendKeyEvent(make_key_event(KeyKind::Char, ch, mods_));
}

void Browser::run_js(const char* js) {
    if (!js) return;
    if (auto b = cef_browser()) b->GetMainFrame()->ExecuteJavaScript(js, "", 0);
}

void Browser::set_focus(bool focus) {
    if (auto h = host()) h->SetFocus(focus);
}

void Browser::add_header(const char* name, const char* value) {
    if (!name || !*name) return;
    std::lock_guard lock(mutex_);
    headers_[name] = value ? value : "";
}

void Browser::needs_paint_response(int texture_id) { frame_.on_response(texture_id); }

void Browser::start_request_response(bool allow) {
    std::string url;
    {
        std::lock_guard lock(mutex_);
        url = gate_.on_response(allow);
    }
    if (!url.empty())
        if (auto b = cef_browser()) b->GetMainFrame()->LoadURL(url);
}

void Browser::js_dialog_response(int result) {
    CefRefPtr<CefJSDialogCallback> cb;
    {
        std::lock_guard lock(mutex_);
        cb.swap(dialog_);
    }
    if (cb) cb->Continue(result != 0, "");
}

void Browser::get_link_at(int x, int y) {
    // Hit-testing links needs a renderer round trip; the panel only uses it for its context menu.
    if (target_) target_->BrowserLinkAtPositionResponse("", x, y);
}

void Browser::request_sizes() {}

void Browser::deliver_frame() {
    if (closed_ || !target_) return;
    frame_.deliver_if_ready([this](int tex, int w, int h, const unsigned char* bgra) {
        target_->BrowserNeedsPaint(tex, w, h, bgra, 0, 0, 0, 0, 0, 0, nullptr);
    });
}

bool Browser::OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId,
                                       CefRefPtr<CefProcessMessage>) {
    return false;
}

// --- CefRenderHandler ---

void Browser::GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) {
    std::lock_guard lock(mutex_);
    rect = CefRect(0, 0, width_, height_);
}

bool Browser::GetScreenPoint(CefRefPtr<CefBrowser>, int vx, int vy, int& sx, int& sy) {
    std::lock_guard lock(mutex_);
    sx = screen_x_ + vx;
    sy = screen_y_ + vy;
    return true;
}

void Browser::OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const void* buffer, int width,
                      int height) {
    if (type == PET_VIEW) frame_.store(buffer, width, height);
    else frame_.store_popup(buffer, width, height);
}

void Browser::OnPopupShow(CefRefPtr<CefBrowser>, bool show) {
    if (!show) frame_.hide_popup();
}

void Browser::OnPopupSize(CefRefPtr<CefBrowser>, const CefRect& r) { frame_.set_popup_rect(r.x, r.y, r.width, r.height); }

// --- CefLifeSpanHandler ---

bool Browser::OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString& target_url,
                            const CefString&, CefLifeSpanHandler::WindowOpenDisposition, bool, const CefPopupFeatures& f, CefWindowInfo&,
                            CefRefPtr<CefClient>&, CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*) {
    std::string url = target_url.ToString();
    int w = f.widthSet ? f.width : 0, h = f.heightSet ? f.height : 0;
    int x = f.xSet ? f.x : 0, y = f.ySet ? f.y : 0;
    to_game([url, w, h, x, y](IHTMLResponses_HL1* t) { t->BrowserPopupHTMLWindow(url.c_str(), w, h, x, y); });
    return true;  // the game opens its own HTMLPopup panel
}

void Browser::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    std::string url, post;
    {
        std::lock_guard lock(mutex_);
        browser_ = browser;
        url.swap(pending_url_);
        post.swap(pending_post_);
    }
    log("browser %d: created", id_);
    if (closed_) {
        browser->GetHost()->CloseBrowser(true);
        return;
    }
    browser->GetHost()->WasResized();
    if (!url.empty()) load(url, post);
    to_game([](IHTMLResponses_HL1* t) { t->BrowserReady(); });
}

bool Browser::DoClose(CefRefPtr<CefBrowser>) {
    if (!closed_) to_game([](IHTMLResponses_HL1* t) { t->BrowserClose(); });
    return !closed_;  // window.close(): keep the browser until the game removes the panel
}

void Browser::OnBeforeClose(CefRefPtr<CefBrowser>) {
    std::lock_guard lock(mutex_);
    browser_ = nullptr;
    dialog_ = nullptr;
}

// --- CefLoadHandler ---

void Browser::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool loading, bool back, bool forward) {
    to_game([back, forward](IHTMLResponses_HL1* t) { t->BrowserCanGoBackandForward(back, forward); });
    if (loading) {
        to_game([](IHTMLResponses_HL1* t) { t->BrowserLoadingResource(); });
        return;
    }
    {
        std::lock_guard lock(mutex_);
        gate_.mark_loaded();
    }
    CefRefPtr<CefNavigationEntry> entry = browser->GetHost()->GetVisibleNavigationEntry();
    std::string url = browser->GetMainFrame()->GetURL().ToString();
    std::string title = entry ? entry->GetTitle().ToString() : "";
    to_game([url, title](IHTMLResponses_HL1* t) { t->BrowserFinishedRequest(url.c_str(), title.c_str()); });
}

void Browser::OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code, const CefString& text,
                          const CefString& failed_url) {
    log("browser %d: load error %d %s (%s)", id_, code, failed_url.ToString().c_str(), text.ToString().c_str());
    if (code == ERR_ABORTED || !frame->IsMain()) return;
    std::string html;
    {
        std::lock_guard lock(mutex_);
        const std::string& detail =
            (code == ERR_NAME_NOT_RESOLVED || code == ERR_INTERNET_DISCONNECTED || code == ERR_CONNECTION_REFUSED ||
             code == ERR_CONNECTION_TIMED_OUT || code == ERR_CONNECTION_RESET)
                ? errors_.connection
            : code == ERR_PROXY_CONNECTION_FAILED ? errors_.proxy
            : code == ERR_INVALID_URL             ? errors_.bad_url
            : code == ERR_CACHE_MISS              ? errors_.cache_miss
                                                  : errors_.unknown;
        if (detail.empty() && errors_.header.empty()) return;  // keep Chromium's own error page
        html = error_page(errors_, detail);
    }
    frame->LoadURL("data:text/html;charset=utf-8," + CefURIEncode(html, false).ToString());
}

// --- CefDisplayHandler ---

void Browser::OnAddressChange(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, const CefString& url) {
    if (!frame->IsMain()) return;
    std::string u = url.ToString();
    to_game([u](IHTMLResponses_HL1* t) { t->BrowserURLChanged(u.c_str(), "", false); });
}

void Browser::OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) {
    std::string s = title.ToString();
    to_game([s](IHTMLResponses_HL1* t) { t->BrowserSetHTMLTitle(s.c_str()); });
}

bool Browser::OnTooltip(CefRefPtr<CefBrowser>, CefString& text) {
    std::string s = text.ToString();
    to_game([s](IHTMLResponses_HL1* t) {
        if (s.empty()) t->BrowserHideToolTip();
        else t->BrowserShowToolTip(s.c_str());
    });
    return true;
}

void Browser::OnStatusMessage(CefRefPtr<CefBrowser>, const CefString& value) {
    std::string s = value.ToString();
    to_game([s](IHTMLResponses_HL1* t) { t->BrowserStatusText(s.c_str()); });
}

bool Browser::OnCursorChange(CefRefPtr<CefBrowser>, CefCursorHandle, cef_cursor_type_t type, const CefCursorInfo&) {
    int c = vgui_cursor(type);
    to_game([c](IHTMLResponses_HL1* t) { t->BrowserSetCursor(c); });
    return true;
}

// --- CefRequestHandler ---

bool Browser::OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request,
                             bool user_gesture, bool is_redirect) {
    std::string url = request->GetURL().ToString();
    bool allow;
    {
        std::lock_guard lock(mutex_);
        allow = gate_.on_before_browse(url, user_gesture, is_redirect, frame->IsMain());
    }
    if (!allow)
        to_game([url](IHTMLResponses_HL1* t) { t->BrowserStartRequest(url.c_str(), "", "", false); });
    return !allow;  // true cancels
}

CefRefPtr<CefResourceRequestHandler> Browser::GetResourceRequestHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                                                                        CefRefPtr<CefRequest>, bool, bool,
                                                                        const CefString&, bool&) {
    std::lock_guard lock(mutex_);
    return headers_.empty() ? nullptr : this;
}

cef_return_value_t Browser::OnBeforeResourceLoad(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                                                 CefRefPtr<CefRequest> request, CefRefPtr<CefCallback>) {
    std::lock_guard lock(mutex_);
    for (auto& [name, value] : headers_) request->SetHeaderByName(name, value, true);
    return RV_CONTINUE;
}

// --- CefJSDialogHandler ---

bool Browser::OnJSDialog(CefRefPtr<CefBrowser>, const CefString&, JSDialogType type, const CefString& message,
                         const CefString&, CefRefPtr<CefJSDialogCallback> callback, bool& suppress) {
    if (type == JSDIALOGTYPE_PROMPT) {
        suppress = true;  // the game has no prompt dialog
        return false;
    }
    {
        std::lock_guard lock(mutex_);
        if (dialog_) dialog_->Continue(false, "");
        dialog_ = callback;
    }
    std::string m = message.ToString();
    bool confirm = type == JSDIALOGTYPE_CONFIRM;
    to_game([m, confirm](IHTMLResponses_HL1* t) {
        if (confirm) t->BrowserJSConfirm(m.c_str());
        else t->BrowserJSAlert(m.c_str());
    });
    return true;
}

bool Browser::OnBeforeUnloadDialog(CefRefPtr<CefBrowser>, const CefString&, bool,
                                   CefRefPtr<CefJSDialogCallback> callback) {
    callback->Continue(true, "");
    return true;
}

}  // namespace nc
