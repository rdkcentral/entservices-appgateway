# Testing Infrastructure

> **Test root:** `Tests/`
> **Framework:** Google Test (GTest) + GMock
> **Test levels:** L0 (integration/branch), L1 (unit), L2 (end-to-end)

---

## 1. High-Level Purpose & Architecture

The `entservices-appgateway` repository uses a **multi-level test strategy** aligned with the broader RDK `entservices-*` test infrastructure:

| Level | Location | Scope |
|---|---|---|
| **L0** | `Tests/L0Tests/` | Per-plugin integration/branch tests; run against real or near-real plugin logic |
| **L1** | `Tests/L1Tests/` | Unit tests; all external dependencies mocked; maximum isolation |
| **L2** | `Tests/L2Tests/` | End-to-end tests (triggered separately) |

Mocks are shared across all levels and reside in `Tests/mocks/`.

---

## 2. Repository Test Structure

```
Tests/
├── README.md                          # Test runner instructions (local + CI)
├── .clang-format                      # Clang format config for test files
├── clang.cmake                        # Clang toolchain for test builds
├── gcc-with-coverage.cmake            # GCC + coverage flags
├── mocks/                             # Shared mock headers (all plugins)
├── CopilotFiles/                      # AI-assisted test generation guides
│   ├── copilot-instructions-mock.md   # How to generate mocks with Copilot
│   └── l1_tests.instructions.md      # L1 test generation instructions
├── L0Tests/
│   ├── CMakeLists.txt
│   ├── common/
│   ├── AppGateway/                    # L0 tests for AppGateway
│   ├── AppGatewayCommon/              # L0 tests for AppGatewayCommon
│   ├── AppNotifications/              # L0 tests for AppNotifications
│   └── AppActions/                    # L0 tests for AppActions
├── L1Tests/
│   ├── CMakeLists.txt
│   ├── .lcovrc_l1                     # lcov coverage config
│   ├── cmake/                         # L1-specific CMake helpers
│   ├── tests/                         # Test entry points
│   ├── AppGatewayCommon/              # L1 unit tests for AppGatewayCommon
│   └── AppActions/                    # L1 unit tests for AppActions
└── L2Tests/                           # End-to-end tests
```

---

## 3. Mock Inventory (`Tests/mocks/`)

All mocks are GTest/GMock-based header files. They are shared across L0, L1, and L2 tests.

| Mock File | Mocked Interface / Class | Used By |
|---|---|---|
| `AppGatewayMock.h` | `Exchange::IAppGateway*` interfaces (Resolver, Responder, Authenticator, etc.) | AppGateway L0/L1 tests |
| `AppNotificationHandlerMock.h` | `Exchange::IAppNotificationHandler` | AppGatewayCommon, AppNotifications tests |
| `AppActionsMock.h` | `Exchange::IAppActions`, `Exchange::IAppActions::INotification` | AppActions L1 tests |
| `MockEmitter.h` | `Exchange::IAppNotificationHandler::IEmitter` | AppGatewayCommon event tests |
| `ServiceMock.h` | `PluginHost::IShell` | All plugins |
| `DispatcherMock.h` | `PluginHost::IDispatcher` | AppGateway, AppActions tests |
| `TelemetryMock.h` | `Exchange::IAppGatewayTelemetry` | AppGateway telemetry tests |
| `UserSettingMock.h` | `Exchange::IUserSettings` (Thunder UserSettings plugin) | AppGatewayCommon user settings tests |
| `NetworkManagerMock.h` | `Exchange::INetworkManager` (Thunder NetworkManager plugin) | AppGatewayCommon network tests |
| `LifecycleManagerMock.h` | Lifecycle management interface | AppGatewayCommon lifecycle tests |
| `WindowManagerMock.h` | Window manager interface | AppGatewayCommon display tests |
| `MockTextToSpeech.h` | TTS interface | AppGatewayCommon TTS tests |
| `MockSharedStorage.h` | Shared storage interface | AppGatewayCommon tests |
| `MockJSONRPCDirectLink.h` | `Utils::JSONRPC::LinkType` (direct link) | AppGatewayCommon JSON-RPC tests |
| `MockInterfaceDetailsIterator.h` | Interface iterator mock | AppGateway tests |
| `COMLinkMock.h` | `RPC::IRemoteConnection::INotification` | AppGateway, AppActions tests |
| `CommunicatorMock.h` | RPC communicator mock | AppGateway tests |
| `WorkerPoolImplementation.h` | `Core::IWorkerPool` fake implementation | Async job testing |
| `mockauthservices.h` | Authentication service mock | AppGatewayCommon auth tests |
| `Telemetry.h` | Telemetry integration helper | Telemetry tests |
| `ThunderPortability.h` | Thunder version portability shims | All |
| `Module.h` | Test module boilerplate | All |

