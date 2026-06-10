# AppGateway — L2 Test Workflow

## 1. What is an L2 Test?

L2 ("Level 2") tests run the **real plugin code** inside a live Thunder (WPEFramework) process.  
There is no firmware, no hardware, and no device.  External hardware calls are intercepted by  
a thin mock library; everything else — Thunder COM-RPC, JSON-RPC, plugin activation, interface  
dispatch — executes exactly as it would on a real box.

This is different from:

| Level | What runs | What is mocked |
|---|---|---|
| **L0 (component test)** | Individual plugin logic in isolation | Thunder service stubs, external interfaces |
| **L1 (unit test)** | Individual C++ class methods | Thunder, all interfaces |
| **L2 (integration test)** | Full Thunder + real plugins | Hardware HAL only |
| **L3 (system test)** | Real device | Nothing |

### Scope For This Document

Primary focus is **AppGateway plugin L2 behaviour** (`IAppGatewayResolver` path):
- Resolver configuration and method resolution (`Configure` / `Resolve`)
- Event subscribe/unsubscribe path via AppNotifications
- Permission and COM-RPC forwarding paths from AppGateway

`AppGatewayCommon` is documented as a **runtime dependency** needed by AppGateway for
`IAppGatewayRequestHandler` / authenticator flows, plus secondary coverage from the
`AppGatewayResponder_L2Test` fixture that is part of the same suite.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│  Ubuntu 22.04 (GitHub Actions runner / local docker)                │
│                                                                     │
│  ┌────────────────────────────────────────────────────────────┐     │
│  │  WPEFramework (Thunder R4.4.1)  — port 9998                │     │
│  │                                                            │     │
│  │   Controller plugin  ──────────────────────────────────┐  │     │
│  │                                                        │  │     │
│  │   ┌──────────────────┐   COM-RPC /tmp/communicator    │  │     │
│  │   │  AppGateway       │◄──────────────────────────────┤  │     │
│  │   │  (IAppGateway     │                               │  │     │
│  │   │   Resolver)       │──► AppGatewayCommon           │  │     │
│  │   └──────────────────┘     (IAppGatewayResponder      │  │     │
│  │                              IAppGatewayRequestHandler │  │     │
│  │   ┌──────────────────┐       IAppGatewayAuthenticator) │  │     │
│  │   │  AppNotifications │◄──────────────────────────────┘  │     │
│  │   │  (IAppNotifications│                                  │     │
│  │   │   plugin)          │                                  │     │
│  │   └──────────────────┘                                   │     │
│  │                                                           │     │
│  │   ┌──────────────────────────────────────┐               │     │
│  │   │  L2TestsIN plugin  (in-process .so)  │               │     │
│  │   │  • Hosts GTest runner                │               │     │
│  │   │  • AppGateway_L2Test fixture         │               │     │
│  │   │  • AppGatewayResponder_L2Test fixture│               │     │
│  │   └──────────────────────────────────────┘               │     │
│  └────────────────────────────────────────────────────────────┘     │
│                                                                     │
│  RdkServicesL2Test binary  ──JSON-RPC──►  WPEFramework:9998         │
└─────────────────────────────────────────────────────────────────────┘
```

### Key deployment decision — `PLUGIN_APPGATEWAYCOMMON_MODE=Off`

AppGatewayCommon is configured with `mode=Off` (in-process) for L2 tests.  
This means it **loads directly into the WPEFramework process** — no separate WPEProcess is spawned.

| Mode | Process | COM-RPC socket | Used in L2? |
|---|---|---|---|
| `Local` (default) | separate `WPEProcess` | `/tmp/communicator` | No |
| `Off` (in-process) | same as WPEFramework | `/tmp/communicator` | **Yes** |

With `mode=Off` the Thunder COM-RPC proxy still uses `/tmp/communicator` but the stub is  
serviced in-process, eliminating the process-lifecycle race that otherwise causes test  
instability when `WPEProcess` starts/stops independently.

---

## 3. Build Pipeline

```mermaid
flowchart TD
    A[ThunderTools R4.4.3\nCMake code-gen tools] --> B
    B[Thunder R4.4.1\npatched × 6\nport 9998, binding 127.0.0.1] --> C
    C[entservices-apis\nIAppGateway.h stubs\npatched × 2] --> D
    D[googletest v1.15.0\nGTest/GMock for L2 host] --> E
    E[mocks\nTestframework mock libs\nHAL/system shim wrappers] --> F
    F[entservices-deviceanddisplay\nSystemServices / PowerManager\n-DRDK_SERVICE_L2_TEST=ON] --> G
    G[networkmanager\nstage INetworkManager.h\ninto install include path] --> H
    H[entservices-appgateway\n-DPLUGIN_APPGATEWAY=ON\n-DPLUGIN_APPGATEWAYCOMMON=ON\n-DPLUGIN_APPNOTIFICATIONS=ON\n-DPLUGIN_APPGATEWAYCOMMON_MODE=Off\n-DRDK_SERVICE_L2_TEST=ON\nNO -Wl,-z,defs for AppGateway/AppGatewayCommon] --> I
    I[entservices-testframework\nL2TestsIN.so — GTest host\nRdkServicesL2Test binary] --> J[Run tests]
