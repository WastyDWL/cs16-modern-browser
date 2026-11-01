#include <windows.h>

#include "include/cef_app.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    CefMainArgs args(instance);
    return CefExecuteProcess(args, nullptr, nullptr);
}
