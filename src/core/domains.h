// Domains allowed to use privileged nextclient.* calls (platform/accepted_domains.txt),
// matched exactly like NextClient's AcceptedDomains.cpp.
#pragma once

#include <set>
#include <string>
#include <string_view>

namespace nc {

class Domains {
public:
    // Returns false if the file can't be read (the set stays empty: everything is denied).
    bool load(const std::wstring& path);
    // Whitespace-separated domain list.
    void load_from_string(std::string_view text);
    bool is_accepted(std::string_view url) const;

private:
    std::set<std::string> domains_;
};

}  // namespace nc
