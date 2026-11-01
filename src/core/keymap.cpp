#include "keymap.h"

#include <windows.h>

namespace nc {

namespace {

uint32_t key_mods(int vgui_mods) {
    uint32_t m = 0;
    if (vgui_mods & kVguiAlt) m |= EVENTFLAG_ALT_DOWN;
    if (vgui_mods & kVguiCtrl) m |= EVENTFLAG_CONTROL_DOWN;
    if (vgui_mods & kVguiShift) m |= EVENTFLAG_SHIFT_DOWN;
    return m;
}

}  // namespace

CefKeyEvent make_key_event(KeyKind kind, int vk_or_char, int vgui_mods) {
    CefKeyEvent e;
    e.modifiers = key_mods(vgui_mods);
    e.windows_key_code = vk_or_char;
    if (kind == KeyKind::Char) {
        e.type = KEYEVENT_CHAR;
        e.character = static_cast<char16_t>(vk_or_char);
        e.unmodified_character = e.character;
        return e;
    }
    e.type = kind == KeyKind::Down ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
    // lParam-style scan code, as Chromium's Windows key handling expects.
    UINT scan = MapVirtualKeyW(static_cast<UINT>(vk_or_char), MAPVK_VK_TO_VSC);
    e.native_key_code = static_cast<int>((scan << 16) | 1u | (kind == KeyKind::Up ? (3u << 30) : 0u));
    return e;
}

cef_mouse_button_type_t mouse_button(int code) {
    switch (code) {
        case 1: return MBT_MIDDLE;
        case 2: return MBT_RIGHT;
        default: return MBT_LEFT;
    }
}

uint32_t mouse_flags(int vgui_mods, bool left, bool middle, bool right) {
    uint32_t f = key_mods(vgui_mods);
    if (left) f |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (middle) f |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    if (right) f |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    return f;
}

int vgui_cursor(cef_cursor_type_t type) {
    // vgui2::CursorCode values
    enum { dc_arrow = 2, dc_ibeam, dc_hourglass, dc_waitarrow, dc_crosshair, dc_up, dc_sizenwse, dc_sizenesw,
           dc_sizewe, dc_sizens, dc_sizeall, dc_no, dc_hand, dc_blank };
    switch (type) {
        case CT_IBEAM: case CT_VERTICALTEXT: return dc_ibeam;
        case CT_HAND: return dc_hand;
        case CT_WAIT: return dc_hourglass;
        case CT_PROGRESS: return dc_waitarrow;
        case CT_CROSS: case CT_CELL: return dc_crosshair;
        case CT_EASTWESTRESIZE: case CT_EASTRESIZE: case CT_WESTRESIZE: case CT_COLUMNRESIZE: return dc_sizewe;
        case CT_NORTHSOUTHRESIZE: case CT_NORTHRESIZE: case CT_SOUTHRESIZE: case CT_ROWRESIZE: return dc_sizens;
        case CT_NORTHWESTSOUTHEASTRESIZE: case CT_NORTHWESTRESIZE: case CT_SOUTHEASTRESIZE: return dc_sizenwse;
        case CT_NORTHEASTSOUTHWESTRESIZE: case CT_NORTHEASTRESIZE: case CT_SOUTHWESTRESIZE: return dc_sizenesw;
        case CT_MOVE: return dc_sizeall;
        case CT_NOTALLOWED: case CT_NODROP: return dc_no;
        case CT_NONE: return dc_blank;
        default: return dc_arrow;
    }
}

int wheel_delta(int d) {
    // HL1's client.dll sends notches (+-1, a few at most); SDK-built panels send d * 100 / 1.5.
    if (d >= -10 && d <= 10) return d * 120;
    return d * 9 / 5;
}

}  // namespace nc
