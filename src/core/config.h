// nextchromium.ini ([main] enabled, frame_rate, debug_port) plus the -nonextchromium switch.
#pragma once

#include <string>
#include <string_view>

namespace nc {

struct Config {
    bool enabled = true;
    int frame_rate = 60;  // 1..240, CEF windowless_frame_rate
    int debug_port = 0;   // 0 = remote debugging off
};

Config parse_config(std::string_view ini_text, std::wstring_view cmdline);
Config load_config(const std::wstring& ini_path, std::wstring_view cmdline);

}  // namespace nc
