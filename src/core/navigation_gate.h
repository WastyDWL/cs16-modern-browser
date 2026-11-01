// Lets the game veto user navigations (IHTMLResponses_HL1::BrowserStartRequest) without
// blocking CEF's UI thread: the navigation is cancelled, the game is asked, and on
// approval the URL is loaded again and passes the gate once.
#pragma once

#include <string>

namespace nc {

class NavigationGate {
public:
    // Returns true to let the navigation proceed, false to cancel it (pending() then holds it).
    bool on_before_browse(const std::string& url, bool user_gesture, bool is_redirect = false,
                          bool main_frame = true);
    // The game's StartRequestResponse; returns the URL to load again, or "" if denied.
    std::string on_response(bool allow);
    void mark_loaded();
    void allow_once(const std::string& url);
    const std::string& pending() const { return pending_; }

private:
    bool loaded_ = false;
    std::string pending_;
    // Navigations let through without asking. Counted rather than matched by URL: Chromium
    // canonicalizes URLs (file:///c:\x becomes file:///C:/x), so a string match would miss.
    int allowed_ = 0;
};

}  // namespace nc
