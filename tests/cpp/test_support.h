#pragma once

#include <cmath>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace test_support {
using TestCase = std::pair<const char*, std::function<void()>>;

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                                 ": assertion failed: " + expression);
    }
}

inline bool near(double actual, double expected, double tolerance = 1e-9) {
    return std::fabs(actual - expected) <= tolerance;
}

inline int run(std::initializer_list<TestCase> cases) {
    int failures = 0;
    int total = 0;
    for (const auto& entry : cases) {
        ++total;
        try {
            entry.second();
            std::cout << "[PASS] " << entry.first << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << entry.first << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << entry.first << ": unexpected exception\n";
        }
    }
    std::cout << (total - failures) << "/" << total << " unit cases passed\n";
    return failures == 0 ? 0 : 1;
}
} // namespace test_support

#define CHECK(condition) test_support::check(static_cast<bool>(condition), #condition, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected) CHECK(test_support::near((actual), (expected)))
