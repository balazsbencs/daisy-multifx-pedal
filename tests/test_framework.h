#pragma once
#include <cstdio>
#include <cstring>

struct TestSuite {
    int passed = 0;
    int failed = 0;
    const char* section_name = "";

    void section(const char* name) {
        section_name = name;
        std::printf("\n[%s]\n", name);
    }

    void check(const char* label, bool ok) {
        if (ok) {
            std::printf("  PASS  %s\n", label);
            ++passed;
        } else {
            std::printf("  FAIL  %s\n", label);
            ++failed;
        }
    }

    int summary() const {
        std::printf("\n─────────────────────────────────────\n");
        std::printf("Results: %d passed, %d failed\n", passed, failed);
        return failed > 0 ? 1 : 0;
    }
};
