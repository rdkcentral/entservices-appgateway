# AppNotifications L1 Test Cases — Deep Dive

> **File:** `Tests/L1Tests/tests/test_AppNotifications.cpp`
> **Test binary:** `AppNotificationsL1Test` (built by `CMakeLists.txt` when `-DPLUGIN_APPNOTIFICATIONS=ON`)
> **Implementation under test:** `AppNotifications/AppNotificationsImplementation.cpp/.h`

---

## Table of Contents

1. [What is an L1 Test?](#1-what-is-an-l1-test)
2. [Is Thunder Running?](#2-is-thunder-running)
3. [Architecture of the Plugin Under Test](#3-architecture-of-the-plugin-under-test)
4. [How the Test Binary is Built and Linked](#4-how-the-test-binary-is-built-and-linked)
5. [Mocks Used and What They Replace](#5-mocks-used-and-what-they-replace)
6. [Test Fixture — AppNotificationsTest](#6-test-fixture--appnotificationstest)
7. [Worker Pool — How Async Jobs Are Tested](#7-worker-pool--how-async-jobs-are-tested)
8. [Flow Diagrams](#8-flow-diagrams)
9. [Test Groups — Detailed Breakdown](#9-test-groups--detailed-breakdown)
   - [9.1 Configure Tests](#91-configure-tests)
   - [9.2 Subscribe Tests](#92-subscribe-tests)
   - [9.3 Emit Tests](#93-emit-tests)
   - [9.4 Cleanup Tests](#94-cleanup-tests)
   - [9.5 SubscriberMap Internal Tests](#95-subscribermap-internal-tests)
   - [9.6 ThunderSubscriptionManager Internal Tests](#96-thundersubscriptionmanager-internal-tests)
   - [9.7 Emitter Tests](#97-emitter-tests)
   - [9.8 End-to-End Scenario Tests](#98-end-to-end-scenario-tests)
   - [9.9 Additional Subscribe Tests](#99-additional-subscribe-tests)
   - [9.10 Additional Emit Tests](#910-additional-emit-tests)
   - [9.11 Additional Cleanup Tests](#911-additional-cleanup-tests)
   - [9.12 Additional SubscriberMap Internal Tests](#912-additional-subscribermap-internal-tests)
   - [9.13 Additional ThunderSubscriptionManager Tests](#913-additional-thundersubscriptionmanager-tests)
   - [9.14 Emitter + NotificationHandler Tests](#914-emitter--notificationhandler-tests)
   - [9.15 HandleNotifier + Emitter Integration Tests](#915-handlenotifier--emitter-integration-tests)
   - [9.16 Full End-to-End Pipeline Tests](#916-full-end-to-end-pipeline-tests)
   - [9.17 GatewayContext Field Verification Tests](#917-gatewaycontext-field-verification-tests)
10. [Complete Test Index](#10-complete-test-index)
11. [Key Design Decisions and Gotchas](#11-key-design-decisions-and-gotchas)

---

## 1. What is an L1 Test?

In the RDK/WPEFramework plugin world, tests are classified by how much of the stack they exercise:

| Level | Name | What runs |
|-------|------|-----------|
| **L1** | Unit / Component test | Plugin implementation only. All external dependencies (Thunder daemon, other plugins, D-Bus, hardware) are **replaced by mocks**. The test binary is a standalone executable. |
| L2 | Integration test | Plugin loaded inside a real (or embedded) Thunder daemon. |
| L3 | System test | Full device stack. |

**AppNotificationsL1Test is a pure L1 test.**  
No Thunder daemon runs. No real `org.rdk.AppGateway` plugin runs. No real `org.rdk.LaunchDelegate` runs. Every external call is intercepted by a GoogleMock mock object.

---

## 2. Is Thunder Running?

**No.** Not at all.

The key evidence:

- The build is a standalone executable (`add_executable(AppNotificationsL1Test ...)`) — it is not a Thunder plugin `.so`.
- `PluginHost::IShell` (the interface Thunder normally provides to a plugin) is replaced by `ServiceMock`, a GoogleMock class.
- `Core::IWorkerPool` (the thread pool Thunder normally manages) is replaced by `WorkerPoolImplementation`, a test-local implementation spun up and torn down per-fixture.
- `QueryInterfaceByCallsign()` — the call a plugin would make to ask Thunder "give me the interface of another running plugin" — is mocked and returns whatever the test tells it to return.

The consequence: **the tests run entirely in process, with no IPC, no JSON-RPC, no Thunder daemon socket**.

---

## 3. Architecture of the Plugin Under Test

```
AppNotificationsImplementation
│
├── mShell               PluginHost::IShell*
│                        → In tests: ServiceMock
│                        → Used to: AddRef/Release, QueryInterfaceByCallsign
│
├── mSubMap              SubscriberMap
│   ├── mSubscribers     std::map<string, vector<AppNotificationContext>>
│   │                    Key = lowercase event name
│   │                    Value = list of contexts (one per Subscribe() call)
│   ├── mAppGateway      Exchange::IAppGatewayResponder*  (cached, lazy-init)
│   │                    → In tests: AppGatewayResponderMock
│   │                    → Acquired via mShell->QueryInterfaceByCallsign("org.rdk.AppGateway")
│   └── mInternalGatewayNotifier  Exchange::IAppGatewayResponder*  (cached, lazy-init)
│                        → In tests: AppGatewayResponderMock
│                        → Acquired via mShell->QueryInterfaceByCallsign("org.rdk.LaunchDelegate")
│
├── mThunderManager      ThunderSubscriptionManager
│   └── mRegisteredNotifications  vector<{module, event}>
│                        Tracks which (module, event) pairs have been
│                        successfully registered with their handler plugin.
│
└── mEmitter             Core::Sink<Emitter>
                         Implements Exchange::IAppNotificationHandler::IEmitter
                         → When an external plugin calls emitter->Emit(...),
                           it submits an EmitJob to the worker pool.
```

### Key interfaces

| Interface | Implemented by | Purpose |
|-----------|---------------|---------|
| `Exchange::IAppNotifications` | `AppNotificationsImplementation` | Public API: `Subscribe`, `Emit`, `Cleanup` |
| `Exchange::IConfiguration` | `AppNotificationsImplementation` | `Configure(shell)` called at plugin init |
| `Exchange::IAppNotificationHandler::IEmitter` | `AppNotificationsImplementation::Emitter` | Passed to external plugins so they can push events back in |
| `Exchange::IAppNotificationHandler` | `AppNotificationHandlerMock` (test mock) | External plugin interface — receives `HandleAppEventNotifier()` calls |
| `Exchange::IAppGatewayResponder` | `AppGatewayResponderMock` (test mock) | Receives final dispatched events via `Emit(GatewayContext, event, payload)` |
| `PluginHost::IShell` | `ServiceMock` (test mock) | Thunder shell — used for `QueryInterfaceByCallsign`, `AddRef`, `Release` |

---

## 4. How the Test Binary is Built and Linked

### CMake build flags

```
cmake -S Tests/L1Tests -B build \
  -DPLUGIN_APPNOTIFICATIONS=ON \
  -DCMAKE_PREFIX_PATH=<repo>/install/usr
```

The `CMakeLists.txt` then:

1. `add_subdirectory(../../AppNotifications ...)` — compiles the **real** implementation into a static/shared library target named `AppNotifications`.
2. `add_executable(AppNotificationsL1Test tests/test_AppNotifications.cpp)`
3. Links against: `AppNotifications`, `GTest::gmock`, `GTest::gtest`, `GTest::gtest_main`, `Threads::Threads`, `WPEFrameworkPlugins`.
4. Adds compile definition `DISABLE_SECURITY_TOKEN` — disables security token validation so plugin code does not try to contact a SecurityAgent.

### What `#define private public` does

At the top of the test file:

```cpp
#define private public
#include "AppNotificationsImplementation.h"
#undef private
```

This makes all `private` members of `AppNotificationsImplementation` (including `mShell`, `mSubMap`, `mThunderManager`, `mEmitter`) accessible to the test code as if they were `public`. This is a compile-time-only trick — it does **not** change the ABI or vtable. It lets tests reach directly into the object's internals to verify state without adding production getters.

### Include paths

| Path | Purpose |
|------|---------|
| `../../helpers` | `WorkerPoolImplementation.h`, `ServiceMock.h`, `ThunderPortability.h` |
| `../../AppNotifications` | `AppNotificationsImplementation.h` |
| `Tests/mocks` or `../mocks` | `AppGatewayMock.h`, `AppNotificationHandlerMock.h` |

---

## 5. Mocks Used and What They Replace

### 5.1 ServiceMock (`ServiceMock.h`)

Replaces `PluginHost::IShell` — the object Thunder normally injects into a plugin.

Key methods mocked:

| Method | Real behaviour | Test behaviour |
|--------|---------------|----------------|
| `AddRef()` | Increments Thunder's internal ref count | `NiceMock` default: no-op |
| `Release()` | Decrements; may destroy | `NiceMock` default: returns `ERROR_NONE` |
| `QueryInterfaceByCallsign(iface_id, callsign)` | Asks Thunder to find a running plugin and return an interface pointer | Tests configure this to return either `nullptr` or a pointer to a mock object (cast to `void*`) |

Because the fixture uses `NiceMock<ServiceMock>`, any call not explicitly set up with `EXPECT_CALL` or `ON_CALL` silently returns a zero/null default instead of failing the test.

### 5.2 AppGatewayResponderMock (`AppGatewayMock.h`)

Replaces `Exchange::IAppGatewayResponder` — the interface on `org.rdk.AppGateway` and `org.rdk.LaunchDelegate`.

Key method mocked:

```cpp
MOCK_METHOD(Core::hresult, Emit,
    (const Exchange::GatewayContext&, const string& event, const string& payload),
    (override));
```

Tests set `EXPECT_CALL(*gatewayMock, Emit(...))` to verify the correct event name, payload, and `GatewayContext` fields reach the downstream plugin.

The mock implements real `AddRef`/`Release` ref counting so tests must call `gatewayMock->Release()` after the `SubscriberMap` destructor has released its cached pointer, to avoid leaks/crashes.

### 5.3 AppNotificationHandlerMock (`AppNotificationHandlerMock.h`)

Replaces `Exchange::IAppNotificationHandler` — the interface on a plugin that wants to receive event subscriptions (e.g. `org.rdk.Pipeline`, `org.rdk.SomePlugin`).

Key method mocked:

```cpp
MOCK_METHOD(Core::hresult, HandleAppEventNotifier,
    (Exchange::IAppNotificationHandler::IEmitter* emitter,
     const string& event,
     bool listen,
     bool& status),
    (override));
```

When `ThunderSubscriptionManager::HandleNotifier()` is called it does:

```cpp
Exchange::IAppNotificationHandler* internalNotifier =
    mParent.mShell->QueryInterfaceByCallsign<...>(module);
internalNotifier->HandleAppEventNotifier(&mParent.mEmitter, event, listen, status);
```

The mock intercepts that call so tests can:
- Verify the correct `emitter` pointer, event name, and `listen` flag were passed.
- Control whether `status` is set to `true` or `false`.
- Control the return code (`ERROR_NONE` vs `ERROR_GENERAL`).
- Simulate exceptions to test error propagation.

### 5.4 NotificationHandler (test-local class)

Not a mock — a real `IEmitter` implementation used in tests to act as the recipient of `Emit()` callbacks. It captures the `event`, `payload`, and `appId` into thread-safe fields and exposes `WaitForRequestStatus()` so the test can block until the async callback arrives or timeout.

---

## 6. Test Fixture — AppNotificationsTest

```cpp
class AppNotificationsTest : public ::testing::Test {
protected:
    NiceMock<ServiceMock> service;                         // (1) declared first
    Core::Sink<AppNotificationsImplementation> impl;       // (2) declared second
    Core::ProxyType<WorkerPoolImplementation> workerPool;  // (3)
    ...
};
```

### Member declaration order matters

C++ destroys members in **reverse declaration order**. `impl` is destroyed before `service`. This is intentional:

- `impl`'s destructor calls `mThunderManager.ClearRegistrations()` then `mShell->Release()`.
- `mShell` points to `service`.
- If `service` were destroyed first, `mShell->Release()` would be a use-after-free crash.

### Constructor sequence

```
AppNotificationsTest()
  1. workerPool created (2 threads, default stack size, 64-job queue)
  2. ON_CALL defaults on service: AddRef→noop, Release→ERROR_NONE,
     QueryInterfaceByCallsign→nullptr
  3. Core::IWorkerPool::Assign(&workerPool)  — installs the pool globally
  4. workerPool->Run()                       — starts worker threads
  5. impl.Configure(&service)               — stores mShell = &service,
                                              calls service.AddRef()
  6. ASSERT(impl.mShell != nullptr)         — sanity check in SetUp()
```

### Destructor sequence

```
~AppNotificationsTest()
  1. sleep 60ms          — drain any in-flight worker-pool jobs
  2. workerPool->Stop()  — stops worker threads, waits for drain
  3. IWorkerPool::Assign(nullptr)  — clears global singleton
  4. (C++ member dtors in reverse order)
     → impl destroyed: ClearRegistrations(), mShell->Release()
     → service destroyed
     → workerPool proxy released
```

---

## 7. Worker Pool — How Async Jobs Are Tested

The plugin uses `Core::IWorkerPool::Instance().Submit(job)` for both `Subscribe` and `Emit`. This means:

- `Subscribe(ctx, listen=true, module, event)` — immediately returns `ERROR_NONE`, but the actual `ThunderSubscriptionManager::Subscribe()` call happens asynchronously on a worker thread.
- `Emit(event, payload, appId)` — immediately returns `ERROR_NONE`, but `SubscriberMap::EventUpdate()` runs asynchronously.

Tests that need to verify side-effects of these async operations use a `sleep_for`:

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(80));   // most Emit tests
std::this_thread::sleep_for(std::chrono::milliseconds(150));  // some handler tests
```

Tests that call internal methods directly (e.g. `impl.mSubMap.EventUpdate(...)` or `impl.mThunderManager.HandleNotifier(...)`) bypass the worker pool entirely and are **synchronous** — no sleep needed.

---

## 8. Flow Diagrams

### 8.1 Subscribe flow

```
Test calls impl.Subscribe(ctx, listen=true, module, event)
  │
  ▼
AppNotificationsImplementation::Subscribe()
  │
  ├─ mSubMap.Exists(event)?
  │    NO → Submit SubscriberJob(module, event, subscribe=true) to worker pool
  │
  └─ mSubMap.Add(event, ctx)
       └─ key stored as lowercase(event)
       └─ ctx pushed to vector at that key
  │
  └─ return ERROR_NONE   ← immediate
  
[Worker thread later runs SubscriberJob::Dispatch()]
  │
  ▼
ThunderSubscriptionManager::Subscribe(module, event)
  │
  ├─ IsNotificationRegistered? YES → log "already registered", return
  │
  └─ RegisterNotification(module, event)
       │
       └─ HandleNotifier(module, event, listen=true)
            │
            └─ mShell->QueryInterfaceByCallsign<IAppNotificationHandler>(module)
                 │
                 ├─ nullptr → log error, return false
                 │
                 └─ handler→HandleAppEventNotifier(&mEmitter, event, true, status)
                      │
                      ├─ status=true  → add {module, event} to mRegisteredNotifications
                      └─ status=false → do NOT add to mRegisteredNotifications
```

### 8.2 Emit flow

```
Test calls impl.Emit(event, payload, appId)
  │
  ▼
AppNotificationsImplementation::Emit()
  │
  └─ Submit EmitJob(event, payload, appId) to worker pool
  └─ return ERROR_NONE   ← immediate

[Worker thread later runs EmitJob::Dispatch()]
  │
  ▼
SubscriberMap::EventUpdate(event, payload, appId)
  │
  ├─ lowercase(event) → look up in mSubscribers
  │    not found → log warning, return
  │
  └─ for each AppNotificationContext in vector:
       ├─ if appId non-empty: only dispatch if context.appId == appId
       └─ else: dispatch to all
            │
            ▼
       SubscriberMap::Dispatch(event, context, payload)
            │
            ├─ IsOriginGateway(context.origin)?
            │    YES → DispatchToGateway(event, context, payload)
            │             └─ lazy-init mAppGateway via QueryInterfaceByCallsign("org.rdk.AppGateway")
            │             └─ mAppGateway->Emit(GatewayContext{requestId, connectionId, appId}, event, payload)
            │
            └─ NO  → DispatchToLaunchDelegate(event, context, payload)
                         └─ lazy-init mInternalGatewayNotifier via QueryInterfaceByCallsign("org.rdk.LaunchDelegate")
                         └─ mInternalGatewayNotifier->Emit(GatewayContext{...}, event, payload)
```

### 8.3 Emitter callback flow (external plugin pushes event in)

```
External plugin (during HandleAppEventNotifier) receives IEmitter* emitter
  │
  └─ emitter->Emit(event, payload, appId)
       │
       ▼
  AppNotificationsImplementation::Emitter::Emit()
       │
       └─ Submit EmitJob to worker pool
            │
            [same as 8.2 from EmitJob::Dispatch() onwards]
```

---

## 9. Test Groups — Detailed Breakdown

---

### 9.1 Configure Tests

These tests verify the plugin's `Configure(IShell*)` method.

---

#### `Configure_StoresShellAndAddsRef`

**What it tests:** After `Configure(&service)` is called in the fixture constructor, `impl.mShell` must equal `&service`.

**How it works:**  
Directly reads `impl.mShell` (accessible via `#define private public`) and asserts pointer equality.

**Thunder involvement:** None. `service` is a `NiceMock<ServiceMock>`.

**Why it matters:** Every other operation in the plugin dereferences `mShell`. If `Configure` fails to store the shell, the plugin is dead.

---

#### `Configure_CalledTwice_ReleasesOldShellAndAddsRefNew`

**What it tests:** Calling `Configure()` a second time with a different `IShell*` must release the old shell and start using the new one.

**How it works:**
1. Creates `service2` (another `NiceMock<ServiceMock>`).
2. Calls `impl.Configure(&service2)`.
3. Asserts `impl.mShell == &service2`.
4. Calls `impl.Configure(&service)` again to restore the fixture's original shell — otherwise the destructor would call `Release()` on the already-destroyed `service2`.

**Thunder involvement:** None.

**Why it matters:** Plugin re-initialization paths (e.g. Thunder plugin activate/deactivate cycles) can call `Configure` more than once. A missing `Release()` on the old shell leaks a reference.

---

### 9.2 Subscribe Tests

These tests verify `AppNotificationsImplementation::Subscribe()`.

---

#### `Subscribe_Listen_True_NewEvent_ReturnsNone`

**What it tests:** Subscribing to a brand-new event (not yet in the map) returns `ERROR_NONE` immediately.

**How it works:** Calls `impl.Subscribe(ctx, true, "org.rdk.SomePlugin", "someEvent")` and checks the return code.

**Thunder involvement:** None directly. A `SubscriberJob` is submitted to the worker pool which will later call `QueryInterfaceByCallsign("org.rdk.SomePlugin")` — the `NiceMock` default returns `nullptr`, so `HandleNotifier` logs an error and returns false, but the test doesn't care about that because it only checks the return code.

---

#### `Subscribe_Listen_True_SameEvent_TwiceNoExtraWorkerJob`

**What it tests:** Subscribing to the same event a second time (same `module`+`event`, different `ctx`) does **not** submit a second `SubscriberJob` — only the first subscription triggers a Thunder registration.

**How it works:**
1. Subscribe `ctx1` → event not in map → `SubscriberJob` submitted, `ctx1` added.
2. Subscribe `ctx2` → event already in map → no new job, `ctx2` appended.
3. Assert `impl.mSubMap.Get("someevent")` returns size 2.

**Why it matters:** Double-registering the same event with a Thunder plugin would be wasteful and could cause duplicate callbacks.

---

#### `Subscribe_Listen_True_AddsContextToMap`

**What it tests:** After subscribing, the context is retrievable from the map with the correct `connectionId` and `appId`.

**How it works:** Subscribes with `MakeContext(5, 200, "app5", GATEWAY_CALLSIGN)` for event `"testEvent"`. Retrieves via `impl.mSubMap.Get("testevent")` (note lowercase) and checks `connectionId == 200`, `appId == "app5"`.

**Key detail:** Event keys are **always lowercased** in `SubscriberMap::Add()` via `StringUtils::toLower()`.

---

#### `Subscribe_Listen_True_MixedCaseEvent_KeyIsLowercased`

**What it tests:** Both `"mymixedcaseevent"` and `"MyMixedCaseEvent"` resolve to the same map entry after subscribing with `"MyMixedCaseEvent"`.

**How it works:** Subscribes with mixed-case event. Then checks:
- `Exists("mymixedcaseevent")` → true
- `Exists("MyMixedCaseEvent")` → true (because `Exists` also lowercases its query)
- `Exists("completely_different")` → false

---

#### `Subscribe_Listen_False_RemovesContextFromMap`

**What it tests:** `Subscribe(ctx, false, ...)` removes the context from the map. After removal the key no longer exists.

**How it works:** Subscribe (listen=true) → verify exists → Subscribe (listen=false) → verify not exists.

---

#### `Subscribe_Listen_False_LastContext_EmitsUnsubscribeJob`

**What it tests:** When the last context for an event is removed, a `SubscriberJob` with `subscribe=false` is submitted to the worker pool (which eventually calls `ThunderSubscriptionManager::Unsubscribe`).

**How it works:** One subscribe → one unsubscribe → verify map key gone. The unsubscribe job runs asynchronously; the test verifies the map state, not the worker-pool side-effect.

---

#### `Subscribe_Listen_False_NonExistentEvent_ReturnsNone`

**What it tests:** Unsubscribing an event that was never subscribed is a no-op that still returns `ERROR_NONE`.

**Why it matters:** Callers should not need to track whether they ever subscribed. Graceful handling prevents crashes when clients disconnect mid-flow.

---

#### `Subscribe_Listen_False_OneOfTwoContextsRemoved`

**What it tests:** When two contexts are subscribed for the same event and one is removed, the other remains. The map key persists.

**How it works:** Subscribe ctx1 and ctx2 → unsubscribe ctx1 → assert key still exists and `Get()` returns size 1 with `connectionId == 101` (ctx2).

---

#### `Subscribe_MultipleDistinctEvents_AllPresentInMap`

**What it tests:** Three distinct events can be subscribed independently and all appear in the map.

---

### 9.3 Emit Tests

These tests verify `AppNotificationsImplementation::Emit()`. All async verification uses `sleep_for(80ms)`.

---

#### `Emit_ReturnsNoneImmediately`

**What it tests:** `Emit()` is fire-and-forget. It always returns `ERROR_NONE` without waiting for dispatch.

---

#### `Emit_EmptyPayload_ReturnsNone` / `Emit_EmptyAppId_ReturnsNone` / `Emit_EmptyEvent_ReturnsNone`

**What they test:** Edge cases — empty strings for any of the three parameters do not cause a crash or a non-zero return code.

**Key note on `Emit_EmptyAppId_ReturnsNone`:** An empty `appId` in `EventUpdate` means broadcast to **all** subscribers for the event. This is tested more deeply later.

---

#### `Emit_WithSubscriber_GatewayOrigin_DispatchesToGateway`

**What it tests:** When a subscriber's `origin` is `"org.rdk.AppGateway"`, dispatching an event routes through `DispatchToGateway`, which calls `QueryInterfaceByCallsign("org.rdk.AppGateway")` and then `mAppGateway->Emit(...)`.

**How it works:**
1. Sets up `gatewayMock`.
2. Configures `EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.AppGateway")))` to return `gatewayMock` (with `AddRef()`).
3. Sets `EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber())`.
4. Subscribes ctx with gateway origin.
5. Calls `impl.Emit(...)` → `EmitJob` submitted.
6. Sleeps 80ms → job runs → `DispatchToGateway` called → `gatewayMock->Emit` called.

**GMock LIFO note:** The catch-all `EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))` is registered **before** the specific one for the gateway callsign. GMock evaluates expectations in **reverse registration order** (LIFO), so the specific matcher takes priority for gateway calls while the catch-all absorbs plugin-module calls from the background `SubscriberJob`.

---

#### `Emit_WithSubscriber_NonGatewayOrigin_DispatchesToLaunchDelegate`

**What it tests:** When a subscriber's `origin` is `"org.rdk.LaunchDelegate"`, dispatching routes through `DispatchToLaunchDelegate` instead.

**How it works:** Same structure as the gateway test, but uses `APP_NOTIFICATIONS_DELEGATE_CALLSIGN` as the origin and expects calls to `QueryInterfaceByCallsign(_, "org.rdk.LaunchDelegate")`.

---

#### `Emit_NoSubscribersForEvent_NoDispatch`

**What it tests:** When no subscriber exists for the emitted event, neither the gateway nor delegate `QueryInterfaceByCallsign` is called.

**How it works:** Sets `EXPECT_CALL(...).Times(0)` for both callsigns, then emits. Sleeps 80ms. If dispatch happened, the `Times(0)` expectation would fail.

---

#### `Emit_WithAppIdFilter_OnlyDispatchesMatchingAppId`

**What it tests:** When `Emit("filteredEvent", "{}", "app1")` is called with a specific `appId`, only the subscriber with `appId == "app1"` receives the event; `"app2"` does not.

**Note:** The test uses `Times(AnyNumber())` on `gatewayMock->Emit` because verifying the exact count (1 vs 2) is covered more precisely in the `Notification_EmitterEmit_AppIdFiltered` and `EventUpdate` tests.

---

#### `Emit_GatewayQueryFails_DoesNotCrash`

**What it tests:** If `QueryInterfaceByCallsign("org.rdk.AppGateway")` returns `nullptr` (gateway plugin not available), `DispatchToGateway` must log an error and return without dereferencing the null pointer.

**Thunder relevance:** In production, the gateway plugin might not be running when a notification fires. This test guards against that race.

---

#### `Emit_LaunchDelegateQueryFails_DoesNotCrash`

**What it tests:** Same null-pointer safety for the `DispatchToLaunchDelegate` path when `"org.rdk.LaunchDelegate"` is unavailable.

---

### 9.4 Cleanup Tests

These tests verify `AppNotificationsImplementation::Cleanup(connectionId, origin)`.

`Cleanup` calls `SubscriberMap::CleanupNotifications(connectionId, origin)` which iterates all map entries and removes any `AppNotificationContext` where **both** `context.connectionId == connectionId` **and** `context.origin == origin`.

---

#### `Cleanup_RemovesAllContextsMatchingConnectionIdAndOrigin`

**What it tests:** Three subscriptions across two events. Cleanup with `connectionId=100` removes the two contexts with that ID and leaves the one with `connectionId=200`.

**Verification:**
- `eventa` key still exists (ctx3 with connId=200 remains).
- `eventb` key is gone (all its contexts had connId=100).

---

#### `Cleanup_ConnectionIdNotPresent_NoChange`

**What it tests:** Cleanup with a `connectionId` that was never subscribed does nothing. Returns `ERROR_NONE`. Existing subscriptions untouched.

---

#### `Cleanup_OriginMismatch_NoChange`

**What it tests:** Even if `connectionId` matches, if `origin` does not match, the context is **not** removed. Both fields must match for removal.

---

#### `Cleanup_EmptyMap_ReturnsNone`

**What it tests:** Cleanup on an empty subscriber map (no subscriptions at all) returns `ERROR_NONE` without crashing.

---

#### `Cleanup_MultipleEventsForConnection_AllCleared`

**What it tests:** One connection subscribed to three different events (`alpha`, `beta`, `gamma`). Cleanup removes all three at once.

---

#### `Cleanup_ThenSubscribe_WorksCorrectly`

**What it tests:** After cleanup removes a subscription, re-subscribing the same event succeeds and the map entry is re-created.

**Why it matters:** Tests the full subscribe → cleanup → re-subscribe lifecycle that happens when an app reconnects.

---

### 9.5 SubscriberMap Internal Tests

These tests bypass the public `impl.Subscribe/Emit/Cleanup` API and call `impl.mSubMap` methods directly (accessible via `#define private public`). They are **synchronous** and do not touch the worker pool.

---

#### `SubscriberMap_Add_And_Exists`

**What it tests:** `Add("SomeKey", ctx)` followed by `Exists("somekey")` returns `true` (key is lowercased on `Add`).

---

#### `SubscriberMap_Get_ReturnsCorrectContexts`

**What it tests:** Two contexts added under the same key. `Get("mykey")` returns a vector of size 2.

---

#### `SubscriberMap_Get_NonExistentKey_ReturnsEmpty`

**What it tests:** `Get("noSuchKey")` returns an empty vector without crashing.

---

#### `SubscriberMap_Remove_ExistingContext_KeyErasedWhenEmpty`

**What it tests:** `Remove(key, ctx)` removes the context. When the vector becomes empty the map key is erased. `Exists` then returns false.

---

#### `SubscriberMap_Remove_NonExistentKey_NoOp`

**What it tests:** `Remove("ghost", ctx)` on a key that was never added does not crash.

---

#### `SubscriberMap_Exists_CaseInsensitive`

**What it tests:** A key stored as `"UPPER"` is found by `Exists("upper")`, `Exists("UPPER")`, and `Exists("Upper")` — all case variants resolve to the same lowercase key.

---

#### `SubscriberMap_CleanupNotifications_ByConnectionAndOrigin`

**What it tests:** `CleanupNotifications(42, GATEWAY_CALLSIGN)` removes only the context with `connectionId=42` and the gateway origin, leaving the other context (`connectionId=43`) untouched.

---

#### `SubscriberMap_CleanupNotifications_ErasesKeyWhenAllRemoved`

**What it tests:** When `CleanupNotifications` removes the only context under a key, the key itself is erased.

---

### 9.6 ThunderSubscriptionManager Internal Tests

These tests call `impl.mThunderManager` methods directly. They are mostly **synchronous** (direct calls, no worker pool).

---

#### `ThunderManager_IsNotificationRegistered_FalseByDefault`

**What it tests:** A freshly constructed `ThunderSubscriptionManager` has an empty `mRegisteredNotifications`. `IsNotificationRegistered` returns false.

---

#### `ThunderManager_RegisterNotification_WhenHandlerAvailable`

**What it tests:** When `QueryInterfaceByCallsign("org.rdk.Module")` returns a mock handler that sets `status=true`, the notification is added to `mRegisteredNotifications`.

**How it works:**
1. `EXPECT_CALL(service, QueryInterfaceByCallsign(_, "org.rdk.Module"))` → returns `handlerMock`.
2. `EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, "notify", true, _))` → sets `status=true`, returns `ERROR_NONE`.
3. Calls `RegisterNotification("org.rdk.Module", "notify")`.
4. Asserts `IsNotificationRegistered("org.rdk.Module", "notify")` → true.

**Thunder involvement:** `QueryInterfaceByCallsign` is the Thunder plugin lookup. In production this would contact the running Thunder daemon to find `org.rdk.Module`. Here the `ServiceMock` intercepts it and returns the test mock.

---

#### `ThunderManager_RegisterNotification_WhenHandlerUnavailable_NotRegistered`

**What it tests:** If `QueryInterfaceByCallsign` returns `nullptr` (plugin not running), `HandleNotifier` logs "not available" and returns false. The notification is **not** added to `mRegisteredNotifications`.

---

#### `ThunderManager_RegisterNotification_HandlerReturnsFalseStatus_NotRegistered`

**What it tests:** The handler plugin returns `status=false` (it rejected the registration). `HandleNotifier` returns false. The notification is **not** added.

**Why it matters:** A plugin might legitimately say "I don't support this event" — that should not be treated as a successful registration.

---

#### `ThunderManager_UnregisterNotification_WhenRegistered`

**What it tests:** Full register → unregister cycle. After a successful `RegisterNotification` call, `UnregisterNotification` calls `HandleNotifier(module, event, listen=false)`, and on receiving `status=true`, removes the entry from `mRegisteredNotifications`.

---

#### `ThunderManager_UnregisterNotification_WhenNotRegistered_NoOp`

**What it tests:** Calling `UnregisterNotification` for a module/event pair that was never registered logs an error but does not crash and does not call `QueryInterfaceByCallsign`.

---

#### `ThunderManager_Subscribe_AlreadyRegistered_NoDuplicateRegistration`

**What it tests:** Calling `Subscribe("org.rdk.ModDup", "dupEvt")` twice. The second call finds it already in `mRegisteredNotifications` and skips the handler call. `HandleAppEventNotifier` is expected exactly once (`Times(1)`).

---

#### `ThunderManager_HandleNotifier_HandlerReturnsError_ReturnsFalse`

**What it tests:** When `HandleAppEventNotifier` returns `ERROR_GENERAL` (not `ERROR_NONE`), `HandleNotifier` returns `false`.

**Note:** The return value of `HandleNotifier` is the `status` bool (set as an out-parameter by the handler), not the `hresult`. If the call itself fails (`hresult != ERROR_NONE`), `status` was never written, so it retains its initial value of `false`.

---

#### `ThunderManager_HandleNotifier_ModuleNotAvailable_ReturnsFalse`

**What it tests:** When `QueryInterfaceByCallsign` returns `nullptr`, `HandleNotifier` returns `false` without calling any handler.

---

### 9.7 Emitter Tests

These tests exercise `impl.mEmitter` (the `Core::Sink<Emitter>` object).

---

#### `Emitter_Emit_SubmitsEmitJobToWorkerPool`

**What it tests:** Calling `impl.mEmitter.Emit("emitterEvent", "{\"x\":1}", "appZ")` does not throw. The `EmitJob` is submitted to the worker pool and runs without crashing (even if there are no subscribers for the event).

---

#### `Emitter_Emit_EmptyArguments_NocrashNoThrow`

**What it tests:** `impl.mEmitter.Emit("", "", "")` is safe — empty strings do not cause a crash.

---

### 9.8 End-to-End Scenario Tests

These tests exercise the full `Subscribe → Emit → Cleanup` pipeline as a user of the public API.

---

#### `EndToEnd_SubscribeEmitCleanup_GatewayOrigin`

**What it tests:** The complete lifecycle:
1. Subscribe a context with gateway origin.
2. Emit the event → verify it reaches `gatewayMock->Emit`.
3. Cleanup the connection → verify the map entry is gone.

**Steps in detail:**
1. Configure `service` to return `gatewayMock` for gateway callsign.
2. `impl.Subscribe(ctx, true, "org.rdk.Plugin", "e2eEvent")` → map entry created.
3. `impl.Emit("e2eEvent", "{\"status\":\"ok\"}", "e2eApp")` → `EmitJob` queued.
4. Sleep 80ms → `EmitJob` runs → `EventUpdate` → `DispatchToGateway` → `gatewayMock->Emit` called.
5. `impl.Cleanup(300, GATEWAY_CALLSIGN)` → map entry removed.
6. Assert `Exists("e2eevent") == false`.

---

#### `EndToEnd_MultipleSubscribersEmitOneAppId`

**What it tests:** Two subscribers for the same event, but emit specifies `appId="appAlpha"`. Only `appAlpha`'s subscriber should receive the dispatch (verified by setting `Times(AnyNumber())` — the exact count is covered more precisely in other tests).

---

### 9.9 Additional Subscribe Tests

---

#### `Subscribe_EmptyModule_Listen_True_ReturnsNoneAndAddsToMap`

**What it tests:** An empty module string is technically valid at the API level. `Subscribe` returns `ERROR_NONE` and adds the context. The `SubscriberJob` will be submitted with an empty module string — it is the caller's problem if no handler exists for it.

---

#### `Subscribe_EmptyEvent_Listen_True_ReturnsNoneAndAddsToMap`

**What it tests:** An empty event name becomes an empty string key `""` in the map. The context is retrievable. This tests that the map handles edge-case keys without crashing.

---

#### `Subscribe_PartialUnsubscribe_ThenResubscribe_Works`

**What it tests:** Subscribe two contexts → unsubscribe one (key persists with one context) → re-subscribe the first one → key now has two contexts again. No extra `SubscriberJob` is submitted on re-subscribe because the key already existed.

---

#### `Subscribe_Listen_False_EmptyEvent_NoEntry_ReturnsNone`

**What it tests:** Unsubscribing an empty event key that was never added does not crash and returns `ERROR_NONE`.

---

### 9.10 Additional Emit Tests

---

#### `Emit_AfterCleanup_NoDispatch`

**What it tests:** After a `Cleanup` call removes all subscribers for an event, a subsequent `Emit` for that event does **not** call `QueryInterfaceByCallsign` for the gateway or delegate.

**How it works:** Uses `EXPECT_CALL(...).Times(0)` for both dispatch callsigns. If any dispatch happens, the test fails.

---

#### `Emit_EmptyAppId_DispatchesToAllSubscribers`

**What it tests:** `Emit("broadcastEvt", "{\"data\":\"all\"}", "")` — empty `appId` broadcasts to **all** subscribers. With two subscribers, `gatewayMock->Emit` must be called at least twice (`AtLeast(2)`).

---

#### `Emit_AppIdNoMatch_NoDispatch`

**What it tests:** Emitting with `appId="differentApp"` when the only subscriber has `appId="app1"` — no dispatch occurs. `gatewayMock->Emit` expected `Times(0)`.

---

### 9.11 Additional Cleanup Tests

---

#### `Cleanup_EmptyOrigin_OnlyRemovesEmptyOriginContexts`

**What it tests:** Two contexts under the same key — one with `origin=""` and one with `origin=GATEWAY_CALLSIGN`. `Cleanup(100, "")` removes only the empty-origin context. The gateway-origin context survives.

---

#### `Cleanup_ZeroConnectionId_OnlyRemovesMatchingConnId`

**What it tests:** `connectionId=0` is a valid value. `Cleanup(0, GATEWAY_CALLSIGN)` removes only the context with `connectionId=0` and leaves `connectionId=100` intact.

---

#### `Cleanup_BothOriginAndConnIdMustMatch_ConnIdMatchOnly_NoRemoval`

**What it tests:** Confirms that matching `connectionId` alone is not enough — `origin` must also match. A context with `(connId=100, origin=GATEWAY_CALLSIGN)` is NOT removed by `Cleanup(100, "org.rdk.SomeOtherOrigin")`.

---

### 9.12 Additional SubscriberMap Internal Tests

---

#### `SubscriberMap_Remove_OnlyRemovesOneMatchingContext_WhenMultipleSameKey`

**What it tests:** Two **identical** contexts added under the same key. `Remove` uses `std::remove` which removes **all** equal elements (not just the first). After one `Remove` call, the vector is empty and the key is erased.

**Important behavioural note:** This is documented in the test comment — `std::remove` in the implementation removes all matching entries, not just the first. This is the expected (though potentially surprising) behaviour.

---

#### `SubscriberMap_CleanupNotifications_ConnIdMatchOnly_OriginMismatch_NoRemoval`

**What it tests:** Direct call to `mSubMap.CleanupNotifications(55, "wrongOrigin")` on a context with `(connId=55, origin=GATEWAY_CALLSIGN)`. Nothing is removed.

---

#### `SubscriberMap_DispatchToGateway_CachesInterface_QueryCalledOnce`

**What it tests:** The `mAppGateway` pointer is a **lazy-initialized cache**. The first `DispatchToGateway` call acquires it via `QueryInterfaceByCallsign`. The second call reuses the cached pointer without calling `QueryInterfaceByCallsign` again.

**How it works:**
- `EXPECT_CALL(service, QueryInterfaceByCallsign(_, GATEWAY_CALLSIGN)).Times(1)` — exactly once.
- `EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(2)` — called both times.
- Calls `DispatchToGateway` twice directly.

**Why it matters:** Avoids expensive interface lookup on every single event dispatch.

---

#### `SubscriberMap_DispatchToGateway_NullGateway_DoesNotCrash`

**What it tests:** Null-safety when the gateway cannot be acquired. `EXPECT_NO_THROW` verifies no exception or crash.

---

#### `SubscriberMap_DispatchToLaunchDelegate_NullDelegate_DoesNotCrash`

**What it tests:** Same null-safety for the delegate path.

---

#### `SubscriberMap_DispatchToLaunchDelegate_CachesInterface_QueryCalledOnce`

**What it tests:** Mirror of the gateway cache test for `mInternalGatewayNotifier`. Exactly one `QueryInterfaceByCallsign` call even when dispatching twice.

---

#### `SubscriberMap_EventUpdate_AppIdEmpty_DispatchesToAllSubscribers`

**What it tests:** `EventUpdate("evtAll", "{}", "")` with three subscribers (a1, a2, a3) → `gatewayMock->Emit` called exactly **three** times.

**This is synchronous** — calls `EventUpdate` directly, no sleep needed.

---

#### `SubscriberMap_EventUpdate_AppIdNonMatch_NoDispatch`

**What it tests:** `EventUpdate("evtNoMatch", "{}", "ghostApp")` when the only subscriber has `appId="realApp"`. `gatewayMock->Emit` expected `Times(0)`.

---

#### `SubscriberMap_EventUpdate_NoSubscribersForKey_NoDispatch`

**What it tests:** `EventUpdate("unknownEvt", "{}", "")` with no subscribers. No crash, no dispatch. Uses `EXPECT_NO_THROW`.

---

### 9.13 Additional ThunderSubscriptionManager Tests

---

#### `ThunderManager_HandleNotifier_ThrowsException_PropagatesFromHandlerMock`

**What it tests:** If `HandleAppEventNotifier` throws a `std::runtime_error`, the exception propagates **out** of `HandleNotifier` (no try/catch). `EXPECT_THROW` verifies this.

**Ref-count note:** Because the exception fires before `internalNotifier->Release()`, the test must manually call `handlerMock->Release()` twice: once for the ref added by `AddRef()` in the `QueryInterfaceByCallsign` lambda, and once for the constructor's initial ref count of 1.

---

#### `ThunderManager_RegisterNotification_ThrowsException_Propagates`

**What it tests:** Same propagation test but through `RegisterNotification` (which calls `HandleNotifier`).

---

#### `ThunderManager_UnregisterNotification_HandlerReturnsFalseStatus_NotRemovedFromList`

**What it tests:** If `HandleNotifier` returns `false` during unregistration (handler sets `status=false`), the entry is **not** removed from `mRegisteredNotifications`. The notification remains "registered" from the manager's perspective.

---

#### `ThunderManager_Unsubscribe_WhenRegistered_CallsUnregister`

**What it tests:** Full `Subscribe` → `Unsubscribe` roundtrip at the `ThunderSubscriptionManager` level. Verifies that `HandleAppEventNotifier` is called with `listen=false` during unsubscription.

---

#### `ThunderManager_Unsubscribe_WhenNotRegistered_IsNoOp`

**What it tests:** `Unsubscribe("org.rdk.GhostMod", "ghostEvt")` when nothing was ever registered. `QueryInterfaceByCallsign` is expected `Times(0)` (never called). No crash.

---

#### `ThunderManager_HandleNotifier_HandlerReturnsErrorNoneButStatusFalse_ReturnsFalse`

**What it tests:** The distinction between the `hresult` return value and the `status` out-parameter. Even when `HandleAppEventNotifier` returns `ERROR_NONE`, if it sets `status=false`, `HandleNotifier` returns `false`. The `status` bool is the semantic return value.

---

### 9.14 Emitter + NotificationHandler Tests

These tests use the `NotificationHandler` helper class (a real `IEmitter` implementation with locking and `WaitForRequestStatus`).

---

#### `Notification_NotificationHandler_ReceivesEmit_DirectCall`

**What it tests:** Directly calling `handler->Emit("testEvent", "{\"key\":\"val\"}", "myApp")` on a `NotificationHandler` object signals the condition variable. `WaitForRequestStatus(1000, AppNotifications_OnEmit)` returns `true`, and all three stored fields match.

---

#### `Notification_NotificationHandler_Reset_ClearsStoredState`

**What it tests:** After `Emit` is called once and `WaitForRequestStatus` confirms it, calling `Reset()` clears the event mask and all stored strings. Subsequent getters return empty strings and mask returns 0.

---

#### `Notification_NotificationHandler_WaitTimeout_ReturnsFalse_WhenNoEmit`

**What it tests:** `WaitForRequestStatus(100, AppNotifications_OnEmit)` returns `false` when `Emit` was never called — the 100ms timeout expires.

---

#### `Notification_NotificationHandler_MultipleEmitCalls_LastValueStored`

**What it tests:** Two `Emit` calls in rapid succession. The event bit stays set after the first call. `WaitForRequestStatus` returns `true`. The stored values reflect the **last** `Emit` call (`"secondEvt"`, `"p2"`, `"a2"`).

---

### 9.15 HandleNotifier + Emitter Integration Tests

---

#### `Notification_HandleNotifier_PassesCorrectEmitterToHandler`

**What it tests:** Verifies that when `ThunderSubscriptionManager::HandleNotifier` calls `HandleAppEventNotifier`, the `IEmitter*` argument it passes is `&impl.mEmitter` — the plugin's own `Emitter` sink.

**How it works:** Uses a capturing lambda in `EXPECT_CALL` that stores the received `IEmitter*` into `capturedEmitter`. After the call, asserts `capturedEmitter == &impl.mEmitter`.

**Why it matters:** External plugins must receive the correct emitter pointer so their callbacks loop back into this plugin correctly.

---

#### `Notification_HandleNotifier_EmitterCanBeInvokedByHandler_TriggersEventUpdate`

**What it tests:** The full round-trip where an external plugin handler receives the `IEmitter*` and immediately calls `emitCb->Emit("cbEvent", "{\"from\":\"handler\"}", "cbApp")` during the `HandleAppEventNotifier` call. This triggers an `EmitJob` submission. After 150ms, the job runs, looks up the subscriber, and calls `gatewayMock->Emit`.

**Flow:**
```
test → HandleNotifier("org.rdk.CbMod", "cbEvent", true)
     → QueryInterfaceByCallsign → handlerMock
     → handlerMock→HandleAppEventNotifier(emitter, ...)
     → [inside mock lambda] emitter→Emit("cbEvent", ...) → EmitJob submitted
     → sleep 150ms
     → EmitJob runs → EventUpdate → DispatchToGateway → gatewayMock→Emit ✓
```

---

### 9.16 Full End-to-End Pipeline Tests

These are the most complex tests, exercising the complete path from `impl.Subscribe()` through `impl.mEmitter.Emit()` to `gatewayMock->Emit()`.

---

#### `Notification_EmitterEmit_WithSubscriber_GatewayOrigin_DispatchesViaGateway`

**What it tests:** A subscriber is added directly to `impl.mSubMap`, then `impl.mEmitter.Emit(...)` is called. The `EmitJob` runs, `EventUpdate` dispatches via gateway, `gatewayMock->Emit` is called exactly once.

---

#### `Notification_EmitterEmit_WithSubscriber_NonGatewayOrigin_DispatchesViaLaunchDelegate`

**What it tests:** Same flow but subscriber has delegate origin. Verifies the `DispatchToLaunchDelegate` path via the `Emitter`.

---

#### `Notification_EmitterEmit_NoSubscribers_NoDispatch`

**What it tests:** Calling `impl.mEmitter.Emit("orphanEvt", ...)` with no subscribers — neither gateway nor delegate `QueryInterfaceByCallsign` is called at all.

---

#### `Notification_EmitterEmit_EmptyAppId_BroadcastsToAllSubscribers`

**What it tests:** Two subscribers for `"broadcastEvt"`. `impl.mEmitter.Emit("broadcastEvt", "{\"data\":\"all\"}", "")` → both receive the event. `gatewayMock->Emit` expected `Times(2)`.

---

#### `Notification_EmitterEmit_AppIdFiltered_OnlyMatchingSubscriberDispatched`

**What it tests:** Two subscribers (`"targetApp"` and `"otherApp"`). `impl.mEmitter.Emit("filteredEvt", "{}", "targetApp")` → exactly **one** dispatch (`gatewayMock->Emit` expected `Times(1)`).

---

#### `Notification_EndToEnd_EmitterEmit_NotificationHandler_Receives`

**What it tests:** Exercises `NotificationHandler` as a direct `IEmitter` receiver. Calls `notifHandler->Emit(...)` and verifies all three stored fields.

---

#### `Notification_EndToEnd_SubscribeEmitViaEmitter_GatewayDispatch`

**What it tests:** The full pipeline:
1. `impl.Subscribe(ctx, true, "org.rdk.Pipeline", "pipelineEvt")` — submits `SubscriberJob`.
2. `impl.mEmitter.Emit("pipelineEvt", "{\"ok\":true}", "pipelineApp")` — submits `EmitJob`.
3. Sleep 150ms — both jobs drain.
4. `gatewayMock->Emit(_, "pipelineEvt", "{\"ok\":true}")` called exactly once.

**Race condition guard:** All `EXPECT_CALL`s are set up **before** `Subscribe()` because the async `SubscriberJob` can immediately call `QueryInterfaceByCallsign("org.rdk.Pipeline")`. A separate `EXPECT_CALL(...).WillRepeatedly(Return(nullptr))` absorbs those calls.

---

#### `Notification_EndToEnd_SubscribeEmitViaEmitter_LaunchDelegateDispatch`

**What it tests:** Same full pipeline but with delegate origin (`"org.rdk.LD"`). `ldMock->Emit(_, "ldEvt", "{\"ld\":1}")` expected once.

---

### 9.17 GatewayContext Field Verification Tests

These tests verify that the `Exchange::GatewayContext` struct passed to `gatewayMock->Emit` contains exactly the `requestId`, `connectionId`, and `appId` from the original `AppNotificationContext`.

---

#### `Notification_DispatchToGateway_GatewayContextFields_MatchSubscriberContext`

**What it tests:** A subscriber created with `MakeContext(77, 888, "fieldApp", GATEWAY_CALLSIGN)`. After `impl.mSubMap.EventUpdate("ctxFieldEvt", "{}", "fieldApp")`, a capturing lambda in `EXPECT_CALL(*gatewayMock, Emit(_, _, _))` stores the `GatewayContext`. Asserts:
- `capturedCtx.requestId == 77`
- `capturedCtx.connectionId == 888`
- `capturedCtx.appId == "fieldApp"`

**This is synchronous** — `EventUpdate` is called directly.

---

#### `Notification_DispatchToLaunchDelegate_GatewayContextFields_MatchSubscriberContext`

**What it tests:** Same field verification for the `DispatchToLaunchDelegate` path. Subscriber `MakeContext(55, 999, "ldFieldApp", DELEGATE_CALLSIGN)`. Asserts `requestId=55`, `connectionId=999`, `appId="ldFieldApp"`.

---

## 10. Complete Test Index

| # | Test Name | Group | Sync/Async |
|---|-----------|-------|------------|
| 1 | `Configure_StoresShellAndAddsRef` | Configure | Sync |
| 2 | `Configure_CalledTwice_ReleasesOldShellAndAddsRefNew` | Configure | Sync |
| 3 | `Subscribe_Listen_True_NewEvent_ReturnsNone` | Subscribe | Async |
| 4 | `Subscribe_Listen_True_SameEvent_TwiceNoExtraWorkerJob` | Subscribe | Async |
| 5 | `Subscribe_Listen_True_AddsContextToMap` | Subscribe | Async |
| 6 | `Subscribe_Listen_True_MixedCaseEvent_KeyIsLowercased` | Subscribe | Async |
| 7 | `Subscribe_Listen_False_RemovesContextFromMap` | Subscribe | Async |
| 8 | `Subscribe_Listen_False_LastContext_EmitsUnsubscribeJob` | Subscribe | Async |
| 9 | `Subscribe_Listen_False_NonExistentEvent_ReturnsNone` | Subscribe | Sync |
| 10 | `Subscribe_Listen_False_OneOfTwoContextsRemoved` | Subscribe | Async |
| 11 | `Subscribe_MultipleDistinctEvents_AllPresentInMap` | Subscribe | Async |
| 12 | `Emit_ReturnsNoneImmediately` | Emit | Sync |
| 13 | `Emit_EmptyPayload_ReturnsNone` | Emit | Sync |
| 14 | `Emit_EmptyAppId_ReturnsNone` | Emit | Sync |
| 15 | `Emit_EmptyEvent_ReturnsNone` | Emit | Sync |
| 16 | `Emit_WithSubscriber_GatewayOrigin_DispatchesToGateway` | Emit | Async (80ms) |
| 17 | `Emit_WithSubscriber_NonGatewayOrigin_DispatchesToLaunchDelegate` | Emit | Async (80ms) |
| 18 | `Emit_NoSubscribersForEvent_NoDispatch` | Emit | Async (80ms) |
| 19 | `Emit_WithAppIdFilter_OnlyDispatchesMatchingAppId` | Emit | Async (80ms) |
| 20 | `Emit_GatewayQueryFails_DoesNotCrash` | Emit | Async (80ms) |
| 21 | `Emit_LaunchDelegateQueryFails_DoesNotCrash` | Emit | Async (80ms) |
| 22 | `Cleanup_RemovesAllContextsMatchingConnectionIdAndOrigin` | Cleanup | Sync |
| 23 | `Cleanup_ConnectionIdNotPresent_NoChange` | Cleanup | Sync |
| 24 | `Cleanup_OriginMismatch_NoChange` | Cleanup | Sync |
| 25 | `Cleanup_EmptyMap_ReturnsNone` | Cleanup | Sync |
| 26 | `Cleanup_MultipleEventsForConnection_AllCleared` | Cleanup | Sync |
| 27 | `Cleanup_ThenSubscribe_WorksCorrectly` | Cleanup | Async |
| 28 | `SubscriberMap_Add_And_Exists` | SubMap | Sync |
| 29 | `SubscriberMap_Get_ReturnsCorrectContexts` | SubMap | Sync |
| 30 | `SubscriberMap_Get_NonExistentKey_ReturnsEmpty` | SubMap | Sync |
| 31 | `SubscriberMap_Remove_ExistingContext_KeyErasedWhenEmpty` | SubMap | Sync |
| 32 | `SubscriberMap_Remove_NonExistentKey_NoOp` | SubMap | Sync |
| 33 | `SubscriberMap_Exists_CaseInsensitive` | SubMap | Sync |
| 34 | `SubscriberMap_CleanupNotifications_ByConnectionAndOrigin` | SubMap | Sync |
| 35 | `SubscriberMap_CleanupNotifications_ErasesKeyWhenAllRemoved` | SubMap | Sync |
| 36 | `ThunderManager_IsNotificationRegistered_FalseByDefault` | ThunderMgr | Sync |
| 37 | `ThunderManager_RegisterNotification_WhenHandlerAvailable` | ThunderMgr | Sync |
| 38 | `ThunderManager_RegisterNotification_WhenHandlerUnavailable_NotRegistered` | ThunderMgr | Sync |
| 39 | `ThunderManager_RegisterNotification_HandlerReturnsFalseStatus_NotRegistered` | ThunderMgr | Sync |
| 40 | `ThunderManager_UnregisterNotification_WhenRegistered` | ThunderMgr | Sync |
| 41 | `ThunderManager_UnregisterNotification_WhenNotRegistered_NoOp` | ThunderMgr | Sync |
| 42 | `ThunderManager_Subscribe_AlreadyRegistered_NoDuplicateRegistration` | ThunderMgr | Sync |
| 43 | `ThunderManager_HandleNotifier_HandlerReturnsError_ReturnsFalse` | ThunderMgr | Sync |
| 44 | `ThunderManager_HandleNotifier_ModuleNotAvailable_ReturnsFalse` | ThunderMgr | Sync |
| 45 | `Emitter_Emit_SubmitsEmitJobToWorkerPool` | Emitter | Async (60ms) |
| 46 | `Emitter_Emit_EmptyArguments_NocrashNoThrow` | Emitter | Async (60ms) |
| 47 | `EndToEnd_SubscribeEmitCleanup_GatewayOrigin` | E2E | Async (80ms) |
| 48 | `EndToEnd_MultipleSubscribersEmitOneAppId` | E2E | Async (80ms) |
| 49 | `Subscribe_EmptyModule_Listen_True_ReturnsNoneAndAddsToMap` | Subscribe+ | Async |
| 50 | `Subscribe_EmptyEvent_Listen_True_ReturnsNoneAndAddsToMap` | Subscribe+ | Async |
| 51 | `Subscribe_PartialUnsubscribe_ThenResubscribe_Works` | Subscribe+ | Async |
| 52 | `Subscribe_Listen_False_EmptyEvent_NoEntry_ReturnsNone` | Subscribe+ | Sync |
| 53 | `Emit_AfterCleanup_NoDispatch` | Emit+ | Async (80ms) |
| 54 | `Emit_EmptyAppId_DispatchesToAllSubscribers` | Emit+ | Async (80ms) |
| 55 | `Emit_AppIdNoMatch_NoDispatch` | Emit+ | Async (80ms) |
| 56 | `Cleanup_EmptyOrigin_OnlyRemovesEmptyOriginContexts` | Cleanup+ | Sync |
| 57 | `Cleanup_ZeroConnectionId_OnlyRemovesMatchingConnId` | Cleanup+ | Sync |
| 58 | `Cleanup_BothOriginAndConnIdMustMatch_ConnIdMatchOnly_NoRemoval` | Cleanup+ | Sync |
| 59 | `SubscriberMap_Remove_OnlyRemovesOneMatchingContext_WhenMultipleSameKey` | SubMap+ | Sync |
| 60 | `SubscriberMap_CleanupNotifications_ConnIdMatchOnly_OriginMismatch_NoRemoval` | SubMap+ | Sync |
| 61 | `SubscriberMap_DispatchToGateway_CachesInterface_QueryCalledOnce` | SubMap+ | Sync |
| 62 | `SubscriberMap_DispatchToGateway_NullGateway_DoesNotCrash` | SubMap+ | Sync |
| 63 | `SubscriberMap_DispatchToLaunchDelegate_NullDelegate_DoesNotCrash` | SubMap+ | Sync |
| 64 | `SubscriberMap_DispatchToLaunchDelegate_CachesInterface_QueryCalledOnce` | SubMap+ | Sync |
| 65 | `SubscriberMap_EventUpdate_AppIdEmpty_DispatchesToAllSubscribers` | SubMap+ | Sync |
| 66 | `SubscriberMap_EventUpdate_AppIdNonMatch_NoDispatch` | SubMap+ | Sync |
| 67 | `SubscriberMap_EventUpdate_NoSubscribersForKey_NoDispatch` | SubMap+ | Sync |
| 68 | `ThunderManager_HandleNotifier_ThrowsException_PropagatesFromHandlerMock` | ThunderMgr+ | Sync |
| 69 | `ThunderManager_RegisterNotification_ThrowsException_Propagates` | ThunderMgr+ | Sync |
| 70 | `ThunderManager_UnregisterNotification_HandlerReturnsFalseStatus_NotRemovedFromList` | ThunderMgr+ | Sync |
| 71 | `ThunderManager_Unsubscribe_WhenRegistered_CallsUnregister` | ThunderMgr+ | Sync |
| 72 | `ThunderManager_Unsubscribe_WhenNotRegistered_IsNoOp` | ThunderMgr+ | Sync |
| 73 | `ThunderManager_HandleNotifier_HandlerReturnsErrorNoneButStatusFalse_ReturnsFalse` | ThunderMgr+ | Sync |
| 74 | `Notification_NotificationHandler_ReceivesEmit_DirectCall` | Handler | Sync |
| 75 | `Notification_NotificationHandler_Reset_ClearsStoredState` | Handler | Sync |
| 76 | `Notification_NotificationHandler_WaitTimeout_ReturnsFalse_WhenNoEmit` | Handler | Sync |
| 77 | `Notification_NotificationHandler_MultipleEmitCalls_LastValueStored` | Handler | Sync |
| 78 | `Notification_HandleNotifier_PassesCorrectEmitterToHandler` | Handler+HN | Sync |
| 79 | `Notification_HandleNotifier_EmitterCanBeInvokedByHandler_TriggersEventUpdate` | Handler+HN | Async (150ms) |
| 80 | `Notification_EmitterEmit_WithSubscriber_GatewayOrigin_DispatchesViaGateway` | Pipeline | Async (150ms) |
| 81 | `Notification_EmitterEmit_WithSubscriber_NonGatewayOrigin_DispatchesViaLaunchDelegate` | Pipeline | Async (150ms) |
| 82 | `Notification_EmitterEmit_NoSubscribers_NoDispatch` | Pipeline | Async (150ms) |
| 83 | `Notification_EmitterEmit_EmptyAppId_BroadcastsToAllSubscribers` | Pipeline | Async (150ms) |
| 84 | `Notification_EmitterEmit_AppIdFiltered_OnlyMatchingSubscriberDispatched` | Pipeline | Async (150ms) |
| 85 | `Notification_EndToEnd_EmitterEmit_NotificationHandler_Receives` | Pipeline | Sync |
| 86 | `Notification_EndToEnd_SubscribeEmitViaEmitter_GatewayDispatch` | Pipeline | Async (150ms) |
| 87 | `Notification_EndToEnd_SubscribeEmitViaEmitter_LaunchDelegateDispatch` | Pipeline | Async (150ms) |
| 88 | `Notification_DispatchToGateway_GatewayContextFields_MatchSubscriberContext` | CtxFields | Sync |
| 89 | `Notification_DispatchToLaunchDelegate_GatewayContextFields_MatchSubscriberContext` | CtxFields | Sync |

---

## 11. Key Design Decisions and Gotchas

### 11.1 `#define private public` — accessing internals safely

The test uses `#define private public` to access `mShell`, `mSubMap`, `mThunderManager`, and `mEmitter`. This is a compile-time trick only. The ABI and memory layout are unchanged. It does **not** affect production builds.

### 11.2 Member declaration order in the fixture

`service` must be declared before `impl`. C++ destroys in reverse order, so `impl` is destroyed first (calls `mShell->Release()` on `service`), and then `service` is destroyed. Reversing this order would be a use-after-free.

### 11.3 GMock LIFO expectation matching

When two `EXPECT_CALL`s match the same call (e.g. a wildcard `(_, _)` and a specific `(_, StrEq("org.rdk.AppGateway"))`), GMock uses the **most recently registered** expectation first. Tests register the catch-all first and the specific one second, so the specific one wins for matching calls.

### 11.4 Race conditions in end-to-end tests

`Subscribe()` submits a `SubscriberJob` asynchronously. In end-to-end tests that also check behaviour right after `Subscribe()`, the background job may call `QueryInterfaceByCallsign` before or after the `EXPECT_CALL` is set up. The fix is to set up **all** `EXPECT_CALL`s before calling `Subscribe()`.

### 11.5 Interface caching in SubscriberMap

Both `mAppGateway` and `mInternalGatewayNotifier` are lazy-initialized and cached on the first dispatch. Subsequent dispatches reuse them. Tests verify this with `Times(1)` on `QueryInterfaceByCallsign`. The `SubscriberMap` destructor releases these cached pointers, so tests must call `gatewayMock->Release()` to balance the initial test-side `AddRef` from the `QueryInterfaceByCallsign` lambda.

### 11.6 Destructor safety — `ClearRegistrations()`

The `ThunderSubscriptionManager` destructor iterates `mRegisteredNotifications` and calls `HandleNotifier(module, event, false)` for each. `HandleNotifier` calls `mParent.mShell->QueryInterfaceByCallsign(...)`. If `mShell` is already null when the destructor runs, this is undefined behaviour.

The fix applied in `AppNotificationsImplementation::~AppNotificationsImplementation()`:

```cpp
mThunderManager.ClearRegistrations();  // discard list under mutex
if (mShell != nullptr) {
    mShell->Release();
    mShell = nullptr;
}
// C++ member dtors run here: mThunderManager dtor loop is now a no-op
```

### 11.7 `ThunderManager_HandleNotifier_ThrowsException` — manual ref counting

When `HandleAppEventNotifier` throws before `internalNotifier->Release()` is reached, the `AddRef()` from `QueryInterfaceByCallsign` leaks. The test compensates by calling `handlerMock->Release()` twice manually. This is documented explicitly in the test comment.

### 11.8 `std::remove` removes ALL equal elements

`SubscriberMap::Remove` uses `std::remove(begin, end, context)`, which removes every element equal to `context` in a single pass. If two identical contexts were added (same `requestId`, `connectionId`, `appId`, `origin`, `version`), both are removed by one `Remove` call. Test #59 (`SubscriberMap_Remove_OnlyRemovesOneMatchingContext_WhenMultipleSameKey`) documents and verifies this behaviour.

### 11.9 `IsOriginGateway` routing

`SubscriberMap::Dispatch` routes to `DispatchToGateway` or `DispatchToLaunchDelegate` based on `ContextUtils::IsOriginGateway(context.origin)`. The tests use the constants:

```cpp
#define APP_NOTIFICATIONS_GATEWAY_CALLSIGN  "org.rdk.AppGateway"
#define APP_NOTIFICATIONS_DELEGATE_CALLSIGN "org.rdk.LaunchDelegate"
```

A context with `origin = "org.rdk.AppGateway"` routes to the gateway path; anything else routes to the delegate path.
