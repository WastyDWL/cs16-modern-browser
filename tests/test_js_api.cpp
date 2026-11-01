#include "core/js_api.h"
#include "testing.h"

using namespace nc;

TEST(api_url_parsed) {
    auto c = parse_api_url("ncb://api/getCvarValue?a=cl%5Fname%20x");
    CHECK(c.has_value());
    CHECK_EQ(c->method, std::string("getCvarValue"));
    CHECK_EQ(c->arg, std::string("cl_name x"));
}

TEST(api_url_without_arg) {
    auto c = parse_api_url("ncb://api/connectedHost");
    CHECK(c.has_value());
    CHECK_EQ(c->method, std::string("connectedHost"));
    CHECK_EQ(c->arg, std::string(""));
}

TEST(api_url_cyrillic_arg) {
    auto c = parse_api_url("ncb://api/exec?a=say%20%D0%BF%D1%80%D0%B8%D0%B2%D0%B5%D1%82");
    CHECK_EQ(c->arg, std::string("say привет"));
}

TEST(api_url_rejects_other_hosts) {
    CHECK(!parse_api_url("ncb://other/exec?a=quit").has_value());
    CHECK(!parse_api_url("https://api/exec").has_value());
    CHECK(!parse_api_url("ncb://api/").has_value());
}

TEST(json_string_escapes) {
    CHECK_EQ(json_string("a\"b\\c\n</script>"), std::string("\"a\\\"b\\\\c\\n\\u003c/script\\u003e\""));
    CHECK_EQ(json_string("Жека"), std::string("\"Жека\""));
    CHECK_EQ(json_string(std::string("x\x01y", 3)), std::string("\"x\\u0001y\""));
}

TEST(host_json_null_when_disconnected) { CHECK_EQ(host_json(std::nullopt), std::string("null")); }

TEST(host_json_players) {
    ConnectedHost h;
    h.address = "45.95.31.157:27115";
    h.joined = true;
    h.players.push_back({"Жека К", 76561198000000001ull, true});
    h.players.push_back({"bot", 0, false});
    CHECK_EQ(host_json(h),
             std::string("{\"address\":\"45.95.31.157:27115\",\"state\":\"joined\",\"players\":["
                         "{\"name\":\"Жека К\",\"steamId\":\"76561198000000001\",\"isInSteamMode\":true,"
                         "\"isLocalPlayer\":true},"
                         "{\"name\":\"bot\",\"steamId\":\"0\",\"isInSteamMode\":false,\"isLocalPlayer\":false}]}"));
}

TEST(dispatch_script_guards_missing_api) {
    CHECK_EQ(dispatch_script("nextclient.gameUi", "show", "null"),
             std::string("window.nextclient&&window.nextclient.gameUi&&"
                         "window.nextclient.gameUi.dispatchEvent(\"show\",null);"));
}

TEST(api_script_defines_nextclient) {
    std::string s = api_script();
    for (const char* name : {"makeAsEventTarget", "nextclient.exec", "getCvarValue", "connectedHost", "gameUi",
                             "optionsDialog", "console", "matchmaking", "ncb://api/"})
        CHECK(s.find(name) != std::string::npos);
}
