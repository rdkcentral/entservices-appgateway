# AppGateway Plugin — L2 Test Plan

## 1. Overview

This document defines the L2 test strategy for the **AppGateway** plugin in `entservices-appgateway`.  
L2 tests run inside a full Thunder stack (Docker/Ubuntu CI), invoke APIs via JSON-RPC, and validate end-to-end plugin behaviour without direct access to internal C++ state.

### Coverage Target

| Metric | Target |
|---|---|
| Line coverage | ≥ 75 % |
| Branch coverage | ≥ 75 % (all reachable branches exercised) |
| Function coverage | 100 % of public API surface |

---

## 2. Plugin Component Map

```
AppGateway plugin
│
├── AppGatewayImplementation      ← IAppGatewayResolver + IConfiguration
│   ├── Configure(IShell)         — reads country, loads resolutions
│   ├── Configure(IStringIterator)— explicit path override
│   ├── Resolve(context,…)        — dispatches to:
│   │   ├── FetchResolvedData     — main resolution branch tree
│   │   │   ├── [no alias]         → ERROR_GENERAL / NotSupported
│   │   │   ├── [permission group] → Authenticate → allowed / denied
│   │   │   ├── [HasEvent]         → PreProcessEvent
│   │   │   │   ├── listen=true    → HandleEvent → IAppNotifications::Subscribe
│   │   │   │   ├── listen=false   → HandleEvent → IAppNotifications::Subscribe
│   │   │   │   └── missing param  → ERROR_BAD_REQUEST
│   │   │   ├── [HasComRpcRequest] → ProcessComRpcRequest → IAppGatewayRequestHandler
│   │   │   └── [plain call]       → Resolver::CallThunderPlugin → JSON-RPC forward
│   │   └── RespondJob (async)     → ReturnMessageInSocket | SendToLaunchDelegate
│   ├── ReadCountryFromConfigFile  — vendor → build config fallback
│   └── InitializeResolver         — RESOLUTIONS_PATH_CFG → regional fallback
│
└── AppGatewayResponderImplementation  ← IAppGatewayResponder + IConfiguration
    ├── Configure(IShell)          — starts WebSocket server
    ├── Respond(context, payload)  — route payload back to WS client
    ├── Emit(context, method, payload) — push event to WS client
    ├── Request(connId, id, method, params) — forward RPC to WS client
    ├── GetGatewayConnectionContext — retrieve per-connection context value
    ├── RecordGatewayConnectionContext — store per-connection context value
    ├── Register/Unregister notification — observer lifecycle
    └── OnConnectionStatusChanged  — WebSocket connect/disconnect event
```

---

## 3. Branch Coverage Map

The table below maps every significant code branch to the test cases that cover it.

