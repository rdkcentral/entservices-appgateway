#pragma once

#include <core/core.h>

namespace L0Test {

/**
 * @brief Basic L0 bootstrap to ensure Thunder Core worker pool is running.
 *
 * Many Thunder subsystems assume Core::IWorkerPool exists and is running.
 * In production, Thunder host initializes this; in L0 tests we do it per-process.
 */

// PUBLIC_INTERFACE
void L0Init();

// PUBLIC_INTERFACE
void L0Shutdown();

/**
 * @brief RAII bootstrap guard.
 *
 * Construct at the beginning of main() (or before plugin Initialize/Invoke) and
 * destruction will stop worker pool (if owned) and call Core::Singleton::Dispose().
 */
class L0BootstrapGuard final {
public:
    L0BootstrapGuard();
    ~L0BootstrapGuard();

    L0BootstrapGuard(const L0BootstrapGuard&) = delete;
    L0BootstrapGuard& operator=(const L0BootstrapGuard&) = delete;

private:
    bool _initialized;
};

} // namespace L0Test
