// window.nextclient — NextClient's JS API (gameui/src/Browser, engine_mini/src/binding/jsapi),
// re-implemented for multi-process Chromium. Page JS calls the game through synchronous
// requests to ncb://api/<method>?a=<arg>, served in the browser process; game events are
// pushed into every frame with dispatch_script().
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "include/internal/cef_types.h"

namespace nc {

struct ApiCall {
    std::string method;
    std::string arg;  // percent-decoded UTF-8
};

std::optional<ApiCall> parse_api_url(std::string_view url);
// ncb://api answers only XHR and fetch; navigations to it are refused.
bool api_resource_type_allowed(cef_resource_type_t type);
// JSON string literal; also escapes < and > so it is safe inside inline script.
std::string json_string(std::string_view utf8);

struct PlayerInfo {
    std::string name;
    uint64_t steam_id = 0;
    bool local = false;
};
struct ConnectedHost {
    std::string address;
    bool joined = false;  // "joined" once in the server, "loading" before
    std::vector<PlayerInfo> players;
};
// nextclient.connectedHost value, as NextClient builds it.
std::string host_json(const std::optional<ConnectedHost>& host);

// JS that fires `type` on `target` ("nextclient", "nextclient.gameUi", ...) if the API exists.
std::string dispatch_script(std::string_view target, std::string_view type, std::string_view detail_json);
// Bootstrap evaluated in every new JS context.
const char* api_script();

}  // namespace nc
