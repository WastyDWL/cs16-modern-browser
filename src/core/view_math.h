#pragma once

#include "include/internal/cef_types_wrappers.h"

namespace nc {

constexpr int kMaxViewSize = 4096;  // 64 MB per BGRA frame; covers a 4K screen

// Size the game asked for, clamped to what CEF accepts (1..kMaxViewSize per side).
CefSize clamp_view_size(int w, int h);

}  // namespace nc
