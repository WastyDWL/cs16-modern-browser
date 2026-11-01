// Answers one window.nextclient call (ncb://api/...). Privileged calls (exec, the options
// dialog) need the calling frame's URL on the accepted-domains list, like NextClient.
#pragma once

#include <optional>
#include <string>

#include "core/api_scheme.h"
#include "core/domains.h"
#include "core/js_api.h"

namespace nc {

class GameApi {
public:
    virtual ~GameApi() = default;
    virtual std::optional<std::string> cvar(const std::string& name) = 0;
    virtual void client_cmd(const std::string& cmd) = 0;
    virtual std::optional<ConnectedHost> host() = 0;
    virtual bool gameui_shown() = 0;
};

ApiResult route_api(const ApiCall& call, const std::string& frame_url, const Domains& domains, GameApi& game);

}  // namespace nc
