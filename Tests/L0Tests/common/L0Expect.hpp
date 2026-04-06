#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "L0TestTypes.hpp"

namespace L0Test {

// PUBLIC_INTERFACE
inline void ExpectTrue(TestResult& tr, const bool condition, const std::string& what)
{
    /** Expect a condition to be true. */
    if (!condition) {
        tr.failures++;
        std::cerr << "FAIL: " << what << std::endl;
    }
}

// PUBLIC_INTERFACE
inline void ExpectEqU32(TestResult& tr, const uint32_t actual, const uint32_t expected, const std::string& what)
{
    /** Expect two uint32_t values to be equal. */
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected=" << expected << " actual=" << actual << std::endl;
    }
}

// PUBLIC_INTERFACE
inline void ExpectEqStr(TestResult& tr, const std::string& actual, const std::string& expected, const std::string& what)
{
    /** Expect two strings to be equal. */
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected='" << expected << "' actual='" << actual << "'" << std::endl;
    }
}

// PUBLIC_INTERFACE
inline void ExpectJsonEq(TestResult& tr, const std::string& actualJson, const std::string& expectedJson, const std::string& what)
{
    /**
     * Expect two JSON payload strings to be equal as raw text.
     *
     * NOTE: This is intentionally string-level comparison (no parsing/canonicalization),
     * as requested. If canonical JSON comparison is needed later, add a separate helper
     * without changing this behavior.
     */
    if (actualJson != expectedJson) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expectedJson='" << expectedJson << "' actualJson='" << actualJson << "'" << std::endl;
    }
}

} // namespace L0Test