```

### Critical build flags for AppGateway plugins

| Flag | Purpose |
|---|---|
| `-DRDK_SERVICE_L2_TEST=ON` | Enables `TestMocklib` HAL shim; skips `-Wl,-z,defs` |
| `-DPLUGIN_APPGATEWAYCOMMON_MODE=Off` | Forces AppGatewayCommon in-process |
| `-fprofile-arcs -ftest-coverage` | Emits `.gcno` / `.gcda` for lcov |
| `-DDISABLE_SECURITY_TOKEN` | Skips token auth on JSON-RPC calls from tests |
| `-DHIDE_NON_EXTERNAL_SYMBOLS=OFF` | Keeps symbols visible for COM-RPC introspection |

### Why `-Wl,-z,defs` is removed for L2 builds

`-Wl,-z,defs` statically embeds `libgcov.a` into each plugin `.so`.  
This creates an **isolated gcov runtime** inside AppGateway.so / AppGatewayCommon.so  
whose `gcov_cleanup` destructor cannot write `.gcda` files because `__gcov_*` symbols  
conflict with the shared `libgcov` instance in `L2TestsIN.so`.  
**Fix:** `if(NOT RDK_SERVICE_L2_TEST)` guard in both `AppGateway/CMakeLists.txt` and  
`AppGatewayCommon/CMakeLists.txt` removes the flag when building for L2 tests,  
allowing gcov symbol sharing and correct `.gcda` emission.

---

## 4. Runtime Initialisation Flow

```mermaid
sequenceDiagram
    participant Runner as RdkServicesL2Test
    participant Thunder as WPEFramework:9998
    participant Ctrl as Controller plugin
    participant AGW as AppGateway plugin
    participant AGWC as AppGatewayCommon (mode=Off)
    participant AN as AppNotifications plugin
    participant L2 as L2TestsIN plugin

    Runner->>Thunder: starts WPEFramework process
    Thunder->>Ctrl: load Controller plugin
    Thunder->>L2: load L2TestsIN.so (hosts GTest)
    L2->>Runner: signals ready

    Note over Runner,L2: GTest SetUp() in fixture constructor
    Runner->>Ctrl: JSON-RPC activate("org.rdk.AppGatewayCommon")
    Runner->>Ctrl: JSON-RPC activate("org.rdk.AppNotifications") [optional]
    Ctrl->>AGWC: Initialize(IShell)
    AGWC-->>Ctrl: "" (success, starts WS server)

    Runner->>Ctrl: JSON-RPC activate("org.rdk.AppGateway")
    Ctrl->>AGW: Initialize(IShell)
    AGW->>AGWC: QueryInterface(IAppGatewayRequestHandler)
    AGW->>AN: QueryInterface(IAppNotifications) [via callsign lookup]
    AGW-->>Ctrl: "" (success, resolver configured)

    Note over Runner,AGW: Tests execute via COM-RPC or JSON-RPC
```

---

## 5. Test Fixtures In This Suite

### 5.1 `AppGateway_L2Test` — tests the **Resolver** interface

```
AppGateway_L2Test
│
├── Activates: AppGatewayCommon (first) + AppGateway
├── COM-RPC client on /tmp/communicator
│   └── Opens IShell for "org.rdk.AppGateway"
│       └── QueryInterface → IAppGatewayResolver  ← used by TC-RES/EVT/PERM
└── JSON-RPC client on 127.0.0.1:9998
    └── InvokeServiceMethod("org.rdk.AppGateway.1", ...) ← used by TC-CFG/TC-INIT