| Branch | Covered By |
|---|---|
| `InitializeResolver` — resolutions file not found → fallback default | TC-INIT-03 |
| `InitializeResolver` — file found, parse fails → fallback default | TC-INIT-04 |
| `InitializeResolver` — country empty → use defaultCountryCode | TC-INIT-05 |
| `InitializeResolver` — country found in region | TC-INIT-01 |
| `InitializeResolver` — country not found, no default → last-resort fallback | TC-INIT-06 |
| `Configure(IStringIterator)` — null paths → ERROR_BAD_REQUEST | TC-CFG-01 |
| `Configure(IStringIterator)` — empty iterator → ERROR_BAD_REQUEST | TC-CFG-02 |
| `Configure(IStringIterator)` — valid paths → SUCCESS | TC-CFG-03 |
| `FetchResolvedData` — resolver null | TC-RES-01 |
| `FetchResolvedData` — resolver not configured | TC-RES-02 |
| `FetchResolvedData` — alias not found | TC-RES-03 |
| `FetchResolvedData` — permission group present, authenticator N/A | TC-PERM-01 |
| `FetchResolvedData` — permission check returns error | TC-PERM-02 |
| `FetchResolvedData` — permission denied | TC-PERM-03 |
| `FetchResolvedData` — permission allowed | TC-PERM-04 |
| `FetchResolvedData` — HasEvent=true, listen=true | TC-EVT-01 |
| `FetchResolvedData` — HasEvent=true, listen=false | TC-EVT-02 |
| `FetchResolvedData` — HasEvent=true, missing listen param | TC-EVT-03 |
| `FetchResolvedData` — HasEvent=true, invalid JSON params | TC-EVT-04 |
| `FetchResolvedData` — HasEvent=true, versioned event | TC-EVT-05 |
| `FetchResolvedData` — HasComRpcRequest, handler available, success | TC-COMRPC-01 |
| `FetchResolvedData` — HasComRpcRequest, handler available, failure | TC-COMRPC-02 |
| `FetchResolvedData` — HasComRpcRequest, handler not available | TC-COMRPC-03 |
| `FetchResolvedData` — plain call, includeContext=false | TC-PLAIN-01 |
| `FetchResolvedData` — plain call, includeContext=true | TC-PLAIN-02 |
| `FetchResolvedData` — plain call, thunder returns empty → "null" | TC-PLAIN-03 |
| `RespondJob::Dispatch` — destination is Gateway origin | TC-RESP-01 |
| `RespondJob::Dispatch` — destination is LaunchDelegate | TC-RESP-02 |
| `Respond` — route to WS client | TC-RESP-03 |
| `Emit` — push event to WS client | TC-EMIT-01 |
| `Request` — forward RPC to WS client | TC-REQ-01 |
| `GetGatewayConnectionContext` — key exists | TC-CTX-01 |
| `GetGatewayConnectionContext` — key not found | TC-CTX-02 |
| `RecordGatewayConnectionContext` — store key | TC-CTX-03 |
| WebSocket auth — token missing session | TC-WS-01 |
| WebSocket auth — authenticator unavailable | TC-WS-02 |
| WebSocket auth — authentication succeeds | TC-WS-03 |
| WebSocket auth — authentication fails | TC-WS-04 |
| WebSocket disconnect — appId found | TC-WS-05 |
| WebSocket disconnect — appId not found | TC-WS-06 |

---

## 4. Test Cases

### 4.1 Initialization & Configuration

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-INIT-01** | Resolver loads country-specific config | `Configure(IShell)` | `RESOLUTIONS_PATH_CFG` contains valid JSON with matching country code `"GB"` | Plugin activates successfully; `Resolve` uses GB-specific rules | `InitializeResolver` country match |
| **TC-INIT-02** | Resolver loads base config with no regional file | `Configure(IShell)` | No `resolutions.json` present; `resolution.base.json` exists | Plugin activates; falls back to default path | Missing regional config file |
| **TC-INIT-03** | Regional file exists but cannot be opened | `Configure(IShell)` | `resolutions.json` is write-only (permission denied) | Plugin falls back to `resolution.base.json` | `!resolutionConfigFile.is_open()` |
| **TC-INIT-04** | Regional file has invalid JSON | `Configure(IShell)` | `resolutions.json` contains `{ invalid json }` | Plugin falls back to `resolution.base.json` | `FromString` fails |
| **TC-INIT-05** | Country empty in config; use defaultCountryCode | `Configure(IShell)` | No vendor/build config files set; `resolutions.json` has `"defaultCountryCode": "US"` | Plugin uses US paths | `country.empty() && defaultCountryCode.IsSet()` |
| **TC-INIT-06** | Country not in any region, no default | `Configure(IShell)` | Country `"XX"` not in any region; no default in config | Last-resort fallback to `resolution.base.json` | `configPaths.empty()` after `GetPathsForCountry` |

