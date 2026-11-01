// Minimal test runner: TEST(name) { CHECK(...); CHECK_EQ(a, b); }
#pragma once

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace nctest {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

template <class T>
std::string show(const T& v) {
    std::ostringstream os;
    if constexpr (requires { os << v; }) os << v;
    else os << "<?>";
    return os.str();
}

inline void fail(const char* file, int line, const std::string& msg) {
    ++failures();
    std::printf("    %s:%d: %s\n", file, line, msg.c_str());
}

}  // namespace nctest

#define NC_CAT2(a, b) a##b
#define NC_CAT(a, b) NC_CAT2(a, b)
#define TEST(name)                                                                  \
    static void name();                                                             \
    static nctest::Registrar NC_CAT(reg_, name)(#name, &name);                       \
    static void name()

#undef CHECK  // CEF headers (base/cef_logging.h) define their own
#undef CHECK_EQ
#define CHECK(cond) \
    do { if (!(cond)) nctest::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_EQ(a, b)                                                                              \
    do {                                                                                            \
        auto&& va_ = (a);                                                                           \
        auto&& vb_ = (b);                                                                           \
        if (!(va_ == vb_))                                                                          \
            nctest::fail(__FILE__, __LINE__,                                                        \
                         "CHECK_EQ(" #a ", " #b "): " + nctest::show(va_) + " != " + nctest::show(vb_)); \
    } while (0)
