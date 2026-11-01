#include "domains.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace nc {

bool Domains::load(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    load_from_string(ss.str());
    return true;
}

void Domains::load_from_string(std::string_view text) {
    std::istringstream in{std::string(text)};
    std::string d;
    while (in >> d) domains_.insert(d);
}

bool Domains::is_accepted(std::string_view url) const {
    // Same expression as NextClient's AcceptedDomains.cpp.
    static const std::regex re(R"(^(?:https?:\/\/)?(?:[^@\/\n]+@)?(?:www\.)?([^:\/\n]+))");
    std::string s(url);
    std::smatch m;
    if (!std::regex_search(s, m, re)) return false;
    return domains_.count(m.str(1)) != 0;
}

}  // namespace nc
