#include <exception>
#include <iostream>
#include <string>

#include "check.h"

namespace check {

namespace {
int failuresInCurrentTest = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void recordFailure(const char* file, int line, const std::string& message) {
    ++failuresInCurrentTest;
    std::cerr << file << ":" << line << ": " << message << "\n";
}

}  // namespace check

// Runs every test, or only those whose names contain the first argument.
int main(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int run = 0;
    int failed = 0;
    for (const auto& test : check::registry()) {
        if (!filter.empty() && std::string(test.name).find(filter) == std::string::npos) continue;
        ++run;
        check::failuresInCurrentTest = 0;
        try {
            test.run();
        } catch (const std::exception& e) {
            check::recordFailure(test.name, 0, std::string("unexpected exception: ") + e.what());
        }
        if (check::failuresInCurrentTest > 0) {
            ++failed;
            std::cerr << "FAILED: " << test.name << "\n\n";
        }
    }
    std::cout << run - failed << " of " << run << " tests passed\n";
    return failed == 0 && run > 0 ? 0 : 1;
}
