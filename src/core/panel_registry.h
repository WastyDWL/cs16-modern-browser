// Which game panels may get a browser lazily (on their first serializer call). Panels created
// before nextchromium loaded never call CreateBrowser again, so lazy creation is needed for
// them; a panel the game already removed must not get a new browser (its target is being freed).
#pragma once

#include <set>

namespace nc {

class PanelRegistry {
public:
    bool lazy_allowed(const void* key) const { return removed_.count(key) == 0; }
    void removed(const void* key) { removed_.insert(key); }
    // CreateBrowser for this key: a new panel, possibly at a reused address.
    void created(const void* key) { removed_.erase(key); }

private:
    std::set<const void*> removed_;
};

}  // namespace nc
