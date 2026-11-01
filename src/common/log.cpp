#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace nc {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
}  // namespace

void log_open(const wchar_t* path, bool truncate) {
    std::lock_guard lock(g_mutex);
    if (g_file) return;
    if (truncate)
        if (FILE* f = _wfsopen(path, L"w", _SH_DENYNO)) std::fclose(f);
    // Append mode: the .asi and nextchromium.dll write to the same file through separate handles.
    g_file = _wfsopen(path, L"a", _SH_DENYNO);
}

void log(const char* fmt, ...) {
    std::lock_guard lock(g_mutex);
    if (!g_file) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::fprintf(g_file, "%02d:%02d:%02d.%03d [%5lu] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                 GetCurrentThreadId());
    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_file, fmt, args);
    va_end(args);
    std::fputc('\n', g_file);
    std::fflush(g_file);
}

}  // namespace nc
