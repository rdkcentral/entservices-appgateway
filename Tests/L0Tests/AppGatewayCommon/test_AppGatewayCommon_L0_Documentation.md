# AppGatewayCommon L0 Test Documentation

## Test Design Approach

The AppGatewayCommon L0 tests validate the plugin in an isolated environment with **no real Thunder daemon** and **no backing plugins**. A lightweight `ServiceMock` (see `ServiceMock.h`) implements `IShell` / `ICOMLink` so that `Initialize()` and `Deinitialize()` can execute. All delegate COM-RPC and JSON-RPC links are intentionally unresolvable; the tests exercise:

- Plugin lifecycle (init / deinit / idempotency / robustness)
- Handler-map routing (known methods, unknown methods, case-insensitivity)
- JSON payload validation on setter methods
- Permission / authentication entry points
- Interface map (`QueryInterface`)

### Testing Structure
- Each test follows the **Arrange – Act – Assert (AAA)** pattern.
- The `main()` entry point creates an `L0BootstrapGuard` to mirror the production Thunder start-up sequence.
- `TestResult` / `ExpectTrue` / `ExpectEqual` helpers print `FAIL:` lines and accumulate a failure count.
- Final output: `"AppGatewayCommon l0test passed."` or `"AppGatewayCommon l0test total failures: N"`.

### Reference Implementation
Based on `AppGateway_Init_DeinitTests.cpp` and the shared L0 infrastructure (`L0Bootstrap.hpp`, `L0Expect.hpp`, `L0ServiceMock.hpp`).

## Quick Reference Table

| Test ID      | Summary                                           | Method / Interface Covered                |
|--------------|---------------------------------------------------|-------------------------------------------|
| AGC_L0_001   | Initialize/Deinitialize lifecycle                 | Initialize, Deinitialize                  |
| AGC_L0_002   | Double Initialize idempotency                     | Initialize                                |
| AGC_L0_003   | Double Deinitialize robustness                    | Deinitialize                              |
| AGC_L0_004   | Information() returns empty string                | Information                               |
| AGC_L0_005   | Unknown method returns ERROR_UNKNOWN_KEY          | HandleAppGatewayRequest                   |
| AGC_L0_006   | device.make routed via handler map                | HandleAppGatewayRequest (device.make)     |
| AGC_L0_007   | metrics.* pass-through returns null               | HandleAppGatewayRequest (metrics.*)       |
| AGC_L0_008   | discovery.watched pass-through returns null       | HandleAppGatewayRequest (discovery.watched) |
| AGC_L0_009   | CheckPermissionGroup always allowed               | CheckPermissionGroup                      |
| AGC_L0_010   | Authenticate returns ERROR_GENERAL               | Authenticate                              |
| AGC_L0_011   | GetSessionId returns ERROR_GENERAL                | GetSessionId                              |
| AGC_L0_012   | Setter with invalid JSON → ERROR_BAD_REQUEST      | HandleAppGatewayRequest (setter)          |
| AGC_L0_013   | Setter with valid JSON → ERROR_UNAVAILABLE        | HandleAppGatewayRequest (setter)          |
| AGC_L0_014   | VoiceGuidance speed out-of-range → ERROR_BAD_REQUEST | HandleAppGatewayRequest (setter)       |
| AGC_L0_015   | Method routing is case-insensitive                | HandleAppGatewayRequest                   |
| AGC_L0_016   | Bool setter with string value → ERROR_BAD_REQUEST | HandleAppGatewayRequest (setter)          |
| AGC_L0_017   | lifecycle.ready returns ERROR_NONE               | HandleAppGatewayRequest (lifecycle.ready) |
| AGC_L0_018   | QueryInterface for IAppGatewayRequestHandler      | IPlugin interface map                     |
| AGC_L0_019   | QueryInterface for IAppGatewayAuthenticator       | IPlugin interface map                     |

## Test Descriptions

### AGC_L0_001
- **Description:** Full lifecycle: Initialize → Deinitialize succeeds without crash.
- **Inputs:** Default `ServiceMock` config.
- **Expected Result:** Initialize returns empty string; Deinitialize completes.

### AGC_L0_002
- **Description:** Two successive Initialize calls do not crash or deadlock.
- **Inputs:** Re-invoke Initialize on already-initialized plugin.
- **Expected Result:** Both calls return empty string (or are silently idempotent).

### AGC_L0_003
- **Description:** Deinitialize called twice does not crash.
- **Inputs:** Call Deinitialize after already deinitialized.
- **Expected Result:** No crash or undefined behaviour.

### AGC_L0_004
- **Description:** `Information()` returns an empty string (no custom metadata).
- **Inputs:** None.
- **Expected Result:** Empty string returned.

### AGC_L0_005
- **Description:** An unknown method name (`not.a.real.method`) is rejected.
- **Inputs:** method=`not.a.real.method`, empty context/payload.
- **Expected Result:** `ERROR_UNKNOWN_KEY`.

