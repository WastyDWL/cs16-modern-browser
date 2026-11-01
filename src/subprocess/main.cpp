// Chromium helper process. In the renderer it installs window.nextclient into every JS context.
#include <windows.h>

#include "common/ncb_scheme.h"
#include "core/js_api.h"
#include "include/cef_app.h"

namespace {

class SubprocessApp final : public CefApp, public CefRenderProcessHandler {
public:
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
        nc::register_ncb_scheme(registrar);
    }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override {
        CefRefPtr<CefV8Value> result;
        CefRefPtr<CefV8Exception> error;
        context->Eval(nc::api_script(), "ncb://api/bootstrap.js", 1, result, error);
    }

    IMPLEMENT_REFCOUNTING(SubprocessApp);
};

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    CefMainArgs args(instance);
    CefRefPtr<CefApp> app = new SubprocessApp();
    return CefExecuteProcess(args, app, nullptr);
}
