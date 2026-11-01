// ncb_harness <mod_dir> <url> <out.bmp> [width height] [settle_ms]
// Drives the Engine the way the game does (fake IHTMLResponses_HL1), without the game:
// loads the page, waits for it to finish, saves the last frame and prints the title.
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "common/log.h"
#include "core/engine.h"

namespace {

struct FakePanel final : IHTMLResponses_HL1 {
    nc::Browser* browser = nullptr;
    bool ready = false, finished = false;
    int paints = 0, w = 0, h = 0;
    std::vector<unsigned char> frame;
    std::string title, url;

    void BrowserReady() override { ready = true; }
    void BrowserNeedsPaint(int tex, int wide, int tall, const unsigned char* bgra, int, int, int, int, int, int,
                           const unsigned char*) override {
        w = wide;
        h = tall;
        frame.assign(bgra, bgra + static_cast<size_t>(wide) * tall * 4);
        ++paints;
        browser->needs_paint_response(tex);  // like vgui2::HTML, from inside the callback
    }
    void BrowserStartRequest(const char* u, const char*, const char*, bool) override {
        std::printf("start request: %s\n", u);
        browser->start_request_response(true);
    }
    void BrowserURLChanged(const char* u, const char*, bool) override { url = u; }
    void BrowserFinishedRequest(const char* u, const char* t) override {
        url = u;
        if (t && *t) title = t;
        finished = true;
    }
    void BrowserShowPopup() override {}
    void BrowserHidePopup() override {}
    void BrowserSizePopup(int, int, int, int) override {}
    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float) override {}
    void BrowserGetZoomResponse(float) override {}
    void BrowserCanGoBackandForward(bool, bool) override {}
    void BrowserJSAlert(const char* m) override {
        std::printf("alert: %s\n", m);
        browser->js_dialog_response(1);
    }
    void BrowserJSConfirm(const char* m) override {
        std::printf("confirm: %s\n", m);
        browser->js_dialog_response(1);
    }
    void BrowserPopupHTMLWindow(const char* u, int, int, int, int) override { std::printf("popup: %s\n", u); }
    void BrowserSetHTMLTitle(const char* t) override { title = t; }
    void BrowserLoadingResource() override {}
    void BrowserStatusText(const char*) override {}
    void BrowserSetCursor(int) override {}
    void BrowserFileLoadDialog() override {}
    void BrowserShowToolTip(const char*) override {}
    void BrowserUpdateToolTip(const char*) override {}
    void BrowserHideToolTip() override {}
    void BrowserClose() override { std::printf("window.close()\n"); }
    void BrowserLinkAtPositionResponse(const char*, int, int) override {}
};

bool save_bmp(const std::string& path, const std::vector<unsigned char>& bgra, int w, int h) {
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h;  // top-down
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + static_cast<DWORD>(bgra.size());
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<char*>(&fh), sizeof(fh));
    f.write(reinterpret_cast<char*>(&ih), sizeof(ih));
    f.write(reinterpret_cast<const char*>(bgra.data()), bgra.size());
    return f.good();
}

std::wstring widen(const char* s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n ? n - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
    return w;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::printf("usage: ncb_harness <mod_dir> <url> <out.bmp> [width height] [settle_ms]\n");
        return 2;
    }
    std::wstring mod_dir = widen(argv[1]);
    int width = argc > 5 ? atoi(argv[4]) : 1024, height = argc > 5 ? atoi(argv[5]) : 768;
    int settle_ms = argc > 6 ? atoi(argv[6]) : 2000;
    nc::log_open((mod_dir + L"\\harness.log").c_str(), true);

    nc::Engine& engine = nc::Engine::get();
    if (!engine.init({mod_dir, nc::Config{}})) {
        std::printf("engine init failed (see harness.log / cef.log)\n");
        return 1;
    }
    FakePanel panel;
    int key = 0;
    panel.browser = engine.get_or_create(&key, &panel);
    panel.browser->set_size(width, height);
    panel.browser->post_url(argv[2], nullptr);

    using clock = std::chrono::steady_clock;
    auto start = clock::now(), finished_at = clock::time_point{};
    while (clock::now() - start < std::chrono::seconds(45)) {
        engine.run_frame();
        if (panel.finished && finished_at == clock::time_point{}) finished_at = clock::now();
        if (finished_at != clock::time_point{} && clock::now() - finished_at > std::chrono::milliseconds(settle_ms))
            break;
        Sleep(10);
    }
    std::printf("ready=%d finished=%d paints=%d size=%dx%d\nurl=%s\ntitle=%s\n", panel.ready, panel.finished,
                panel.paints, panel.w, panel.h, panel.url.c_str(), panel.title.c_str());
    bool saved = !panel.frame.empty() && save_bmp(argv[3], panel.frame, panel.w, panel.h);
    engine.remove(&key);
    engine.shutdown();
    return saved && panel.finished ? 0 : 1;
}