### AGC_L0_006
- **Description:** `device.make` is found in the handler map and dispatched.
- **Inputs:** method=`device.make`, empty context/payload.
- **Expected Result:** `ERROR_NONE` or `ERROR_UNAVAILABLE` (delegate may not be connected).

### AGC_L0_007
- **Description:** `metrics.*` methods are pass-through and return `"null"` as result.
- **Inputs:** method=`metrics.appLoaded`, empty context/payload.
- **Expected Result:** `ERROR_NONE`, result string is `"null"`.

### AGC_L0_008
- **Description:** `discovery.watched` is pass-through and returns `"null"`.
- **Inputs:** method=`discovery.watched`, empty context/payload.
- **Expected Result:** `ERROR_NONE`, result string is `"null"`.

### AGC_L0_009
- **Description:** Permission check always returns `allowed = true`.
- **Inputs:** appId=`"testApp"`, group=`"someGroup"`.
- **Expected Result:** `ERROR_NONE`, `allowed == true`.

### AGC_L0_010
- **Description:** Authenticate delegates to `LifecycleDelegate::Authenticate` via `InvokeLifecycleDelegate`. Both `mDelegate` and `lifecycleDelegate` are non-null (created during `Initialize`). The test session ID is not in `mAppIdInstanceIdMap` → `GetAppId()` returns empty.
- **Inputs:** sessionId=`"test-session-id"`.
- **Expected Result:** `ERROR_GENERAL`.

### AGC_L0_011
- **Description:** GetSessionId delegates to `LifecycleDelegate::GetSessionId`. The appId is not in the map → `GetAppInstanceId()` returns empty.
- **Inputs:** `appId = "com.test.app"`.
- **Expected Result:** `ERROR_GENERAL`.

### AGC_L0_012
- **Description:** Setter method (`accessibility.voiceGuidance.enabled`) with an invalid JSON payload is rejected.
- **Inputs:** method=setter, payload=`not-valid-json`.
- **Expected Result:** `ERROR_BAD_REQUEST`.

### AGC_L0_013
- **Description:** Setter method with valid JSON payload but no backing plugin → `ERROR_UNAVAILABLE`.
- **Inputs:** method=`accessibility.voiceGuidance.enabled`, payload=`{"value":true}`.
- **Expected Result:** `ERROR_UNAVAILABLE`.

### AGC_L0_014
- **Description:** `accessibility.voiceGuidance.speed` with out-of-range value → `ERROR_BAD_REQUEST`.
- **Inputs:** payload=`{"value":"10"}` (numeric string ≥ double max consideration).
- **Expected Result:** `ERROR_BAD_REQUEST`.

### AGC_L0_015
- **Description:** Method routing is case-insensitive (`Device.Make` resolves the same as `device.make`).
- **Inputs:** method=`Device.Make`.
- **Expected Result:** Same result code as `device.make` (not `ERROR_UNKNOWN_KEY`).

### AGC_L0_016
- **Description:** Bool setter receives a string instead of boolean → type validation rejects it.
- **Inputs:** method=`accessibility.voiceGuidance.enabled`, payload=`{"value":"yes"}`.
- **Expected Result:** `ERROR_BAD_REQUEST`.

### AGC_L0_017
- **Description:** `lifecycle.ready` routes through the handler map to `LifecycleReady()`. In L0, `mLifecycleManagerState` is null (because `ConfigUtils::useAppManagers()` checks `/opt/ai2managers` which doesn't exist in CI), so LifecycleReady sets `result="null"` and returns `ERROR_NONE`.
- **Inputs:** method=`lifecycle.ready`, empty payload.
- **Expected Result:** `ERROR_NONE`.

### AGC_L0_018
- **Description:** `QueryInterface` for `IAppGatewayRequestHandler` returns a valid pointer.
- **Inputs:** Interface ID = `IAppGatewayRequestHandler::ID`.
- **Expected Result:** Non-null pointer.

### AGC_L0_019
- **Description:** `QueryInterface` for `IAppGatewayAuthenticator` returns a valid pointer.
- **Inputs:** Interface ID = `IAppGatewayAuthenticator::ID`.
- **Expected Result:** Non-null pointer.

## Methods Not Fully Covered (L0 Limitations)

| Method                         | Reason                                                   |
|--------------------------------|----------------------------------------------------------|
| Delegate getter methods        | Require live COM-RPC / JSON-RPC links (L1/L2 scope)     |
| EventRegistration callbacks    | Require asynchronous Thunder event framework (L1 scope)  |
| HandleAppNotification          | Requires notification sender plugin (L1 scope)           |

## Coverage & Build

- **CMake target:** `appgatewaycommon_l0test` (in `Tests/L0Tests/CMakeLists.txt`)
- **Coverage:** Enabled via `APPGW_ENABLE_COVERAGE=ON`; reports generated by lcov/genhtml in `coverage-html/`
- **Workflow:** `.github/workflows/L0-tests.yml` builds and runs both `appgateway_l0test` and `appgatewaycommon_l0test`
- **Adding a new plugin:** Append target name to `L0_PLUGIN_TESTS` env var in the workflow
