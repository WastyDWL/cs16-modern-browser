// Reads the real size of the game's vgui2::HTML panel. The panel only sends BrowserSize when its
// size changes, and panels created before nextchromium loaded already sent theirs to CEF1.
#pragma once

#include <cstdint>

#include "common/valve_html.h"

namespace nc {

// VPANEL of the vgui2::Panel that owns this IHTMLResponses_HL1, found through MSVC RTTI;
// 0 if the object is not a vgui2::Panel (e.g. the harness's fake panel).
uint32_t vgui_vpanel_of(IHTMLResponses_HL1* target);
// Panel size via vgui2.dll's VGUI_Panel007; false outside the game or for non-panels.
bool vgui_panel_size(IHTMLResponses_HL1* target, int& wide, int& tall);

}  // namespace nc
