#pragma once

namespace nc {

// Opens (truncates) the log file. Safe to call once; later calls are ignored.
void log_open(const wchar_t* path);
// printf-style line; adds timestamp and thread id, flushes immediately.
void log(const char* fmt, ...);

}  // namespace nc