---

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-CFG-01** | Null paths iterator passed | `Configure(IStringIterator*)` | `paths = nullptr` | Returns `Core::ERROR_BAD_REQUEST`; resolution contains error | Null iterator guard |
| **TC-CFG-02** | Empty paths iterator | `Configure(IStringIterator*)` | Iterator yields zero items | Returns `Core::ERROR_BAD_REQUEST` | `configPaths.empty()` |
| **TC-CFG-03** | Single valid config path | `Configure(IStringIterator*)` | Iterator yields `"/etc/app-gateway/resolution.base.json"` | Returns `Core::ERROR_NONE`; resolver becomes configured | Happy path |
| **TC-CFG-04** | Multiple config paths, last wins | `Configure(IStringIterator*)` | Iterator yields `[base.json, override.json]` | Returns `Core::ERROR_NONE`; later file takes precedence | Multi-path override loop |
| **TC-CFG-05** | One path invalid, one valid | `Configure(IStringIterator*)` | `["/nonexistent.json", "/etc/app-gateway/resolution.base.json"]` | Returns `Core::ERROR_NONE`; warning logged for bad path | `anyConfigLoaded` partial success |
| **TC-CFG-06** | All paths invalid | `Configure(IStringIterator*)` | `["/nonexistent1.json", "/nonexistent2.json"]` | Returns `Core::ERROR_GENERAL`; `anyConfigLoaded = false` | `!anyConfigLoaded` |

---

### 4.2 Resolve — Core Dispatch

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-RES-01** | Resolver pointer is null | `Resolve()` | Resolver not initialized (internal state) | Resolution contains `"Resolver not initialized"` error; `ERROR_GENERAL` | `mResolverPtr == nullptr` |
| **TC-RES-02** | Resolver not configured | `Resolve()` | Resolver initialized but `LoadConfig` never called | Resolution contains `"Resolver not configured"` error | `!mResolverPtr->IsConfigured()` |
| **TC-RES-03** | Method has no alias mapping | `Resolve()` | `method = "device.unknown.method"` | Resolution contains `notSupported` error; `ERROR_GENERAL` | `alias.empty()` |
| **TC-RES-04** | Plain method — valid alias, Thunder returns result | `Resolve()` | `method = "device.version"`, `params = "{}"` | Resolution contains JSON result from downstream plugin | Happy path `CallThunderPlugin` |
| **TC-RES-05** | Plain method — Thunder returns empty string | `Resolve()` | `method = "device.version"`, mock returns `""` | Resolution becomes `"null"` | `resolution.empty()` guard |
| **TC-RES-06** | Plain method — Thunder call fails | `Resolve()` | Mock returns `ERROR_GENERAL` | Resolution contains `"Failed with internal error"` | `result != Core::ERROR_NONE` after `CallThunderPlugin` |

---

### 4.3 Permission Group

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-PERM-01** | Method requires permission, authenticator unavailable | `Resolve()` | Method mapped to `permissionGroup = "premium"`, `IAppGatewayAuthenticator` QueryInterface returns null | Resolution contains `notPermitted` error | Authenticator null |
| **TC-PERM-02** | `CheckPermissionGroup` returns error | `Resolve()` | Authenticator available but `CheckPermissionGroup` returns `ERROR_GENERAL` | Resolution contains `notPermitted` error; telemetry `RecordExternalServiceError` called | `CheckPermissionGroup` failure |
| **TC-PERM-03** | App not in permission group | `Resolve()` | `appId = "com.app.free"`, group `"premium"`, `allowed = false` | Resolution contains `notPermitted` error | `!allowed` |
| **TC-PERM-04** | App is in permission group | `Resolve()` | `appId = "com.app.premium"`, group `"premium"`, `allowed = true` | Resolution proceeds to alias resolution | `allowed = true` |

---

### 4.4 Event Handling

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-EVT-01** | Subscribe to event (`listen = true`) | `Resolve()` | `method = "device.onStatusChanged"`, `params = '{"listen": true}'` | Resolution `{"listening": true, "event": "device.onStatusChanged"}`; `IAppNotifications::Subscribe` called with `listen=true` | `HasEvent` + `listen=true` |
| **TC-EVT-02** | Unsubscribe from event (`listen = false`) | `Resolve()` | `method = "device.onStatusChanged"`, `params = '{"listen": false}'` | Resolution `{"listening": false, "event": "device.onStatusChanged"}`; `IAppNotifications::Subscribe` called with `listen=false` | `listen=false` |
| **TC-EVT-03** | Event params missing `listen` key | `Resolve()` | `method = "device.onStatusChanged"`, `params = '{"foo": true}'` | Resolution contains `badRequest` error | `!HasBooleanEntry(listen)` |
| **TC-EVT-04** | Event params are invalid JSON | `Resolve()` | `method = "device.onStatusChanged"`, `params = "not-json"` | Resolution contains `badRequest` error | `params_obj.FromString` fails |
| **TC-EVT-05** | Versioned event — context selects v2 name | `Resolve()` | `method = "device.onStatusChanged"`, `context.version = 2`, event is versioned | `Subscribe` called with version-qualified event name | `IsVersionedEvent` true |
| **TC-EVT-06** | AppNotifications interface unavailable | `Resolve()` | `IAppNotifications` QueryInterface returns null | Returns `ERROR_GENERAL` | `mAppNotifications == nullptr` after QueryInterface |

