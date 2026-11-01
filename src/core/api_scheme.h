#pragma once

#include <string>

namespace nc {

struct ApiResult {
    int status = 200;  // 403: domain not trusted, 404: unknown method, 501: not available
    std::string json = "null";
};

// Call after CefInitialize, in the browser process.
void register_api_scheme_handler();

}  // namespace nc
