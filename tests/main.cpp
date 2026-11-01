#include <cstring>

#include "testing.h"

// Usage: nc_tests [substring]  — runs tests whose name contains the substring.
int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : "";
    int run = 0, failed = 0;
    for (auto& c : nctest::registry()) {
        if (!std::strstr(c.name, filter)) continue;
        int before = nctest::failures();
        c.fn();
        ++run;
        bool ok = nctest::failures() == before;
        if (!ok) ++failed;
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", c.name);
    }
    std::printf("%d tests, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
