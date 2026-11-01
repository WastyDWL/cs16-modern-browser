// Translation of VGUI input (as passed to IHTMLSerializer) into CEF events.
#pragma once

#include <cstdint>

#include "include/internal/cef_types_wrappers.h"

namespace nc {

enum class KeyKind { Down, Up, Char };

// VGUI modifier bits used by vgui2::HTML::GetKeyModifiers.
constexpr int kVguiAlt = 1;
constexpr int kVguiCtrl = 2;
constexpr int kVguiShift = 4;

// vk_or_char: a Windows virtual-key code for Down/Up (vgui converts it), a UTF-16 unit for Char.
CefKeyEvent make_key_event(KeyKind kind, int vk_or_char, int vgui_mods);
// IInputEventHTML::EMouseButton: 0 left, 1 middle, 2 right; anything else -> left.
cef_mouse_button_type_t mouse_button(int code);
uint32_t mouse_flags(int vgui_mods, bool left, bool middle, bool right);
// CEF cursor -> vgui2::CursorCode (what IHTMLResponses_HL1::BrowserSetCursor expects).
int vgui_cursor(cef_cursor_type_t type);

}  // namespace nc
