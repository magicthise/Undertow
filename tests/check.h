#pragma once

// A minimal test harness, so the tests build anywhere without downloading a
// framework. TEST_CASE registers a test; CHECK and CHECK_EQ record failures
// and carry on; CHECK_THROWS expects an exception of a given type.

#include <sstream>
#include <string>
#include <vector>

namespace check {

struct TestCase {
    const char* name;
    void (*run)();
};

std::vector<TestCase>& registry();
void recordFailure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

}  // namespace check

#define CHECK_JOIN_(a, b) a##b
#define CHECK_JOIN(a, b) CHECK_JOIN_(a, b)

#define TEST_CASE(name)                                                                                      \
    static void CHECK_JOIN(test_body_, __LINE__)();                                                          \
    static const check::Registrar CHECK_JOIN(test_registrar_, __LINE__)(name, &CHECK_JOIN(test_body_, __LINE__)); \
    static void CHECK_JOIN(test_body_, __LINE__)()

#define CHECK(expr)                                                                       \
    do {                                                                                  \
        if (!(expr)) check::recordFailure(__FILE__, __LINE__, "CHECK(" #expr ") failed"); \
    } while (0)

// Copies both values, so expressions that reach into temporaries (such as
// *parseNote("c")) stay valid while they're compared.
#define CHECK_EQ(actual, expected)                                                                     \
    do {                                                                                               \
        const auto check_actual_ = (actual);                                                           \
        const auto check_expected_ = (expected);                                                       \
        if (!(check_actual_ == check_expected_)) {                                                     \
            std::ostringstream check_message_;                                                         \
            check_message_ << #actual << "\n      got: " << check_actual_ << "\n expected: " << check_expected_; \
            check::recordFailure(__FILE__, __LINE__, check_message_.str());                            \
        }                                                                                              \
    } while (0)

#define CHECK_THROWS(expr, exception_type)                                                                \
    do {                                                                                                  \
        bool check_threw_ = false;                                                                        \
        try {                                                                                             \
            (void)(expr);                                                                                 \
        } catch (const exception_type&) {                                                                 \
            check_threw_ = true;                                                                          \
        } catch (...) {                                                                                   \
        }                                                                                                 \
        if (!check_threw_) check::recordFailure(__FILE__, __LINE__, #expr " didn't throw " #exception_type); \
    } while (0)
