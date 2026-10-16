#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// A deliberately tiny harness: no dependencies, fixed seeds, and failures that
// print enough context to reproduce. Only the first few failures are printed.
namespace tu {

inline long g_checks = 0, g_failures = 0;

inline void fail(const char* file, int line, const std::string& msg) {
    ++g_failures;
    if (g_failures <= 20) std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, msg.c_str());
}

inline int summary(const char* suite) {
    if (g_failures == 0) {
        std::fprintf(stderr, "PASS %s (%ld checks)\n", suite, g_checks);
        return 0;
    }
    std::fprintf(stderr, "FAIL %s (%ld of %ld checks failed)\n", suite, g_failures, g_checks);
    return 1;
}

inline std::string hex(std::uint64_t v) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "0x%llx", static_cast<unsigned long long>(v));
    return buf;
}

}  // namespace tu

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++tu::g_checks;                                                  \
        if (!(cond)) tu::fail(__FILE__, __LINE__, "CHECK(" #cond ")");   \
    } while (0)

#define CHECK_MSG(cond, msg)                                             \
    do {                                                                 \
        ++tu::g_checks;                                                  \
        if (!(cond)) tu::fail(__FILE__, __LINE__, (msg));                \
    } while (0)
