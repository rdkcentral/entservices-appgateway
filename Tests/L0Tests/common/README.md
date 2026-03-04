# Shared L0 test utilities (`tests/l0/common`)

This folder contains reusable, plugin-agnostic helpers for L0 test executables:

- `L0TestTypes.hpp`: `L0Test::TestResult` and result-to-exit utilities.
- `L0Expect.hpp`: simple `Expect*` assertion helpers that print `FAIL:` lines and increment `TestResult.failures`.
- `L0Bootstrap.hpp/.cpp`: minimal Thunder Core bootstrap (WorkerPool start/stop + `Core::Singleton::Dispose()`).
- `L0ServiceMock.hpp`: lightweight base registry helper for service mocks (no gmock).

## Example

```cpp
#include <iostream>

#include "common/L0Bootstrap.hpp"
#include "common/L0Expect.hpp"
#include "common/L0TestTypes.hpp"

int main() {
    // Ensure Thunder Core worker pool exists and is running for the duration of the process.
    L0Test::L0BootstrapGuard bootstrap;

    L0Test::TestResult tr;

    L0Test::ExpectTrue(tr, true, "sanity");
    L0Test::ExpectEqU32(tr, 1, 1, "numbers equal");
    L0Test::ExpectEqStr(tr, std::string("a"), std::string("a"), "strings equal");
    L0Test::ExpectJsonEq(tr, std::string("{\"a\":1}"), std::string("{\"a\":1}"), "json text equal");

    L0Test::PrintTotals(std::cerr, "ExamplePlugin l0test", tr.failures);
    return L0Test::ResultToExitCode(tr);
}
```

## Notes

- `L0BootstrapGuard` mirrors typical patterns in existing tests:
  - If no `Core::IWorkerPool` is available, it creates one and starts it.
  - On shutdown, it stops and detaches the worker pool (only if it owns it).
  - Always calls `WPEFramework::Core::Singleton::Dispose()` at end of process.

- `L0ServiceMock` is a **helper base**, not a full Thunder host implementation.
  Extend your plugin-specific `ServiceMock` and use the registry to keep
  `QueryInterfaceByCallsign` behavior consistent across plugins.
