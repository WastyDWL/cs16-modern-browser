#include "core/api_router.h"
#include "testing.h"

using namespace nc;

namespace {

struct FakeGame : GameApi {
    std::vector<std::string> cmds;
    std::optional<ConnectedHost> h;
    bool shown = false;
    std::optional<std::string> cvar(const std::string& n) override {
        if (n == "name") return std::string("Жека \"K\"");
        return std::nullopt;
    }
    void client_cmd(const std::string& c) override { cmds.push_back(c); }
    std::optional<ConnectedHost> host() override { return h; }
    bool gameui_shown() override { return shown; }
};

Domains trusted() {
    Domains d;
    d.load_from_string("epicentre-cs.ru");
    return d;
}

}  // namespace

TEST(router_exec_untrusted_denied) {
    FakeGame g;
    ApiResult r = route_api({"exec", "quit"}, "https://evil.example/", trusted(), g);
    CHECK_EQ(r.status, 403);
    CHECK(g.cmds.empty());
}

TEST(router_exec_trusted_runs_command) {
    FakeGame g;
    ApiResult r = route_api({"exec", "say hi"}, "https://epicentre-cs.ru/statsx/", trusted(), g);
    CHECK_EQ(r.status, 200);
    CHECK_EQ(g.cmds.size(), (size_t)1);
    CHECK_EQ(g.cmds[0], std::string("say hi"));
}

TEST(router_cvar_values) {
    FakeGame g;
    CHECK_EQ(route_api({"getCvarValue", "name"}, "", trusted(), g).json, std::string("\"Жека \\\"K\\\"\""));
    CHECK_EQ(route_api({"getCvarValue", "nope"}, "", trusted(), g).json, std::string("null"));
}

TEST(router_connected_host_and_gameui) {
    FakeGame g;
    CHECK_EQ(route_api({"connectedHost", ""}, "", trusted(), g).json, std::string("null"));
    g.h = ConnectedHost{"1.2.3.4:27015", false, {}};
    CHECK_EQ(route_api({"connectedHost", ""}, "", trusted(), g).json,
             std::string("{\"address\":\"1.2.3.4:27015\",\"state\":\"loading\",\"players\":[]}"));
    g.shown = true;
    CHECK_EQ(route_api({"gameUiShown", ""}, "", trusted(), g).json, std::string("true"));
}

TEST(router_options_dialog_needs_trust) {
    FakeGame g;
    CHECK_EQ(route_api({"openOptionsDialog", "video"}, "https://evil.example/", trusted(), g).status, 403);
    CHECK_EQ(route_api({"openOptionsDialog", "video"}, "https://epicentre-cs.ru/", trusted(), g).status, 501);
}

TEST(router_unknown_and_unavailable) {
    FakeGame g;
    CHECK_EQ(route_api({"format_c", ""}, "", trusted(), g).status, 404);
    CHECK_EQ(route_api({"getServerInfo", "1.2.3.4:27015"}, "", trusted(), g).status, 501);
}
