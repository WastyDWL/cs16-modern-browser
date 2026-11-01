#include "config.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace nc {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

int to_int(std::string_view s, int fallback) {
    int v = fallback;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

bool has_switch(std::wstring_view cmdline, std::wstring_view sw) {
    for (size_t pos = cmdline.find(sw); pos != std::wstring_view::npos; pos = cmdline.find(sw, pos + 1)) {
        bool start = pos == 0 || cmdline[pos - 1] == L' ' || cmdline[pos - 1] == L'"';
        size_t end = pos + sw.size();
        bool stop = end == cmdline.size() || cmdline[end] == L' ' || cmdline[end] == L'"';
        if (start && stop) return true;
    }
    return false;
}

}  // namespace

Config parse_config(std::string_view ini_text, std::wstring_view cmdline) {
    Config c;
    std::string section;
    std::istringstream in{std::string(ini_text)};
    for (std::string raw; std::getline(in, raw);) {
        std::string_view line = trim(raw);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = std::string(line.substr(1, line.size() - 2));
            continue;
        }
        size_t eq = line.find('=');
        if (section != "main" || eq == std::string_view::npos) continue;
        std::string_view key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        if (key == "enabled") c.enabled = to_int(value, 1) != 0;
        else if (key == "frame_rate") c.frame_rate = std::clamp(to_int(value, 60), 1, 240);
        else if (key == "debug_port") c.debug_port = std::clamp(to_int(value, 0), 0, 65535);
    }
    if (has_switch(cmdline, L"-nonextchromium")) c.enabled = false;
    return c;
}

Config load_config(const std::wstring& ini_path, std::wstring_view cmdline) {
    std::ifstream f(ini_path, std::ios::binary);
    std::stringstream ss;
    if (f) ss << f.rdbuf();
    return parse_config(ss.str(), cmdline);
}

}  // namespace nc
