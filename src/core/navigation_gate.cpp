#include "navigation_gate.h"

namespace nc {

bool NavigationGate::on_before_browse(const std::string& url, bool user_gesture, bool is_redirect, bool main_frame) {
    if (!main_frame || is_redirect) return true;
    if (allowed_ > 0) {  // our own PostURL or a navigation the game already approved
        --allowed_;
        return true;
    }
    if (!loaded_ || !user_gesture) return true;
    pending_ = url;
    return false;
}

std::string NavigationGate::on_response(bool allow) {
    std::string url;
    url.swap(pending_);
    if (!allow || url.empty()) return {};
    allow_once(url);
    return url;
}

void NavigationGate::mark_loaded() { loaded_ = true; }

void NavigationGate::allow_once(const std::string&) { ++allowed_; }

}  // namespace nc
