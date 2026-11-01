#include "core/api_router.h"

#include <cctype>
#include <set>

namespace nc {

namespace {

// Any page may call getCvarValue (as in NextClient), so passwords stay hidden.
bool secret_cvar(std::string name) {
    for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return name.find("password") != std::string::npos || name.find("_pw") != std::string::npos;
}

}  // namespace

ApiResult route_api(const ApiCall& call, const std::string& frame_url, const Domains& domains, GameApi& game) {
    const std::string& m = call.method;
    if (m == "exec" || m == "openOptionsDialog") {
        if (!domains.is_accepted(frame_url)) return {403, "null"};
        if (m == "openOptionsDialog") return {501, "null"};  // needs GameUI internals NextClient keeps private
        game.client_cmd(call.arg);
        return {200, "true"};
    }
    if (m == "getCvarValue") {
        if (secret_cvar(call.arg)) return {200, "null"};
        std::optional<std::string> v = game.cvar(call.arg);
        return {200, v ? json_string(*v) : "null"};
    }
    if (m == "connectedHost") return {200, host_json(game.host())};
    if (m == "gameUiShown") return {200, game.gameui_shown() ? "true" : "false"};

    static const std::set<std::string> matchmaking = {"getServerInfo",      "getPlayersInfo",    "getRules",
                                                      "getFavoriteServers", "getHistoryServers", "addFavoriteServer",
                                                      "removeFavoriteServer"};
    if (matchmaking.count(m)) return {501, "null"};
    return {404, "null"};
}

}  // namespace nc
