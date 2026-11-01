#include "frame_buffer.h"

#include <algorithm>
#include <cstring>

namespace nc {

void FrameBuffer::store(const void* bgra, int w, int h) {
    std::lock_guard lock(mutex_);
    view_.resize(static_cast<size_t>(w) * h);
    std::memcpy(view_.data(), bgra, view_.size() * 4);
    w_ = w;
    h_ = h;
    dirty_ = true;
}

void FrameBuffer::store_popup(const void* bgra, int w, int h) {
    std::lock_guard lock(mutex_);
    popup_.resize(static_cast<size_t>(w) * h);
    std::memcpy(popup_.data(), bgra, popup_.size() * 4);
    pw_ = w;
    ph_ = h;
    dirty_ = true;
}

void FrameBuffer::set_popup_rect(int x, int y, int w, int h) {
    std::lock_guard lock(mutex_);
    px_ = x;
    py_ = y;
    prw_ = w;
    prh_ = h;
    popup_visible_ = true;
    dirty_ = true;
}

void FrameBuffer::hide_popup() {
    std::lock_guard lock(mutex_);
    popup_visible_ = false;
    popup_.clear();
    pw_ = ph_ = 0;
    dirty_ = true;
}

void FrameBuffer::deliver_if_ready(const DeliverFn& deliver) {
    std::unique_lock lock(mutex_);
    if (!dirty_ || in_flight_ || view_.empty()) return;
    // out_ is a snapshot owned by the main thread, so `deliver` runs without the lock: the game
    // answers NeedsPaintResponse from inside BrowserNeedsPaint, and CEF may paint meanwhile.
    out_ = view_;
    if (popup_visible_ && !popup_.empty()) {
        // Clip the popup (a <select> dropdown) against the view and the popup's own buffer.
        int x0 = std::max(px_, 0), y0 = std::max(py_, 0);
        int x1 = std::min({px_ + prw_, px_ + pw_, w_}), y1 = std::min({py_ + prh_, py_ + ph_, h_});
        for (int y = y0; y < y1; ++y)
            std::memcpy(&out_[static_cast<size_t>(y) * w_ + x0],
                        &popup_[static_cast<size_t>(y - py_) * pw_ + (x0 - px_)], static_cast<size_t>(x1 - x0) * 4);
    }
    dirty_ = false;
    in_flight_ = next_id_++;
    if (next_id_ <= 0) next_id_ = 1;
    const int id = in_flight_, w = w_, h = h_;
    lock.unlock();
    deliver(id, w, h, reinterpret_cast<const unsigned char*>(out_.data()));
}

void FrameBuffer::on_response(int texture_id) {
    std::lock_guard lock(mutex_);
    if (texture_id == in_flight_) in_flight_ = 0;
}

}  // namespace nc