---

## 4. L0 Test Coverage

### AppGateway (`Tests/L0Tests/AppGateway/`)

| Test File | Coverage Area |
|---|---|
| `AppGatewayTest.cpp` | Plugin lifecycle (`Initialize` / `Deinitialize`) |
| `AppGateway_Init_DeinitTests.cpp` | Detailed init/deinit edge cases |
| `AppGateway_JsonRpcResolveTests.cpp` | JSON-RPC method resolution paths |
| `AppGatewayImplementation_BranchTests.cpp` | `AppGatewayImplementation::Resolve()` branch coverage |
| `AppGatewayResponderImplementation_Tests.cpp` | Responder `Respond()`, `Emit()`, `Request()` |
| `Responder_BehaviorTests.cpp` | Responder edge cases and registry behavior |
| `Resolver_Configure_And_ResolveTests.cpp` | `Resolver::LoadConfig()`, `ResolveAlias()` |
| `ContextUtils_ConversionTests.cpp` | `ContextUtils` context conversion helpers |
| `AppGatewayTelemetry_Tests.cpp` | Telemetry recording and aggregation |
| `AppGatewayTelemetry_DirectAccess_Tests.cpp` | Telemetry singleton direct-access tests |

**Local headers:**
- `ContextConversionHelpers.h` — test utilities for context conversion
- `ContextUtils.h` — test-local context utility overrides
- `ServiceMock.h` — local `IShell` mock

### AppGatewayCommon (`Tests/L0Tests/AppGatewayCommon/`)

| Test File | Coverage Area |
|---|---|
| `AppGatewayCommon_common_test.h` | Shared test fixture header |
| `AppGatewayCommon_main_test.cpp` | Test runner entry point |
| `AppGatewayCommon_routing_test.cpp` | Handler dispatch table routing |
| `AppGatewayCommon_setters_test.cpp` | Setter methods (setName, setLocale, etc.) |
| `AppGatewayCommon_lifecycle_test.cpp` | Lifecycle 1.0 & 2.0 flows |
| `AppGatewayCommon_events_test.cpp` | `EventRegistrationJob`, `HandleAppEventNotifier()` |

### AppNotifications (`Tests/L0Tests/AppNotifications/`)

*(Directory present in L0Tests; specific test files not listed in workspace discovery data.)*

### AppActions (`Tests/L0Tests/AppActions/`)

*(Directory present in L0Tests; specific test files not listed in workspace discovery data.)*

---

## 5. L1 Test Coverage

### AppGatewayCommon (`Tests/L1Tests/AppGatewayCommon/`)

| Test File | Coverage Area |
|---|---|
| `AppGatewayCommon_core_test.cpp` | Core request dispatch, handler lookup |
| `AppGatewayCommon_system_test.cpp` | Device info APIs (make, name, SKU, chipset, uptime) |
| `AppGatewayCommon_usersettings_test.cpp` | Voice guidance, captions, audio description, speed |
| `AppGatewayCommon_network_test.cpp` | Internet connection status |
| `AppGatewayCommon_lifecycle_test.cpp` | Lifecycle 1.0 & 2.0 flows |
| `AppGatewayCommon_events_test.cpp` | Event registration, `EventRegistrationJob` drain |
| `AppGatewayCommon_appdelegate_test.cpp` | Intent dispatch, actions |