---

### 4.5 COM-RPC Request Path

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-COMRPC-01** | COM-RPC handler available, call succeeds | `Resolve()` | `method = "lifecycle.launch"`, handler returns `ERROR_NONE` and valid JSON result | Resolution contains handler's result | `HasComRpcRequestSupport` + success |
| **TC-COMRPC-02** | COM-RPC handler available, call fails | `Resolve()` | `method = "lifecycle.launch"`, handler returns `ERROR_GENERAL` | Resolution contains `"HandleAppGatewayRequest failed"` error; telemetry `RecordApiError` called | Handler failure |
| **TC-COMRPC-03** | COM-RPC handler available, call fails with custom error in resolution | `Resolve()` | Handler returns `ERROR_GENERAL` and sets non-empty `resolution` | Resolution contains the handler-provided error (not overwritten) | `!resolution.empty()` guard |
| **TC-COMRPC-04** | COM-RPC handler not available | `Resolve()` | `QueryInterfaceByCallsign` returns null | Resolution contains `notAvailable` error | `requestHandler == nullptr` |
| **TC-COMRPC-05** | COM-RPC with `includeContext=true` | `Resolve()` | Method config sets `includeContext=true` and `additionalContext = {"env": "prod"}` | Forwarded params contain `_additionalContext` + `origin` field | `onlyAdditionalContext = true` branch in `UpdateContext` |

---

### 4.6 Context Injection (`includeContext`)

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-PLAIN-01** | Plain call, no context injection | `Resolve()` | `method = "account.id"`, `includeContext = false` | Downstream receives original `params` unchanged | `HasIncludeContext` false |
| **TC-PLAIN-02** | Plain call, context injected | `Resolve()` | `method = "account.id"`, `includeContext = true` | Downstream receives params with added `context` object `{appId, connectionId, requestId}` | `HasIncludeContext` true |
| **TC-PLAIN-03** | Context injection — params are not valid JSON | `Resolve()` | `method = "account.id"`, `includeContext = true`, `params = "bad"` | Warning logged; context still injected into empty base object | `!paramsObj.FromString(params)` |

---

### 4.7 Responder — `Respond` / `Emit` / `Request`

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-RESP-01** | Respond routes payload back to Gateway origin | `RespondJob::Dispatch` | Origin is a Gateway socket address | `ReturnMessageInSocket` called; payload delivered to WS client | `IsOriginGateway` = true |
| **TC-RESP-02** | Respond routes payload to LaunchDelegate | `RespondJob::Dispatch` | Origin is a non-Gateway app | `SendToLaunchDelegate` called | `IsOriginGateway` = false |
| **TC-RESP-03** | `Respond` — direct call, WS client present | `IAppGatewayResponder::Respond` | Valid `context.connectionId`, payload `'{"result": 1}'` | Payload delivered to the WS connection | Normal Respond path |
| **TC-EMIT-01** | Emit event to connected client | `IAppGatewayResponder::Emit` | `connectionId = 1`, `method = "onUpdate"`, `payload = '{"val":42}'` | Event frame dispatched to WS client via `DispatchNotificationToConnection` | EmitJob dispatch |
| **TC-REQ-01** | Request forwarded to WS client | `IAppGatewayResponder::Request` | `connectionId = 1`, `id = 100`, `method = "ping"`, `params = "{}"` | Request frame sent to WS client via WsMsgJob | WsMsgJob dispatch |

---

