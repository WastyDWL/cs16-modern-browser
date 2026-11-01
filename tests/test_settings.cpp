#include "core/config.h"
#include "core/domains.h"
#include "testing.h"

using namespace nc;

TEST(domains_missing_file_denies_all) {
    Domains d;
    CHECK(!d.load(L"Z:\\definitely\\missing\\accepted_domains.txt"));
    CHECK(!d.is_accepted("https://epicentre-cs.ru/"));
    CHECK(!d.is_accepted(""));
}

TEST(domains_crlf_and_spaces) {
    Domains d;
    d.load_from_string("a.com\r\n  b.ru \n");
    CHECK(d.is_accepted("https://www.a.com/x"));
    CHECK(d.is_accepted("http://b.ru:8080"));
    CHECK(d.is_accepted("a.com"));
    CHECK(!d.is_accepted("https://evil.com/?a.com"));
    CHECK(!d.is_accepted("https://a.com.evil.com/"));
    CHECK(!d.is_accepted("file:///c:/a.com/x.html"));
}

TEST(domains_userinfo) {
    Domains d;
    d.load_from_string("a.com");
    CHECK(d.is_accepted("https://u@a.com/"));
    CHECK(!d.is_accepted("https://a.com@evil.com/"));
}

TEST(config_defaults) {
    Config c = load_config(L"Z:\\missing.ini", L"cstrike.exe -game cstrike");
    CHECK(c.enabled);
    CHECK_EQ(c.frame_rate, 60);
    CHECK_EQ(c.debug_port, 0);
}

TEST(config_cmdline_disables) {
    Config c = load_config(L"Z:\\missing.ini", L"cstrike.exe -game cstrike -nonextchromium");
    CHECK(!c.enabled);
}

TEST(config_ini_values) {
    Config c = parse_config("[main]\r\nenabled=0\nframe_rate=30\ndebug_port=9222\n", L"");
    CHECK(!c.enabled);
    CHECK_EQ(c.frame_rate, 30);
    CHECK_EQ(c.debug_port, 9222);
}

TEST(config_frame_rate_clamped) {
    Config c = parse_config("[main]\nframe_rate=0\n", L"");
    CHECK_EQ(c.frame_rate, 1);
    Config d = parse_config("[main]\nframe_rate=1000\n", L"");
    CHECK_EQ(d.frame_rate, 240);
}
