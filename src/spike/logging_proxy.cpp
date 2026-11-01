// Spike only: forwards every call to the original chromehtml objects and logs it.
#include "logging_proxy.h"

#include <atomic>

#include "../common/log.h"

namespace {

// Logs the first 15 calls of a method, then every 300th.
#define LOGC(fmt, ...)                                                  \
    do {                                                                \
        static std::atomic<int> n_{0};                                  \
        int c_ = ++n_;                                                  \
        if (c_ <= 15 || c_ % 300 == 0) nc::log("#%d " fmt, c_, __VA_ARGS__); \
    } while (0)

const char* s(const char* p) { return p ? p : "(null)"; }

class LogResponses final : public IHTMLResponses_HL1 {
public:
    explicit LogResponses(IHTMLResponses_HL1* t) : t_(t) {}
    void BrowserReady() override { LOGC("R.BrowserReady %p", t_); t_->BrowserReady(); }
    void BrowserNeedsPaint(int tex, int w, int h, const unsigned char* rgba, int ux, int uy, int uw, int uh, int cw,
                           int ch, const unsigned char* crgba) override {
        LOGC("R.NeedsPaint tex=%d %dx%d rgba=%p upd=%d,%d %dx%d combo=%dx%d %p px0=%02x%02x%02x%02x", tex, w, h, rgba,
             ux, uy, uw, uh, cw, ch, crgba, rgba ? rgba[0] : 0, rgba ? rgba[1] : 0, rgba ? rgba[2] : 0,
             rgba ? rgba[3] : 0);
        t_->BrowserNeedsPaint(tex, w, h, rgba, ux, uy, uw, uh, cw, ch, crgba);
    }
    void BrowserStartRequest(const char* url, const char* target, const char* post, bool redir) override {
        LOGC("R.StartRequest url=%s target=%s redir=%d", s(url), s(target), redir);
        t_->BrowserStartRequest(url, target, post, redir);
    }
    void BrowserURLChanged(const char* url, const char* post, bool redir) override {
        LOGC("R.URLChanged %s", s(url));
        t_->BrowserURLChanged(url, post, redir);
    }
    void BrowserFinishedRequest(const char* url, const char* title) override {
        LOGC("R.FinishedRequest %s title=%s", s(url), s(title));
        t_->BrowserFinishedRequest(url, title);
    }
    void BrowserShowPopup() override { LOGC("R.ShowPopup %d", 0); t_->BrowserShowPopup(); }
    void BrowserHidePopup() override { LOGC("R.HidePopup %d", 0); t_->BrowserHidePopup(); }
    void BrowserSizePopup(int x, int y, int w, int h) override {
        LOGC("R.SizePopup %d,%d %dx%d", x, y, w, h);
        t_->BrowserSizePopup(x, y, w, h);
    }
    void BrowserHorizontalScrollBarSizeResponse(int x, int y, int w, int h, int sc, int mx, float z) override {
        LOGC("R.HScroll %d,%d %dx%d s=%d max=%d z=%f", x, y, w, h, sc, mx, z);
        t_->BrowserHorizontalScrollBarSizeResponse(x, y, w, h, sc, mx, z);
    }
    void BrowserVerticalScrollBarSizeResponse(int x, int y, int w, int h, int sc, int mx, float z) override {
        LOGC("R.VScroll %d,%d %dx%d s=%d max=%d z=%f", x, y, w, h, sc, mx, z);
        t_->BrowserVerticalScrollBarSizeResponse(x, y, w, h, sc, mx, z);
    }
    void BrowserGetZoomResponse(float z) override { LOGC("R.Zoom %f", z); t_->BrowserGetZoomResponse(z); }
    void BrowserCanGoBackandForward(bool b, bool f) override {
        LOGC("R.CanGoBF %d %d", b, f);
        t_->BrowserCanGoBackandForward(b, f);
    }
    void BrowserJSAlert(const char* m) override { LOGC("R.JSAlert %s", s(m)); t_->BrowserJSAlert(m); }
    void BrowserJSConfirm(const char* m) override { LOGC("R.JSConfirm %s", s(m)); t_->BrowserJSConfirm(m); }
    void BrowserPopupHTMLWindow(const char* url, int w, int h, int x, int y) override {
        LOGC("R.PopupHTMLWindow %s %dx%d @%d,%d", s(url), w, h, x, y);
        t_->BrowserPopupHTMLWindow(url, w, h, x, y);
    }
    void BrowserSetHTMLTitle(const char* t) override { LOGC("R.Title %s", s(t)); t_->BrowserSetHTMLTitle(t); }
    void BrowserLoadingResource() override { LOGC("R.LoadingResource %d", 0); t_->BrowserLoadingResource(); }
    void BrowserStatusText(const char* t) override { LOGC("R.StatusText %s", s(t)); t_->BrowserStatusText(t); }
    void BrowserSetCursor(int c) override { LOGC("R.SetCursor %d", c); t_->BrowserSetCursor(c); }
    void BrowserFileLoadDialog() override { LOGC("R.FileLoadDialog %d", 0); t_->BrowserFileLoadDialog(); }
    void BrowserShowToolTip(const char* t) override { LOGC("R.ShowToolTip %s", s(t)); t_->BrowserShowToolTip(t); }
    void BrowserUpdateToolTip(const char* t) override {
        LOGC("R.UpdateToolTip %s", s(t));
        t_->BrowserUpdateToolTip(t);
    }
    void BrowserHideToolTip() override { LOGC("R.HideToolTip %d", 0); t_->BrowserHideToolTip(); }
    void BrowserClose() override { LOGC("R.Close %d", 0); t_->BrowserClose(); }
    void BrowserLinkAtPositionResponse(const char* url, int x, int y) override {
        LOGC("R.LinkAtPos %s %d,%d", s(url), x, y);
        t_->BrowserLinkAtPositionResponse(url, x, y);
    }

private:
    IHTMLResponses_HL1* t_;
};

class LogSerializer final : public IHTMLSerializer {
public:
    explicit LogSerializer(IHTMLSerializer* o) : o_(o) {}
    IHTMLResponses* GetResponseTarget() override {
        auto r = o_->GetResponseTarget();
        LOGC("S.GetResponseTarget -> %p", r);
        return r;
    }
    int BrowserGetIndex() override {
        int i = o_->BrowserGetIndex();
        LOGC("S.BrowserGetIndex -> %d", i);
        return i;
    }
    void BrowserPosition(int x, int y) override { LOGC("S.Position %d,%d", x, y); o_->BrowserPosition(x, y); }
    void PostURL(const char* u, const char* p) override {
        LOGC("S.PostURL %s post=%s", s(u), s(p));
        o_->PostURL(u, p);
    }
    void BrowserErrorStrings(const char* a, const char* b, const char* c, const char* d, const char* e,
                             const char* f, const char* g) override {
        LOGC("S.ErrorStrings title=%s", s(a));
        o_->BrowserErrorStrings(a, b, c, d, e, f, g);
    }
    void StopLoad() override { LOGC("S.StopLoad %d", 0); o_->StopLoad(); }
    void Reload() override { LOGC("S.Reload %d", 0); o_->Reload(); }
    void GoBack() override { LOGC("S.GoBack %d", 0); o_->GoBack(); }
    void GoForward() override { LOGC("S.GoForward %d", 0); o_->GoForward(); }
    void MouseDown(int c) override { LOGC("S.MouseDown %d", c); o_->MouseDown(c); }
    void MouseUp(int c) override { LOGC("S.MouseUp %d", c); o_->MouseUp(c); }
    void MouseWheel(int d) override { LOGC("S.MouseWheel %d", d); o_->MouseWheel(d); }
    void MouseMove(int x, int y) override { LOGC("S.MouseMove %d,%d", x, y); o_->MouseMove(x, y); }
    void MouseDoubleClick(int c) override { LOGC("S.MouseDbl %d", c); o_->MouseDoubleClick(c); }
    void BrowserSize(int w, int h) override { LOGC("S.Size %dx%d", w, h); o_->BrowserSize(w, h); }
    void KeyUp(int k, int m) override { LOGC("S.KeyUp %d m=%d", k, m); o_->KeyUp(k, m); }
    void KeyDown(int k, int m) override { LOGC("S.KeyDown %d m=%d", k, m); o_->KeyDown(k, m); }
    void KeyChar(int c) override { LOGC("S.KeyChar %d", c); o_->KeyChar(c); }
    void RunJavaScript(const char* js) override { LOGC("S.RunJS %.200s", s(js)); o_->RunJavaScript(js); }
    void SetHorizontalScroll(int v) override { LOGC("S.SetHScroll %d", v); o_->SetHorizontalScroll(v); }
    void SetVerticalScroll(int v) override { LOGC("S.SetVScroll %d", v); o_->SetVerticalScroll(v); }
    void SetFocus(bool f) override { LOGC("S.SetFocus %d", f); o_->SetFocus(f); }
    void AddHeader(const char* h, const char* v) override {
        LOGC("S.AddHeader %s: %s", s(h), s(v));
        o_->AddHeader(h, v);
    }
    void NeedsPaintResponse(int t) override { LOGC("S.NeedsPaintResponse %d", t); o_->NeedsPaintResponse(t); }
    void StartRequestResponse(bool r) override { LOGC("S.StartRequestResponse %d", r); o_->StartRequestResponse(r); }
    void RequestBrowserSizes() override { LOGC("S.RequestBrowserSizes %d", 0); o_->RequestBrowserSizes(); }
    void JSDialogResponse(int r) override { LOGC("S.JSDialogResponse %d", r); o_->JSDialogResponse(r); }
    void GetLinkAtPosition(int x, int y) override { LOGC("S.GetLinkAtPos %d,%d", x, y); o_->GetLinkAtPosition(x, y); }

private:
    IHTMLSerializer* o_;
};

class LogController final : public IHTMLChromeController {
public:
    explicit LogController(IHTMLChromeController* o) : o_(o) {}
    bool Init(const char* cache, const char* cookies) override {
        bool r = o_->Init(cache, cookies);
        LOGC("C.Init cache=%s cookies=%s -> %d", s(cache), s(cookies), r);
        return r;
    }
    void Shutdown() override { LOGC("C.Shutdown %d", 0); o_->Shutdown(); }
    bool RunFrame() override {
        bool r = o_->RunFrame();
        LOGC("C.RunFrame -> %d", r);
        return r;
    }
    void SetWebCookie(const char* h, const char* k, const char* v, const char* p, RTime32 e) override {
        LOGC("C.SetWebCookie %s %s=%s", s(h), s(k), s(v));
        o_->SetWebCookie(h, k, v, p, e);
    }
    void GetWebCookiesForURL(CUtlString* out, const char* url, const char* name) override {
        LOGC("C.GetWebCookiesForURL %s %s", s(url), s(name));
        o_->GetWebCookiesForURL(out, url, name);
    }
    void SetClientBuildID(uint64_t id) override {
        LOGC("C.SetClientBuildID %llu", id);
        o_->SetClientBuildID(id);
    }
    bool BHasPendingMessages() override {
        bool r = o_->BHasPendingMessages();
        LOGC("C.BHasPendingMessages -> %d", r);
        return r;
    }
    void CreateBrowser(IHTMLResponses* b, bool popup, const char* ua) override {
        LOGC("C.CreateBrowser %p popup=%d ua=%s", b, popup, s(ua));
        o_->CreateBrowser(b, popup, ua);
    }
    void RemoveBrowser(IHTMLResponses* b) override { LOGC("C.RemoveBrowser %p", b); o_->RemoveBrowser(b); }
    void WakeThread() override { LOGC("C.WakeThread %d", 0); o_->WakeThread(); }
    HTMLCommandBuffer_t* GetFreeCommandBuffer(EHTMLCommands c, int i) override {
        LOGC("C.GetFreeCommandBuffer %d %d", (int)c, i);
        return o_->GetFreeCommandBuffer(c, i);
    }
    void PushCommand(HTMLCommandBuffer_t* b) override { LOGC("C.PushCommand %p", b); o_->PushCommand(b); }
    void SetCefThreadTargetFrameRate(uint32_t f) override {
        LOGC("C.SetCefThreadTargetFrameRate %u", f);
        o_->SetCefThreadTargetFrameRate(f);
    }
    IHTMLSerializer* CreateSerializer(IHTMLResponses_HL1* t) override {
        auto* resp = new LogResponses(t);
        auto* ser = o_->CreateSerializer(resp);
        LOGC("C.CreateSerializer target=%p -> %p", t, ser);
        return ser ? new LogSerializer(ser) : nullptr;
    }

private:
    IHTMLChromeController* o_;
};

}  // namespace

IHTMLChromeController* make_logging_controller(IHTMLChromeController* original) {
    return new LogController(original);
}
IHTMLResponses_HL1* make_log_responses(IHTMLResponses_HL1* t) { return new LogResponses(t); }
IHTMLSerializer* make_log_serializer(IHTMLSerializer* s) { return new LogSerializer(s); }
