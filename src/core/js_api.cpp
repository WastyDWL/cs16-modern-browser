#include "core/js_api.h"

#include <cstdio>

namespace nc {

namespace {

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percent_decode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
            out += static_cast<char>(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

// SteamID account type 1 (individual) — CSteamID::BIndividualAccount in NextClient's code.
bool individual_account(uint64_t id) { return ((id >> 52) & 0xF) == 1; }

constexpr char kApiScript[] = R"JS((function () {
  if (window.nextclient && window.nextclient.__ncb) return;
  function url(method, arg) {
    return 'ncb://api/' + method + (arg === undefined ? '' : '?a=' + encodeURIComponent(String(arg)));
  }
  function call(method, arg) {
    try {
      var x = new XMLHttpRequest();
      x.open('GET', url(method, arg), false);
      x.send();
      return x.status === 200 ? JSON.parse(x.responseText) : null;
    } catch (e) {
      return null;
    }
  }
  function callAsync(method, arg) {
    return fetch(url(method, arg)).then(function (r) {
      if (!r.ok) throw new Error('nextclient.' + method + ': ' + r.status);
      return r.json();
    });
  }
  window.makeAsEventTarget = function (object) {
    var target = new EventTarget();
    Object.defineProperty(object, 'eventTarget', { get: function () { return target; } });
    object.addEventListener = function () { return target.addEventListener.apply(target, arguments); };
    object.removeEventListener = function () { return target.removeEventListener.apply(target, arguments); };
    object.dispatchEvent = function (type, params) {
      return target.dispatchEvent(new CustomEvent(type, { detail: params, cancelable: true }));
    };
  };
  var nextclient = window.nextclient = { __ncb: true };
  nextclient.exec = function (cmd) { if (typeof cmd == 'string') return call('exec', cmd); };
  nextclient.getCvarValue = function (name) { return call('getCvarValue', name); };
  Object.defineProperty(nextclient, 'connectedHost', { get: function () { return call('connectedHost'); } });
  makeAsEventTarget(nextclient);

  nextclient.gameUi = { optionsDialog: {} };
  Object.defineProperty(nextclient.gameUi, 'shown', { get: function () { return call('gameUiShown') === true; } });
  nextclient.gameUi.optionsDialog.open = function (tab) { if (typeof tab == 'string') call('openOptionsDialog', tab); };
  makeAsEventTarget(nextclient.gameUi);

  nextclient.console = {};
  makeAsEventTarget(nextclient.console);

  nextclient.matchmaking = {
    getServerInfo: function (ip) { return callAsync('getServerInfo', ip); },
    getPlayersInfo: function (ip) { return callAsync('getPlayersInfo', ip); },
    getRules: function (ip) { return callAsync('getRules', ip); },
    getFavoriteServers: function () { return callAsync('getFavoriteServers'); },
    getHistoryServers: function () { return callAsync('getHistoryServers'); },
    addFavoriteServer: function (ip) { return callAsync('addFavoriteServer', ip); },
    removeFavoriteServer: function (ip) { return callAsync('removeFavoriteServer', ip); }
  };
})();)JS";

}  // namespace

std::optional<ApiCall> parse_api_url(std::string_view url) {
    constexpr std::string_view prefix = "ncb://api/";
    if (url.substr(0, prefix.size()) != prefix) return std::nullopt;
    url.remove_prefix(prefix.size());
    size_t q = url.find('?');
    ApiCall call{std::string(url.substr(0, q)), ""};
    if (call.method.empty()) return std::nullopt;
    if (q != std::string_view::npos) {
        std::string_view query = url.substr(q + 1);
        if (query.substr(0, 2) == "a=") call.arg = percent_decode(query.substr(2));
    }
    return call;
}

std::string json_string(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<': out += "\\u003c"; break;
            case '>': out += "\\u003e"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

std::string host_json(const std::optional<ConnectedHost>& host) {
    if (!host) return "null";
    std::string out = "{\"address\":" + json_string(host->address) + ",\"state\":" +
                      (host->joined ? "\"joined\"" : "\"loading\"") + ",\"players\":[";
    for (size_t i = 0; i < host->players.size(); ++i) {
        const PlayerInfo& p = host->players[i];
        if (i) out += ",";
        out += "{\"name\":" + json_string(p.name) + ",\"steamId\":\"" + std::to_string(p.steam_id) +
               "\",\"isInSteamMode\":" + (individual_account(p.steam_id) ? "true" : "false") +
               ",\"isLocalPlayer\":" + (p.local ? "true" : "false") + "}";
    }
    return out + "]}";
}

std::string dispatch_script(std::string_view target, std::string_view type, std::string_view detail_json) {
    // Guard every level: "nextclient.gameUi" -> window.nextclient&&window.nextclient.gameUi&&...
    std::string guard, path = "window";
    std::string_view rest = target;
    while (!rest.empty()) {
        size_t dot = rest.find('.');
        path += "." + std::string(rest.substr(0, dot));
        guard += path + "&&";
        rest = dot == std::string_view::npos ? std::string_view{} : rest.substr(dot + 1);
    }
    return guard + path + ".dispatchEvent(" + json_string(type) + "," + std::string(detail_json) + ");";
}

const char* api_script() { return kApiScript; }

}  // namespace nc