### AppActions (`Tests/L1Tests/AppActions/`)

| Test File | Coverage Area |
|---|---|
| `AppActions_test.cpp` | `ActionStart()` dispatch, notification register/unregister |

---

## 6. CI / GitHub Workflow Integration

From `Tests/README.md`, the CI pipeline performs three steps per pull request:

```
a) Build mocks      → compiles TestMock lib from all mock headers → install/usr/lib
b) Build plugin .so → compiles test lib (.so) from all applicable test files
c) Build testframework → links plugin .so files into L1/L2 executable and runs tests
```

Tests are triggered by `.github/workflows/tests-trigger.yml`. To run locally using `act`:

```bash
# 1. Install act
curl -SL https://raw.githubusercontent.com/nektos/act/master/install.sh | bash

# 2. Run all tests
./bin/act -W .github/workflows/tests-trigger.yml -s GITHUB_TOKEN=<your token>
```

### Build Flags for Tests

| Flag | Effect |
|---|---|
| `RDK_SERVICES_L1_TEST=ON` | Includes `Tests/L1Tests/` in the CMake build |
| `RDK_SERVICE_L2_TEST=ON` | Includes `Tests/L2Tests/` in the CMake build |

### Coverage

L1 tests use lcov for coverage reporting:
- Configuration: `Tests/L1Tests/.lcovrc_l1`
- Coverage build toolchain: `Tests/gcc-with-coverage.cmake`

---

## 7. L1 Test Design Guidelines

From `Tests/CopilotFiles/l1_tests.instructions.md`:

### Key Principles

1. **Scenario testing** — normal operation, boundary conditions, and error cases must all be covered.
2. **Isolation** — use mocks for all external dependencies.
3. **Event verification** — correct event dispatch and notification flow must be validated.
4. **No invented error codes** — use only `Core::ERROR_NONE`, `Core::ERROR_GENERAL`, `Core::ERROR_INVALID_PARAMETER`, `Core::ERROR_UNAVAILABLE`, `Core::ERROR_ILLEGAL_STATE`.
5. **No exception-only assertions** — avoid bare `EXPECT_NO_THROW`; always verify actual outcomes.
6. **Mock setup** — use `EXPECT_CALL` with `::testing::_` unless a specific value is required.

### Test Structure Pattern

```cpp
// Typical L1 test fixture pattern (from actual test files)
class AppGatewayCommonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize mocks, create plugin instance
    }
    void TearDown() override {
        // Release interfaces, verify mock expectations
    }
    // Mock members
    NiceMock<ServiceMock> mServiceMock;
    // Plugin under test
    std::unique_ptr<AppGatewayCommon> mPlugin;
};

TEST_F(AppGatewayCommonTest, GetDeviceName_ReturnsExpectedValue) {
    // Arrange
    EXPECT_CALL(/* system mock */, GetFriendlyName(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>("TestDevice"),
            ::testing::Return(Core::ERROR_NONE)));
    // Act
    string result;
    auto rc = mPlugin->HandleAppGatewayRequest(ctx, "device.name", "{}", result);
    // Assert
    EXPECT_EQ(rc, Core::ERROR_NONE);
    EXPECT_FALSE(result.empty());
}
```

---

## 8. Missing Coverage & Improvement Suggestions

### AppGateway

