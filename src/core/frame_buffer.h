// Latest rendered frame of one browser (view + composited <select> popup) and the
// NeedsPaint/NeedsPaintResponse handshake with the game: one frame in flight at a time.
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace nc {

class FrameBuffer {
public:
    using DeliverFn = std::function<void(int texture_id, int wide, int tall, const unsigned char* bgra)>;

    // Called from CEF's OnPaint (any thread). Pixels are BGRA, tightly packed.
    void store(const void* bgra, int w, int h);
    void store_popup(const void* bgra, int w, int h);
    void set_popup_rect(int x, int y, int w, int h);
    void hide_popup();

    // Main thread: hands the composited frame to `deliver` if there is a new one and the game
    // acknowledged the previous one. The pointer is valid only during the call. texture_id is
    // always 0, as HL1's chromehtml sends it: vgui2::HTML would draw a non-zero id as an
    // existing surface texture instead of the pixels.
    void deliver_if_ready(const DeliverFn& deliver);
    // Main thread: the game's NeedsPaintResponse(texture_id).
    void on_response(int texture_id);

private:
    std::mutex mutex_;
    std::vector<uint32_t> view_, popup_, out_;
    int w_ = 0, h_ = 0, pw_ = 0, ph_ = 0;
    int px_ = 0, py_ = 0, prw_ = 0, prh_ = 0;
    bool popup_visible_ = false;
    bool dirty_ = false;
    bool in_flight_ = false;  // a frame was handed to the game and not acknowledged yet
};

}  // namespace nc