```

Tests covered: TC-CFG-01..06, TC-CFG-JSONRPC-01/02, TC-RES-03..05,  
TC-COMRPC-04, TC-EVT-01..05, TC-PERM-01, TC-INIT-01/03/06 — **22 tests**

### 5.2 `AppGatewayResponder_L2Test` — secondary fixture for **Responder** interface (AppGatewayCommon)

```
AppGatewayResponder_L2Test
│
├── Activates: AppGatewayCommon (first) + AppGateway
├── COM-RPC client on /tmp/communicator  (AGWR_Engine / AGWR_Client)
│   └── Opens IShell for "org.rdk.AppGateway"
│       └── QueryInterface → IAppGatewayResponder  ← all responder tests
└── Notification handler: AppGatewayResponderNotificationHandler
    └── implements IAppGatewayResponder::INotification
        └── captures OnAppConnectionChanged events
```

Tests covered: TC-NOTIF-01..04, TC-CTX-01..03 (overwrite), TC-RESP-03,  
TC-EMIT-01, TC-REQ-01 — **8 tests**

**Total: 30 tests (29 PASS + 1 SKIP)**

---

## 6. Step-by-Step Test Execution Flow

```mermaid
flowchart TD
    A[RdkServicesL2Test starts] --> B[WPEFramework boots\nLoads Controller + L2TestsIN]
    B --> C{GTest fixture\nSetUp}
    C -->|AppGateway_L2Test| D[Activate AppGatewayCommon\nActivate AppGateway\nOpen COM-RPC IShell\nQueryInterface IAppGatewayResolver]
    C -->|AppGatewayResponder_L2Test| E[Activate AppGatewayCommon\nActivate AppGateway\nOpen COM-RPC IShell\nQueryInterface IAppGatewayResponder]

    D --> F{Test type}
    F -->|TC-CFG-02..06\nTC-INIT-01/03/06\nTC-CFG-JSONRPC| G[JSON-RPC call\nInvokeServiceMethod\nAppGateway.1.configure\nor AppGateway.1.resolve]
    F -->|TC-RES / TC-EVT\nTC-PERM / TC-COMRPC| H[COM-RPC call\nIAppGatewayResolver::Resolve\nor IAppGatewayResolver::Configure]

    E --> I{Test type}
    I -->|TC-NOTIF| J[IAppGatewayResponder::Register\nIAppGatewayResponder::Unregister]
    I -->|TC-CTX| K[IAppGatewayResponder::\nRecordGatewayConnectionContext\nGetGatewayConnectionContext]
    I -->|TC-RESP/EMIT/REQ| L[IAppGatewayResponder::\nRespond / Emit / Request]

    G --> M[Assert return code\nor JSON response]
    H --> M
    J --> M
    K --> M
    L --> M

    M --> N[Fixture TearDown\nRelease interfaces\nDeactivate plugins]
    N --> O{More tests?}
    O -->|yes| C
    O -->|no| P[lcov captures .gcda\ngenhtml → coverage report\nValgrind → memory report\nJSON results uploaded]