| Missing Area | Suggestion |
|---|---|
| `Resolver::LoadConfig()` with malformed JSON | Add L1 test with corrupted / empty JSON files |
| `Resolver::LoadConfig()` with overlay files | Test that a second `LoadConfig()` call merges (not replaces) entries |
| `AppGatewayImplementation::Resolve()` full path | Add L1 test using mocked `Resolver` + mocked `IAppGatewayRequestHandler` |
| `AppGatewayTelemetry` flush/threshold logic | Add unit test triggering forced flush at cache threshold |
| `CompliantJsonRpcRegistry` RPCV2 detection | Add test verifying JSON-RPC 2.0 formatting is applied to compliant connections |
| WebSocket round-trip integration | Add an integration test for a full `Request()` → `Resolve()` → `Respond()` cycle |

### AppGatewayCommon

| Missing Area | Suggestion |
|---|---|
| `CheckPermissionGroup()` negative path | Add test for unauthorized appId |
| `Authenticate()` with invalid/expired token | Add negative-path test |
| `Deinitialize()` drain wait | Add concurrency stress test: submit many `EventRegistrationJob`s, then call `Deinitialize()` |
| `TTSDelegate` methods | No unit tests found; add coverage for TTS set/get operations |
| All `GetDisplay*` methods | Verify `SystemDelegate` is correctly wired for display info APIs |

### AppNotifications

| Missing Area | Suggestion |
|---|---|
| `SubscriberMap::Add()` / `Remove()` | Add unit tests with multiple events and contexts |
| `SubscriberMap::EventUpdate()` | Test that all subscribed contexts receive the event |
| `SubscriberMap::CleanupNotifications()` | Test partial cleanup (only some connections disconnecting) |
| Last-subscriber unsubscribe | Verify `ThunderSubscriptionManager::Unsubscribe()` is called when last subscriber removes |
| Concurrent subscribe/unsubscribe | Add thread-safety stress test for simultaneous operations on the same key |
| `Cleanup()` → unsubscribe from Thunder | Verify end-to-end cleanup path |

### AppActions

| Missing Area | Suggestion |
|---|---|
| Multiple notification listeners | Test that all registered listeners receive `OnActionStartRequest` |
| `Unregister()` during dispatch | Test thread safety when a listener unregisters itself inside the callback |
| `Deactivated()` remote process death | Add test simulating out-of-process crash |
| JSON-RPC event broadcast | Verify `JAppActions::Event::OnActionStartRequest()` is called via mock dispatcher |

---

## 9. Beginner-to-Expert Learning Path for Testing

### Must-Know First

1. Understand GTest/GMock fundamentals — `TEST_F`, `EXPECT_CALL`, `EXPECT_EQ`, matchers.
2. Read `Tests/mocks/WorkerPoolImplementation.h` — the fake worker pool is essential for testing async `Core::IDispatch` jobs synchronously.
3. Read an existing test file (e.g., `Tests/L1Tests/AppGatewayCommon/AppGatewayCommon_core_test.cpp`) to understand the fixture pattern.

### Intermediate

4. Learn how `Core::Sink<T>` works with mocks — some interfaces need to be `Core::Sink`-wrapped before they can be passed to `Register()`.
5. Understand the mock hierarchy — `ServiceMock` wraps `PluginHost::IShell`; most plugin tests need it to stub out `QueryInterface` calls.
6. Read `Tests/CopilotFiles/l1_tests.instructions.md` for the full AI-assisted test generation guide.

### Advanced

7. **Writing a new L1 test for AppGateway:**
   - Include `Tests/mocks/AppGatewayMock.h`, `Tests/mocks/ServiceMock.h`.
   - Use `WorkerPoolImplementation` to synchronously drain async jobs.
   - Mock `Resolver` responses; verify `IAppGatewayResponder::Respond()` is called with the expected payload.
8. **Coverage-guided testing** — run with `gcc-with-coverage.cmake` and `lcov`; inspect the coverage report to find untested branches in `AppGatewayImplementation::Resolve()` and `SubscriberMap::EventUpdate()`.

---

*Back to [README.md](../README.md)*
