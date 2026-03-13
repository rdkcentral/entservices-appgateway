#pragma once

#include <cstdint>
#include <ostream>
#include <string>

namespace L0Test {

/**
 * @brief Aggregated test result for L0 test executables.
 *
 * This is intentionally tiny and header-only so all plugin L0 tests can use it
 * without pulling additional dependencies.
 */
struct TestResult {
    uint32_t failures{0};

    void Reset()
    {
        failures = 0;
    }
};

// PUBLIC_INTERFACE
inline int ResultToExitCode(const uint32_t failures)
{
    /**
     * Convert a failure count to a process exit code.
     * Convention: 0 => success, non-zero => failure count (clamped by int range by caller).
     */
    return (failures == 0) ? 0 : static_cast<int>(failures);
}

// PUBLIC_INTERFACE
inline int ResultToExitCode(const TestResult& tr)
{
    /** Convenience overload for TestResult. */
    return ResultToExitCode(tr.failures);
}

// PUBLIC_INTERFACE
inline void PrintTotals(std::ostream& out, const char* suiteName, const uint32_t failures)
{
    /** Print a consistent totals line. */
    if (failures == 0) {
        out << (suiteName ? suiteName : "L0Test") << " passed." << std::endl;
    } else {
        out << (suiteName ? suiteName : "L0Test") << " total failures: " << failures << std::endl;
    }
}

} // namespace L0Test
