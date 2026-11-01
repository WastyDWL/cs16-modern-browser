// ncb:// — the scheme page JS uses to reach the game (see core/js_api.h). Must be registered
// identically in the browser and the render processes.
#pragma once

#include "include/cef_scheme.h"

namespace nc {

constexpr char kNcbScheme[] = "ncb";
constexpr char kNcbApiHost[] = "api";

inline void register_ncb_scheme(CefRawPtr<CefSchemeRegistrar> registrar) {
    // Secure + CORS so https pages can call it (synchronous XHR and fetch).
    registrar->AddCustomScheme(kNcbScheme, CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
                                               CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_FETCH_ENABLED);
}

}  // namespace nc
