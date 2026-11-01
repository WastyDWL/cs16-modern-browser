#include "core/vgui_panel.h"
#include "testing.h"

// Same shape as the game's classes: vgui2::Panel's first base is IClientPanel (GetVPanel at
// slot 0) and vgui2::HTML adds IHTMLResponses_HL1 as a second base.
namespace vgui2 {
class IClientPanel {
public:
    virtual unsigned GetVPanel() = 0;
};
class Panel : public IClientPanel {  // `class`, as in the game: RTTI names it .?AVPanel@vgui2@@
public:
    unsigned GetVPanel() override { return vpanel; }
    virtual ~Panel() = default;
    unsigned vpanel = 0;
    int padding[7] = {};
};
}  // namespace vgui2

namespace {

struct NullResponses : IHTMLResponses_HL1 {
    void BrowserReady() override {}
    void BrowserNeedsPaint(int, int, int, const unsigned char*, int, int, int, int, int, int,
                           const unsigned char*) override {}
    void BrowserStartRequest(const char*, const char*, const char*, bool) override {}
    void BrowserURLChanged(const char*, const char*, bool) override {}
    void BrowserFinishedRequest(const char*, const char*) override {}
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int, int, int, int) override {}
    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserGetZoomResponse(float) override {}
    void BrowserCanGoBackandForward(bool, bool) override {}
    void BrowserJSAlert(const char*) override {}
    void BrowserJSConfirm(const char*) override {}
    void BrowserPopupHTMLWindow(const char*, int, int, int, int) override {}
    void BrowserSetHTMLTitle(const char*) override {}
    void BrowserLoadingResource() override {}
    void BrowserStatusText(const char*) override {}
    void BrowserSetCursor(int) override {}
    void BrowserFileLoadDialog() override {}
    void BrowserShowToolTip(const char*) override {}
    void BrowserUpdateToolTip(const char*) override {}
    void BrowserHideToolTip() override {}
    void BrowserClose() override {}
    void BrowserLinkAtPositionResponse(const char*, int, int) override {}
};

struct FakeHtmlPanel : vgui2::Panel, NullResponses {};

}  // namespace

TEST(vpanel_of_vgui_html_panel) {
    FakeHtmlPanel panel;
    panel.vpanel = 0x1234;
    IHTMLResponses_HL1* target = &panel;  // second base: not at the start of the object
    CHECK(static_cast<void*>(target) != static_cast<void*>(&panel));
    CHECK_EQ(nc::vgui_vpanel_of(target), 0x1234u);
}

TEST(vpanel_of_non_panel_is_zero) {
    NullResponses r;
    CHECK_EQ(nc::vgui_vpanel_of(&r), 0u);
    CHECK_EQ(nc::vgui_vpanel_of(nullptr), 0u);
}
