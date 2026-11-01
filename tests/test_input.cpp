#include "core/keymap.h"
#include "core/view_math.h"
#include "testing.h"

using namespace nc;

TEST(keyevent_char_ascii) {
    CefKeyEvent e = make_key_event(KeyKind::Char, 'a', 0);
    CHECK_EQ(e.type, KEYEVENT_CHAR);
    CHECK_EQ(e.character, (char16_t)L'a');
    CHECK_EQ(e.windows_key_code, (int)'a');
}

TEST(keyevent_cyrillic_char) {
    CefKeyEvent e = make_key_event(KeyKind::Char, 0x0436, 0);
    CHECK_EQ(e.type, KEYEVENT_CHAR);
    CHECK_EQ((int)e.character, 0x0436);
    CHECK_EQ((int)e.unmodified_character, 0x0436);
}

TEST(keyevent_modifiers) {
    CefKeyEvent e = make_key_event(KeyKind::Down, 0x41, 2 | 4);
    CHECK_EQ(e.type, KEYEVENT_RAWKEYDOWN);
    CHECK_EQ(e.windows_key_code, 0x41);
    CHECK((e.modifiers & EVENTFLAG_CONTROL_DOWN) != 0);
    CHECK((e.modifiers & EVENTFLAG_SHIFT_DOWN) != 0);
    CHECK((e.modifiers & EVENTFLAG_ALT_DOWN) == 0);
}

TEST(keyevent_up) {
    CefKeyEvent e = make_key_event(KeyKind::Up, 0x0D, 1);
    CHECK_EQ(e.type, KEYEVENT_KEYUP);
    CHECK((e.modifiers & EVENTFLAG_ALT_DOWN) != 0);
}

TEST(mouse_button_codes) {
    CHECK_EQ(mouse_button(0), MBT_LEFT);
    CHECK_EQ(mouse_button(1), MBT_MIDDLE);
    CHECK_EQ(mouse_button(2), MBT_RIGHT);
    CHECK_EQ(mouse_button(77), MBT_LEFT);
}

TEST(mouse_flags_buttons_and_mods) {
    uint32_t f = mouse_flags(4, true, false, true);
    CHECK((f & EVENTFLAG_LEFT_MOUSE_BUTTON) != 0);
    CHECK((f & EVENTFLAG_RIGHT_MOUSE_BUTTON) != 0);
    CHECK((f & EVENTFLAG_MIDDLE_MOUSE_BUTTON) == 0);
    CHECK((f & EVENTFLAG_SHIFT_DOWN) != 0);
}

TEST(view_size_clamped) {
    CefSize a = clamp_view_size(0, -5);
    CHECK_EQ(a.width, 1);
    CHECK_EQ(a.height, 1);
    CefSize b = clamp_view_size(10000, 20);
    CHECK_EQ(b.width, 8192);
    CHECK_EQ(b.height, 20);
}

TEST(cursor_mapping) {
    CHECK_EQ(vgui_cursor(CT_POINTER), 2);    // dc_arrow
    CHECK_EQ(vgui_cursor(CT_IBEAM), 3);      // dc_ibeam
    CHECK_EQ(vgui_cursor(CT_HAND), 14);      // dc_hand
    CHECK_EQ(vgui_cursor(CT_WAIT), 4);       // dc_hourglass
    CHECK_EQ(vgui_cursor(CT_EASTWESTRESIZE), 10);
    CHECK_EQ(vgui_cursor(CT_NORTHSOUTHRESIZE), 11);
    CHECK_EQ(vgui_cursor(CT_NOTALLOWED), 13);
    CHECK_EQ(vgui_cursor(CT_NONE), 15);      // dc_blank
    CHECK_EQ(vgui_cursor(CT_COPY), 2);       // anything unmapped -> arrow
}