```

---

## 7. AppGateway-Focused Dependency Strategy

### 7.1 What AppGatewayCommon Implements (No Mocking Required)

AppGatewayCommon itself **implements** three interfaces directly:

| Interface | Role | Consumed by |
|---|---|---|
| `IAppGatewayRequestHandler` | Handles COM-RPC method dispatch from AppGateway | AppGateway plugin |
| `IAppNotificationHandler` | Receives event subscription requests | AppGateway plugin |
| `IAppGatewayAuthenticator` | Authenticates WebSocket clients by sessionId | AppGatewayCommon internally |

Because AppGatewayCommon **is** these interfaces in the live Thunder stack, no mock is needed  
for them — the real implementation runs.

### 7.2 What IS Needed But Cannot Be Exercised Without a Real Environment

The following AppGatewayCommon paths require external services that are not present in CI:

| Dependency | What it does | L2 behaviour |
|---|---|---|
| Live WebSocket client (app) | Connects to AppGatewayCommon WS server | Not present — `Respond`, `Emit`, `Request` return error but **must not crash** |
| Valid `sessionId` token | Required for `IAppGatewayAuthenticator::Authenticate` | Cannot simulate — WS auth path (TC-WS-*) not exercised in current tests |
| `SettingsDelegate` backends | `IUserSettings`, `INetworkManager`, cloud stores | Not present in CI — `HandleAppGatewayRequest` for those paths returns early |

### 7.3 What AppGateway (Resolver) Needs From AppGatewayCommon

When AppGateway resolves a method with `"useComRpc": true`, it calls  
`QueryInterfaceByCallsign("org.rdk.AppGatewayCommon")` to get `IAppGatewayRequestHandler`.  
In L2 tests this works because AppGatewayCommon is **already activated** (in-process).

```
TC-COMRPC-04: test.comrpc → QueryInterfaceByCallsign → IAppGatewayRequestHandler
              AppGatewayCommon is up but HandleAppGatewayRequest for "test.comrpc"
              method is not mapped → returns error → resolution = "notAvailable"
              ✔ PASS (verifies the not-available branch)
```

### 7.4 Mock Summary for Current 30-Test Suite

| Mock / Substitute | Used | How |
|---|---|---|
| **No hardware mocks needed** | AppGateway / AppGatewayCommon have no HAL calls | — |
| **TestMocklib** (`-DRDK_SERVICE_L2_TEST`) | Other plugins (SystemServices, PersistentStore etc.) | Intercepted by CMake HAL shim |
| **Temp JSON config files** (`mkstemps`) | TC-INIT / TC-CFG resolution config files | Written inline in test fixture, deleted on teardown |
| **Real Thunder Controller** | Plugin activate/deactivate, JSON-RPC forward | Live Thunder instance, no mock |
| **Real AppNotifications plugin (when available)** | TC-EVT-01..05 event subscribe/unsubscribe | Fixture attempts `Controller.1.activate("org.rdk.AppNotifications")`; AppGateway then uses `QueryInterfaceByCallsign` |
| **Real AppGatewayCommon plugin** | AppGateway dependency path (TC-COMRPC-04) + responder fixture (TC-NOTIF/CTX/RESP) | Real plugin, mode=Off |

---

## 8. Known Thunder R4.4.1 Constraints

### 8.1 `IStringIterator*` as IN parameter via COM-RPC — Crashes

**Affected:** `IAppGatewayResolver::Configure(IStringIterator* paths)`

Passing a COM-RPC proxy `IStringIterator*` as an IN parameter triggers a crash inside  
Thunder's reverse-proxy cleanup (double-Release on the marshalled stub). This is a  
Thunder R4.4.1 framework bug — not an AppGateway bug.

**Workaround:**
- TC-CFG-01 (`Configure(nullptr)`) is `GTEST_SKIP()`'d — cannot be safely called via COM-RPC.
- TC-CFG-02..06 use **JSON-RPC** (`AppGateway.1.configure` with `params.paths[]`) instead of  
  the COM-RPC `Configure(IStringIterator*)` path.

```
COM-RPC path (broken in R4.4.1):
  Test → RPC::CommunicatorClient::Open<IAppGatewayResolver>
       → Configure(IStringIterator*) → CRASH during proxy cleanup

JSON-RPC workaround (used instead):
  Test → JSONRPC::LinkType → AppGateway.1.configure {"paths": [...]}
       → Thunder dispatches to AppGateway::configure() JSONRPC handler
       → Internally calls Configure(IStringIterator*) from the same process → OK
