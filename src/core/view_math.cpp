#include "view_math.h"

#include <algorithm>

namespace nc {

CefSize clamp_view_size(int w, int h) {
    return CefSize(std::clamp(w, 1, kMaxViewSize), std::clamp(h, 1, kMaxViewSize));
}

}  // namespace nc
