#pragma once

namespace nc {

// Opens the log file (shared, so the .asi and nextchromium.dll can both write to it).
// Safe to call once per module; later calls are ignored.
void log_open(const wchar_t* path, bool truncate);
// printf-style line; adds timestamp and thread id, flushes immediately.
void log(const char* fmt, ...);

}  // namespace nc