### 4.8 Gateway Connection Context

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-CTX-01** | Record and retrieve context value | `RecordGatewayConnectionContext` then `GetGatewayConnectionContext` | `connectionId=1`, `contextKey="appId"`, `contextValue="com.app.test"` | `GetGatewayConnectionContext` returns `"com.app.test"` | Happy path |
| **TC-CTX-02** | Retrieve non-existent context key | `GetGatewayConnectionContext` | `connectionId=1`, `contextKey="nonExistent"` | Returns error or empty string | Key not found |
| **TC-CTX-03** | Overwrite existing context key | `RecordGatewayConnectionContext` × 2 | Same key recorded twice with different values | Second value is returned on `Get` | Overwrite branch |

---

### 4.9 WebSocket Authentication & Lifecycle

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-WS-01** | Auth token has no session field | WebSocket connect | Token = `"some-token-without-session"` | Auth handler returns `false`; connection rejected | `sessionId.empty()` |
| **TC-WS-02** | Authenticator interface unavailable | WebSocket connect | Token has valid session; `IAppGatewayAuthenticator` QueryInterface = null | Auth handler returns `false`; telemetry error recorded | `mAuthenticator == nullptr` |
| **TC-WS-03** | Successful authentication | WebSocket connect | Token with valid session; `Authenticate` returns `ERROR_NONE`, `appId = "com.app.test"` | Connection registered; `OnConnectionStatusChanged(connected=true)` notification fired | Auth success |
| **TC-WS-04** | Authentication fails | WebSocket connect | Token with valid session; `Authenticate` returns error | Auth handler returns `false`; telemetry records auth failure with `UNKNOWN` appId | `Authenticate` failure |
| **TC-WS-05** | Clean disconnect, appId known | WebSocket disconnect | Previously registered `connectionId` with appId | Telemetry decrements WS connections; `OnConnectionStatusChanged(connected=false)` fired | `mAppIdRegistry.Get` success |
| **TC-WS-06** | Disconnect, appId not found in registry | WebSocket disconnect | `connectionId` not previously registered | `appId = "UNKNOWN"` used; telemetry still decremented | `!mAppIdRegistry.Get` |

---

### 4.10 Notification Observer Lifecycle

| ID | Use Case | Method / API | Input | Expected Output | Branch Covered |
|---|---|---|---|---|---|
| **TC-NOTIF-01** | Register notification observer | `Register(INotification*)` | Valid `INotification` mock | Returns `Core::ERROR_NONE`; observer added to list | Register path |
| **TC-NOTIF-02** | Unregister notification observer | `Unregister(INotification*)` | Same observer registered in TC-NOTIF-01 | Returns `Core::ERROR_NONE`; observer removed | Unregister path |
| **TC-NOTIF-03** | Duplicate register | `Register(INotification*)` | Same observer registered twice | Second call returns error or is a no-op; no duplicate in list | Duplicate guard |
| **TC-NOTIF-04** | Unregister observer not registered | `Unregister(INotification*)` | Observer not previously registered | Returns error | Not-found guard |

---

## 5. Coverage Planning Strategy

### 5.1 Priority Matrix

| Priority | Test Group | Reason |
|---|---|---|
| P1 — Must Have | TC-INIT, TC-CFG, TC-RES-01..06 | Core initialization and main dispatch path; ~40% of line coverage |
| P1 — Must Have | TC-EVT-01..04, TC-PLAIN-01..02 | Highest traffic paths in production |
| P2 — Should Have | TC-PERM-01..04, TC-COMRPC-01..04 | Complex branch trees; push coverage to 70% |
| P2 — Should Have | TC-RESP-01..03, TC-EMIT-01, TC-REQ-01 | Responder paths |
| P3 — Nice to Have | TC-WS-01..06, TC-CTX-01..03, TC-NOTIF-01..04 | Edge cases; push to 80%+ |

### 5.2 Execution Order (CI)

```
1.  TC-INIT-01  → Verify plugin activates cleanly
2.  TC-CFG-03   → Confirm explicit configure works
3.  TC-RES-04   → Smoke-test the resolve happy path
4.  TC-EVT-01/02 → Validate event subscribe/unsubscribe
5.  TC-PLAIN-01/02 → Validate context injection
6.  TC-COMRPC-01 → Validate COM-RPC forward
7.  TC-PERM-04   → Validate permission-allowed flow
8.  TC-WS-03     → Validate WS authentication
9.  [All remaining P2/P3 cases]
```