```

### 8.2 Non-`@opaque` `string&` OUT parameters via COM-RPC — Not Marshalled Back

**Affected:** `IAppGatewayResponder::GetGatewayConnectionContext(connId, key, contextValue /* @out */)`

`contextValue` is a plain `string&` output (not `@opaque`). Thunder R4.4.1's COM-RPC  
serialiser does not reliably copy back plain `string&` out-parameters across the  
proxy/stub boundary when `mode=Off`.

**Workaround:**  
TC-CTX-01, TC-CTX-02, TC-CTX-03 only assert `EXPECT_EQ(result, Core::ERROR_NONE)`.  
They do **not** assert the string value returned.

### 8.3 WPEFramework SIGSEGV on Shutdown

Thunder R4.4.1 crashes with SIGSEGV during Controller's self-deactivation after all  
plugins have been cleanly deactivated. This is a known Thunder framework issue and  
does **not** affect test pass/fail — all assertions complete before teardown.

---

## 9. Coverage Collection

```mermaid
flowchart LR
    A[Plugins built with\n-fprofile-arcs\n-ftest-coverage] --> B[.gcno files\ngenerated at build time]
    B --> C[Tests execute\n.gcda files written on\nplugin unload / process exit]
    C --> D[lcov -c -d build/entservices-appgateway\n→ coverage.info]
    D --> E[lcov -r ... removes\n/usr/include, _deps,\nmocks, test headers\n→ filtered_coverage.info]
    E --> F[genhtml → coverage/\nHTML report uploaded\nas CI artifact]
```

### Why AppGateway and AppGatewayCommon were previously missing from coverage

`-Wl,-z,defs` was present in both `AppGateway/CMakeLists.txt` and  
`AppGatewayCommon/CMakeLists.txt`. This caused `libgcov.a` to be **statically embedded**  
inside each plugin `.so`, creating isolated gcov runtimes. When L2TestsIN.so was  
`dlclose()`'d, only the shared `libgcov` (from `AppNotifications.so`, which had no  
`-z,defs`) wrote its `.gcda` files. AppGateway and AppGatewayCommon had no mechanism  
to flush their isolated gcov data.

**Fix applied (not yet pushed):**
```cmake
# AppGateway/CMakeLists.txt  and  AppGatewayCommon/CMakeLists.txt
# To error any missing links (skip for L2 test builds: -Wl,-z,defs embeds
# libgcov.a statically, preventing __gcov_* symbol sharing and breaking
# coverage data collection)
if(NOT RDK_SERVICE_L2_TEST)
    target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,defs)
endif()
```

---

## 10. CI Pipeline Phases

| Phase | Step | Output |
|---|---|---|
| **Build** | ThunderTools → Thunder → apis → mocks → device-display → appgateway → testframework | Installed under `$GITHUB_WORKSPACE/install/usr/` |
| **Setup** | Create `/etc/app-gateway/`, device nodes, temp dirs | Filesystem ready for plugin config |
| **Run (plain)** | `RdkServicesL2Test` — 30 tests, ~18 s | `rdkL2TestResultsWithoutValgrind.json` |
| **Run (valgrind)** | Same binary under `valgrind --leak-check=yes` | `valgrind_log`, `rdkL2TestResultsWithValgrind.json` |
| **Coverage** | `lcov -c -d build/entservices-appgateway` → filter → `genhtml` | `coverage/index.html` |
| **Upload** | `actions/upload-artifact@v4` — `artifacts-L2-infra` | Downloadable from GitHub Actions run |

### Current CI result (Run 6, 5 Jun 2026)

```
[==========] 30 tests from 2 test suites ran. (18128 ms total)
[  PASSED  ] 29 tests.
[  SKIPPED ] 1 test.  (AppGateway_L2Test.Configure_NullIterator_COMRPC)
[  FAILED  ] 0 tests.
```

---

## 11. File Structure

```
entservices-appgateway/
├── AppGateway/
│   ├── CMakeLists.txt              ← -Wl,-z,defs guarded by if(NOT RDK_SERVICE_L2_TEST)
│   ├── AppGatewayImplementation.cpp
│   └── ...
├── AppGatewayCommon/
│   ├── CMakeLists.txt              ← same -Wl,-z,defs guard
│   ├── AppGatewayCommon.cpp        ← implements IAppGatewayRequestHandler,
│   │                                  IAppNotificationHandler, IAppGatewayAuthenticator
│   └── ...
├── .github/workflows/
│   └── L2-tests.yml                ← PLUGIN_APPGATEWAYCOMMON_MODE=Off; gcc with-coverage only
└── Tests/L2Tests/
    ├── docs/
    │   ├── AppGateway_L2_TestPlan.md     ← per-test-case plan
    │   └── AppGateway_L2_TestWorkflow.md ← this file
    └── tests/
        └── AppGateway_L2Test.cpp   ← AppGateway_L2Test + AppGatewayResponder_L2Test
```
