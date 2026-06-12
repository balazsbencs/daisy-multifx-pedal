#include "test_framework.h"

void run_browse_tests(TestSuite&);
void run_preset_store_tests(TestSuite&);

int main() {
    TestSuite t;
    run_browse_tests(t);
    run_preset_store_tests(t);
    return t.summary();
}