### 5.3 Mock Requirements

| Dependency | Mock Interface | Used In |
|---|---|---|
| `IAppNotifications` | `MockAppNotifications` | TC-EVT-* |
| `IAppGatewayAuthenticator` | `MockAppGatewayAuthenticator` | TC-PERM-*, TC-WS-* |
| `IAppGatewayRequestHandler` | `MockAppGatewayRequestHandler` | TC-COMRPC-* |
| `IAppGatewayResponder` | `MockAppGatewayResponder` | TC-RESP-* |
| `IAppGatewayResponder` (Internal/LaunchDelegate) | `MockLaunchDelegateResponder` | TC-RESP-02 |
| Resolver (JSON config files) | Temp files via `tmpnam` / fixture | TC-INIT-*, TC-CFG-* |
| WebSocket client | `MockWsClient` | TC-WS-* |

### 5.4 Estimated Coverage Contribution

| Group | Lines Exercised (est.) | Cumulative |
|---|---|---|
| TC-INIT + TC-CFG | ~120 | ~25 % |
| TC-RES + TC-PLAIN | ~90 | ~45 % |
| TC-EVT | ~60 | ~57 % |
| TC-PERM + TC-COMRPC | ~70 | ~71 % |
| TC-RESP + TC-EMIT + TC-REQ | ~40 | ~78 % |
| TC-WS + TC-CTX + TC-NOTIF | ~50 | ~88 % |

Running P1 + P2 cases (first 4 groups) is sufficient to hit the **75 % minimum target**.  
Including all groups pushes coverage to **~88 %**.

---

## 6. JSON-RPC Call Examples

### Resolve a plain method
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "AppGateway.1.resolve",
  "params": {
    "context": { "appId": "com.app.test", "connectionId": 1, "requestId": 10 },
    "origin": "127.0.0.1:3473",
    "method": "device.version",
    "params": "{}"
  }
}
```

### Subscribe to an event
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "AppGateway.1.resolve",
  "params": {
    "context": { "appId": "com.app.test", "connectionId": 1, "requestId": 11 },
    "origin": "127.0.0.1:3473",
    "method": "device.onStatusChanged",
    "params": "{\"listen\": true}"
  }
}
```

### Configure with explicit paths
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "AppGateway.1.configure",
  "params": {
    "paths": ["/etc/app-gateway/resolution.base.json", "/etc/app-gateway/override.json"]
  }
}
```

---

## 7. File Structure

```
Tests/L2Tests/
├── CMakeLists.txt
├── docs/
│   └── AppGateway_L2_TestPlan.md       ← this file
└── tests/
    └── AppGateway_L2Test.cpp           ← test implementation
```

The test file `AppGateway_L2Test.cpp` should be structured as GTest test suites mapping to each section above:

```cpp
class AppGatewayInitTest      : public AppGatewayL2TestBase { ... }; // TC-INIT-*
class AppGatewayConfigTest    : public AppGatewayL2TestBase { ... }; // TC-CFG-*
class AppGatewayResolveTest   : public AppGatewayL2TestBase { ... }; // TC-RES-*
class AppGatewayEventTest     : public AppGatewayL2TestBase { ... }; // TC-EVT-*
class AppGatewayPermTest      : public AppGatewayL2TestBase { ... }; // TC-PERM-*
class AppGatewayComRpcTest    : public AppGatewayL2TestBase { ... }; // TC-COMRPC-*
class AppGatewayResponderTest : public AppGatewayL2TestBase { ... }; // TC-RESP/EMIT/REQ
class AppGatewayContextTest   : public AppGatewayL2TestBase { ... }; // TC-CTX-*
class AppGatewayWsTest        : public AppGatewayL2TestBase { ... }; // TC-WS-*
class AppGatewayNotifTest     : public AppGatewayL2TestBase { ... }; // TC-NOTIF-*
```
