// Serves ncb://api/<method>?a=<arg> (window.nextclient's calls into the game). Requests arrive on
// CEF's IO thread; each one is answered on the game's main thread through Engine::handle_api.
#include "core/api_scheme.h"

#include <algorithm>
#include <cstring>

#include "common/ncb_scheme.h"
#include "core/engine.h"
#include "core/js_api.h"

namespace nc {

namespace {

class ApiHandler final : public CefResourceHandler {
public:
    ApiHandler(std::string url, std::string frame_url) : url_(std::move(url)), frame_url_(std::move(frame_url)) {}

    bool Open(CefRefPtr<CefRequest>, bool& handle_request, CefRefPtr<CefCallback> callback) override {
        handle_request = false;
        std::optional<ApiCall> call = parse_api_url(url_);
        if (!call) {
            status_ = 404;
            handle_request = true;
            return true;
        }
        CefRefPtr<ApiHandler> self(this);
        Engine::get().queue().post([self, call = *call, callback] {
            ApiResult r = Engine::get().handle_api(call, self->frame_url_);
            self->status_ = r.status;
            self->body_ = std::move(r.json);
            callback->Continue();
        });
        return true;
    }

    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& length, CefString&) override {
        response->SetStatus(status_);
        response->SetMimeType("application/json");
        CefResponse::HeaderMap headers;
        headers.emplace("Access-Control-Allow-Origin", "*");
        headers.emplace("Cache-Control", "no-store");
        response->SetHeaderMap(headers);
        length = static_cast<int64_t>(body_.size());
    }

    bool Read(void* out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback>) override {
        size_t n = std::min(static_cast<size_t>(bytes_to_read), body_.size() - offset_);
        std::memcpy(out, body_.data() + offset_, n);
        offset_ += n;
        bytes_read = static_cast<int>(n);
        return n > 0;
    }

    void Cancel() override {}

private:
    const std::string url_, frame_url_;
    int status_ = 500;
    std::string body_;
    size_t offset_ = 0;

    IMPLEMENT_REFCOUNTING(ApiHandler);
};

class ApiFactory final : public CefSchemeHandlerFactory {
public:
    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, const CefString&,
                                         CefRefPtr<CefRequest> request) override {
        // Trust is decided by the URL of the frame that made the call, never by the page's JS.
        // Only XHR/fetch: for a navigation (redirect, top.location) the frame URL would still be
        // the previous, possibly trusted, page.
        if (!api_resource_type_allowed(request->GetResourceType())) return new ApiHandler("", "");
        return new ApiHandler(request->GetURL().ToString(), frame ? frame->GetURL().ToString() : "");
    }

    IMPLEMENT_REFCOUNTING(ApiFactory);
};

}  // namespace

void register_api_scheme_handler() { CefRegisterSchemeHandlerFactory(kNcbScheme, kNcbApiHost, new ApiFactory()); }

}  // namespace nc
