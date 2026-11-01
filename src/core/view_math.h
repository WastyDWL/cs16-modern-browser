#pragma once

#include "include/internal/cef_types_wrappers.h"

namespace nc {

constexpr int kMaxViewSize = 8192;

// Size the game asked for, clamped to what CEF accepts (1..kMaxViewSize per side).
CefSize clamp_view_size(int w, int h);

}  // namespace nc
