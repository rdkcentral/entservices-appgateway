# AppNotifications Plugin — L0 Unit Test Cases Document

## Table of Contents

1. [Overview](#overview)
2. [Plugin Architecture Summary](#plugin-architecture-summary)
3. [Source Files Under Test](#source-files-under-test)
4. [Complete Method Inventory](#complete-method-inventory)
5. [Quick Reference — Test Case Summary Table](#quick-reference--test-case-summary-table)
6. [Detailed Test Cases](#detailed-test-cases)
   - [AppNotifications.cpp (Plugin Shell)](#appnotificationscpp-plugin-shell)
   - [AppNotificationsImplementation.cpp (Core Logic)](#appnotificationsimplementationcpp-core-logic)
   - [SubscriberMap (Inner Class)](#subscribermap-inner-class)
   - [ThunderSubscriptionManager (Inner Class)](#thundersubscriptionmanager-inner-class)
   - [SubscriberJob / EmitJob / Emitter (Dispatch Classes)](#subscriberjob--emitjob--emitter-dispatch-classes)
7. [Coverage Analysis](#coverage-analysis)
8. [Uncovered / Partially Covered Methods](#uncovered--partially-covered-methods)
9. [L0 Test Code Generation Guide (Reference from AppGateway L0)](#l0-test-code-generation-guide-reference-from-appgateway-l0)
10. [Proposed File Structure and CMakeLists.txt](#proposed-file-structure-and-cmakeliststxt)

---

## Overview

This document defines comprehensive L0 (unit-level) test cases for the **AppNotifications** plugin in the `entservices-appgateway` repository. The goal is to achieve **minimum 75% code coverage** across all `.cpp` and `.h` files of the plugin. Test cases cover **positive**, **negative**, and **boundary** scenarios.

L0 tests run **in-process** without a real Thunder host, using mock/fake implementations for external dependencies (`IShell`, `IAppGatewayResponder`, `IAppNotificationHandler`). This approach is identical to the pattern used by the existing **AppGateway** L0 test suite under `Tests/L0Tests/AppGateway/`.

---

## Plugin Architecture Summary

The AppNotifications plugin consists of two layers:

| Layer | File | Role |
|-------|------|------|
| **Plugin Shell** | `AppNotifications.cpp` / `AppNotifications.h` | Thunder IPlugin lifecycle (Initialize/Deinitialize), JSONRPC dispatcher, COM aggregation of `IAppNotifications` |
| **Implementation** | `AppNotificationsImplementation.cpp` / `AppNotificationsImplementation.h` | Core business logic: subscription management, event emission, Thunder notification bridging, gateway dispatch |
| **Module** | `Module.cpp` / `Module.h` | MODULE_NAME_DECLARATION boilerplate |

Key inner classes within `AppNotificationsImplementation`:

| Inner Class | Purpose |
|-------------|---------|
| `SubscriberMap` | Thread-safe subscriber registry; event dispatch to AppGateway or InternalGateway (LaunchDelegate) |
| `ThunderSubscriptionManager` | Manages Thunder-side notification subscriptions via `IAppNotificationHandler` |
| `SubscriberJob` | `Core::IDispatch` job for async Subscribe/Unsubscribe |
| `EmitJob` | `Core::IDispatch` job for async Emit (event fanout) |
| `Emitter` | `IAppNotificationHandler::IEmitter` implementation; bridges handler callbacks back to Emit pipeline |

---

## Source Files Under Test

| # | File | Path | LOC | Type |
|---|------|------|-----|------|
| 1 | `AppNotifications.h` | `AppNotifications/AppNotifications.h` | 70 | Header |
| 2 | `AppNotifications.cpp` | `AppNotifications/AppNotifications.cpp` | 128 | Source |
| 3 | `AppNotificationsImplementation.h` | `AppNotifications/AppNotificationsImplementation.h` | 255 | Header |
| 4 | `AppNotificationsImplementation.cpp` | `AppNotifications/AppNotificationsImplementation.cpp` | 305 | Source |
| 5 | `Module.h` | `AppNotifications/Module.h` | 29 | Header |
| 6 | `Module.cpp` | `AppNotifications/Module.cpp` | 22 | Source |

**Total production code**: ~809 lines across 6 files.

---

## Complete Method Inventory

### AppNotifications.cpp (Plugin Shell) — 5 methods

| # | Method | File:Line | Testable | Notes |
|---|--------|-----------|----------|-------|
| 1 | `AppNotifications()` (constructor) | `AppNotifications.cpp:46` | Yes | Verifies member initialization |
| 2 | `~AppNotifications()` (destructor) | `AppNotifications.cpp:51` | Yes | Via scope exit |
| 3 | `Initialize(IShell*)` | `AppNotifications.cpp:56` | Yes | Core lifecycle — success path, failure path (null implementation) |
| 4 | `Deinitialize(IShell*)` | `AppNotifications.cpp:84` | Yes | Cleanup, RemoteConnection teardown |
| 5 | `Deactivated(IRemoteConnection*)` | `AppNotifications.cpp:117` | Yes | Connection-ID match and mismatch |

### AppNotificationsImplementation.cpp (Core Logic) — 5 methods

| # | Method | File:Line | Testable | Notes |
|---|--------|-----------|----------|-------|
| 6 | `AppNotificationsImplementation()` (constructor) | `Impl.cpp:38` | Yes | Member initialization |
| 7 | `~AppNotificationsImplementation()` (destructor) | `Impl.cpp:46` | Yes | Shell release path, null shell path |
| 8 | `Subscribe(context, listen, module, event)` | `Impl.cpp:56` | Yes | Add/Remove, first subscribe triggers Thunder sub, last unsub triggers Thunder unsub |
| 9 | `Emit(event, payload, appId)` | `Impl.cpp:81` | Yes | Async job dispatch |
| 10 | `Cleanup(connectionId, origin)` | `Impl.cpp:90` | Yes | Remove subscribers by connection |
| 11 | `Configure(IShell*)` | `Impl.cpp:112` | Yes | Shell acquisition |

### SubscriberMap — 8 methods

| # | Method | File:Line | Testable | Notes |
|---|--------|-----------|----------|-------|
| 12 | `SubscriberMap(parent)` (constructor) | `Impl.h:37` | Yes | Initialization |
| 13 | `~SubscriberMap()` (destructor) | `Impl.h:43` | Yes | Cleanup of mAppGateway, mInternalGatewayNotifier |
| 14 | `Add(key, context)` | `Impl.cpp:122` | Yes | Case-insensitive key, vector push |
| 15 | `Remove(key, context)` | `Impl.cpp:128` | Yes | Remove matching context, auto-erase empty |
| 16 | `Get(key)` | `Impl.cpp:141` | Yes | Return contexts for key, empty if missing |
| 17 | `Exists(key)` | `Impl.cpp:151` | Yes | Case-insensitive lookup |
| 18 | `EventUpdate(key, payload, appId)` | `Impl.cpp:158` | Yes | Dispatch to matching subscribers; appId filtering; no-listener warning |
| 19 | `Dispatch(key, context, payload)` | `Impl.cpp:186` | Yes | Route to Gateway vs LaunchDelegate based on origin |
| 20 | `DispatchToGateway(key, context, payload)` | `Impl.cpp:194` | Yes | Lazy-acquire IAppGatewayResponder, Emit |
| 21 | `DispatchToLaunchDelegate(key, context, payload)` | `Impl.cpp:209` | Yes | Lazy-acquire InternalGateway, Emit |
| 22 | `CleanupNotifications(connectionId, origin)` | `Impl.cpp:96` | Yes | Remove by connection+origin, erase empty entries |

### ThunderSubscriptionManager — 7 methods

| # | Method | File:Line | Testable | Notes |
|---|--------|-----------|----------|-------|
| 23 | `~ThunderSubscriptionManager()` (destructor) | `Impl.cpp:224` | Yes | Unsubscribe all registered notifications |
| 24 | `Subscribe(module, event)` | `Impl.cpp:238` | Yes | Skip if already registered, else RegisterNotification |
| 25 | `Unsubscribe(module, event)` | `Impl.cpp:251` | Yes | Skip if not registered, else UnregisterNotification |
| 26 | `HandleNotifier(module, event, listen)` | `Impl.cpp:264` | Yes | Query IAppNotificationHandler, call HandleAppEventNotifier |
| 27 | `RegisterNotification(module, event)` | `Impl.cpp:281` | Yes | Call HandleNotifier(true), add to registry on success |
| 28 | `UnregisterNotification(module, event)` | `Impl.cpp:290` | Yes | Call HandleNotifier(false), remove from registry on success |
| 29 | `IsNotificationRegistered(module, notification)` | `Impl.cpp:298` | Yes | Case-insensitive lookup |

### Dispatch Jobs & Emitter — 5 methods (inline in header)

| # | Method | File:Line | Testable | Notes |
|---|--------|-----------|----------|-------|
| 30 | `SubscriberJob::Dispatch()` | `Impl.h:177` | Yes | Calls Subscribe or Unsubscribe on ThunderManager |
| 31 | `SubscriberJob::Create()` | `Impl.h:171` | Yes | Factory method |
| 32 | `EmitJob::Dispatch()` | `Impl.h:212` | Yes | Calls EventUpdate on SubscriberMap |
| 33 | `EmitJob::Create()` | `Impl.h:206` | Yes | Factory method |
| 34 | `Emitter::Emit()` | `Impl.h:231` | Yes | Submits EmitJob to WorkerPool |

### Operator Overload

| # | Method | File:Line | Testable | Notes |
|---|--------|-----------|----------|-------|
| 35 | `operator==(AppNotificationContext, AppNotificationContext)` | `Impl.cpp:25` | Yes | Equality check for all 5 fields |

**Total testable methods: 35**

---

## Quick Reference — Test Case Summary Table

| Test ID | Test Name | Source File Covered | Method(s) Covered | Scenario | Expected Result |
|---------|-----------|--------------------|--------------------|----------|-----------------|
| **AN-L0-001** | Initialize_Success | `AppNotifications.cpp` | `Initialize` | Valid IShell, implementation instantiates successfully | Returns empty string (no error) |
| **AN-L0-002** | Initialize_FailNullImpl | `AppNotifications.cpp` | `Initialize` | IShell::Root returns nullptr | Returns error string |
| **AN-L0-003** | Initialize_ConfigureInterface | `AppNotifications.cpp` | `Initialize` | Implementation supports IConfiguration | Configure() called on impl, returns empty string |
| **AN-L0-004** | Deinitialize_HappyPath | `AppNotifications.cpp` | `Deinitialize` | Normal deinit after successful init | No crash, mAppNotifications released |
| **AN-L0-005** | Deinitialize_NullImpl | `AppNotifications.cpp` | `Deinitialize` | mAppNotifications is nullptr (init failed) | No crash, graceful cleanup |
| **AN-L0-006** | Deinitialize_WithRemoteConnection | `AppNotifications.cpp` | `Deinitialize` | RemoteConnection non-null | Terminate + Release called |
| **AN-L0-007** | Deactivated_MatchingConnectionId | `AppNotifications.cpp` | `Deactivated` | connection->Id() matches mConnectionId | Submits DEACTIVATED job |
| **AN-L0-008** | Deactivated_NonMatchingConnectionId | `AppNotifications.cpp` | `Deactivated` | connection->Id() does not match | No job submitted |
| **AN-L0-009** | Constructor_Destructor_Lifecycle | `AppNotifications.cpp` | ctor/dtor | Create and destroy | No crash, SYSLOG messages emitted |
| **AN-L0-010** | Impl_Constructor_MemberInit | `AppNotificationsImpl.cpp` | ctor | Construct implementation | mShell == nullptr, sub-objects initialized |
| **AN-L0-011** | Impl_Destructor_ShellRelease | `AppNotificationsImpl.cpp` | dtor | Destroy after Configure | mShell released |
| **AN-L0-012** | Impl_Destructor_NullShell | `AppNotificationsImpl.cpp` | dtor | Destroy without Configure | No crash |
| **AN-L0-013** | Configure_Success | `AppNotificationsImpl.cpp` | `Configure` | Valid IShell pointer | Returns ERROR_NONE, shell stored+AddRef'd |
| **AN-L0-014** | Subscribe_FirstListener_TriggersThunderSub | `AppNotificationsImpl.cpp` | `Subscribe` | First subscriber for event "onFoo" | SubscriberJob submitted (subscribe=true), context added to map |
| **AN-L0-015** | Subscribe_SecondListener_NoThunderSub | `AppNotificationsImpl.cpp` | `Subscribe` | Second subscriber for same event | No new SubscriberJob, context added |
| **AN-L0-016** | Subscribe_CaseInsensitive | `AppNotificationsImpl.cpp` | `Subscribe`, `SubscriberMap::Add` | Subscribe with "OnFoo" then check "onfoo" | Both stored under same lowercase key |
| **AN-L0-017** | Unsubscribe_LastListener_TriggersThunderUnsub | `AppNotificationsImpl.cpp` | `Subscribe(listen=false)` | Remove last subscriber for event | SubscriberJob submitted (subscribe=false) |
| **AN-L0-018** | Unsubscribe_NotLastListener_NoThunderUnsub | `AppNotificationsImpl.cpp` | `Subscribe(listen=false)` | Remove one of two subscribers | No SubscriberJob |
| **AN-L0-019** | Unsubscribe_NonExistent_NoCrash | `AppNotificationsImpl.cpp` | `Subscribe(listen=false)` | Remove context not in map | Returns ERROR_NONE, no crash |
| **AN-L0-020** | Emit_SubmitsJob | `AppNotificationsImpl.cpp` | `Emit` | Valid event, payload, appId | EmitJob submitted, returns ERROR_NONE |
| **AN-L0-021** | Emit_EmptyPayload | `AppNotificationsImpl.cpp` | `Emit` | Empty payload string | Returns ERROR_NONE, job dispatched |
| **AN-L0-022** | Emit_EmptyAppId | `AppNotificationsImpl.cpp` | `Emit` | appId is empty | Returns ERROR_NONE (broadcast to all) |
| **AN-L0-023** | Cleanup_RemovesMatchingSubscribers | `AppNotificationsImpl.cpp` | `Cleanup`, `CleanupNotifications` | connectionId=10, origin="org.rdk.AppGateway" | Matching subscribers removed, others retained |
| **AN-L0-024** | Cleanup_EmptiesEntireKey | `AppNotificationsImpl.cpp` | `Cleanup`, `CleanupNotifications` | Only subscriber for event matches | Event key erased from map |
| **AN-L0-025** | Cleanup_NoMatch_NoCrash | `AppNotificationsImpl.cpp` | `Cleanup`, `CleanupNotifications` | No subscribers match | Map unchanged, no crash |
| **AN-L0-026** | Cleanup_MultipleEvents | `AppNotificationsImpl.cpp` | `Cleanup`, `CleanupNotifications` | Multiple events have matching subscribers | All matching entries removed across events |
| **AN-L0-027** | SubscriberMap_Add_NewKey | `AppNotificationsImpl.cpp` | `Add` | New key | Vector created with single context |
| **AN-L0-028** | SubscriberMap_Add_ExistingKey | `AppNotificationsImpl.cpp` | `Add` | Existing key | Context appended to vector |
| **AN-L0-029** | SubscriberMap_Remove_ExistingContext | `AppNotificationsImpl.cpp` | `Remove` | Context exists | Removed from vector |
| **AN-L0-030** | SubscriberMap_Remove_LastContext_ErasesKey | `AppNotificationsImpl.cpp` | `Remove` | Last context for key | Key erased from map |
| **AN-L0-031** | SubscriberMap_Remove_NonExistent_NoCrash | `AppNotificationsImpl.cpp` | `Remove` | Context not in map | No-op, no crash |
| **AN-L0-032** | SubscriberMap_Get_Existing | `AppNotificationsImpl.cpp` | `Get` | Key exists | Returns vector of contexts |
| **AN-L0-033** | SubscriberMap_Get_NonExistent | `AppNotificationsImpl.cpp` | `Get` | Key does not exist | Returns empty vector |
| **AN-L0-034** | SubscriberMap_Exists_True | `AppNotificationsImpl.cpp` | `Exists` | Key present | Returns true |
| **AN-L0-035** | SubscriberMap_Exists_False | `AppNotificationsImpl.cpp` | `Exists` | Key absent | Returns false |
| **AN-L0-036** | SubscriberMap_Exists_CaseInsensitive | `AppNotificationsImpl.cpp` | `Exists` | Add "OnFoo", check "onfoo" | Returns true |
| **AN-L0-037** | EventUpdate_DispatchToAll_EmptyAppId | `AppNotificationsImpl.cpp` | `EventUpdate`, `Dispatch` | appId="" — broadcast | All subscribers for event receive dispatch |
| **AN-L0-038** | EventUpdate_FilterByAppId | `AppNotificationsImpl.cpp` | `EventUpdate` | appId="com.app.one" | Only matching appId subscriber dispatched |
| **AN-L0-039** | EventUpdate_NoListeners_LogWarning | `AppNotificationsImpl.cpp` | `EventUpdate` | Event has no subscribers | LOGWARN emitted, no crash |
| **AN-L0-040** | EventUpdate_VersionedEventKey | `AppNotificationsImpl.cpp` | `EventUpdate` | Event key "onFoo.v8" | clearKey strips suffix, dispatches "onFoo" |
| **AN-L0-041** | Dispatch_OriginGateway | `AppNotificationsImpl.cpp` | `Dispatch`, `DispatchToGateway` | context.origin == APP_GATEWAY_CALLSIGN | DispatchToGateway path executed |
| **AN-L0-042** | Dispatch_OriginNonGateway | `AppNotificationsImpl.cpp` | `Dispatch`, `DispatchToLaunchDelegate` | context.origin != APP_GATEWAY_CALLSIGN | DispatchToLaunchDelegate path executed |
| **AN-L0-043** | DispatchToGateway_LazyAcquire_Success | `AppNotificationsImpl.cpp` | `DispatchToGateway` | mAppGateway is null, shell returns valid responder | Interface acquired, Emit called |
| **AN-L0-044** | DispatchToGateway_LazyAcquire_Failure | `AppNotificationsImpl.cpp` | `DispatchToGateway` | Shell returns nullptr for AppGateway | LOGERR, early return, no crash |
| **AN-L0-045** | DispatchToGateway_AlreadyAcquired | `AppNotificationsImpl.cpp` | `DispatchToGateway` | mAppGateway already set | Skips acquire, Emit called |
| **AN-L0-046** | DispatchToLaunchDelegate_LazyAcquire_Success | `AppNotificationsImpl.cpp` | `DispatchToLaunchDelegate` | mInternalGatewayNotifier null, shell returns valid | Interface acquired, Emit called |
| **AN-L0-047** | DispatchToLaunchDelegate_LazyAcquire_Failure | `AppNotificationsImpl.cpp` | `DispatchToLaunchDelegate` | Shell returns nullptr for InternalGateway | LOGERR, early return, no crash |
| **AN-L0-048** | DispatchToLaunchDelegate_AlreadyAcquired | `AppNotificationsImpl.cpp` | `DispatchToLaunchDelegate` | Interface already cached | Skips acquire, Emit called |
| **AN-L0-049** | ThunderMgr_Subscribe_NewEvent | `AppNotificationsImpl.cpp` | `ThunderSubscriptionManager::Subscribe`, `RegisterNotification` | Event not previously registered | HandleNotifier called with listen=true |
| **AN-L0-050** | ThunderMgr_Subscribe_AlreadyRegistered | `AppNotificationsImpl.cpp` | `ThunderSubscriptionManager::Subscribe` | Event already registered | LOGTRACE, skip |
| **AN-L0-051** | ThunderMgr_Unsubscribe_RegisteredEvent | `AppNotificationsImpl.cpp` | `ThunderSubscriptionManager::Unsubscribe`, `UnregisterNotification` | Event previously registered | HandleNotifier called with listen=false |
| **AN-L0-052** | ThunderMgr_Unsubscribe_NotRegistered | `AppNotificationsImpl.cpp` | `ThunderSubscriptionManager::Unsubscribe` | Event not registered | LOGERR, skip |
| **AN-L0-053** | HandleNotifier_Success | `AppNotificationsImpl.cpp` | `HandleNotifier` | IAppNotificationHandler available, returns ERROR_NONE | Returns status from handler |
| **AN-L0-054** | HandleNotifier_HandlerNotAvailable | `AppNotificationsImpl.cpp` | `HandleNotifier` | QueryInterfaceByCallsign returns nullptr | LOGERR, returns false |
| **AN-L0-055** | HandleNotifier_HandlerReturnsError | `AppNotificationsImpl.cpp` | `HandleNotifier` | HandleAppEventNotifier returns error | LOGERR "subscription failure", returns false |
| **AN-L0-056** | RegisterNotification_HandleNotifier_ReturnsTrue | `AppNotificationsImpl.cpp` | `RegisterNotification` | HandleNotifier returns true | Added to mRegisteredNotifications |
| **AN-L0-057** | RegisterNotification_HandleNotifier_ReturnsFalse | `AppNotificationsImpl.cpp` | `RegisterNotification` | HandleNotifier returns false | Not added to mRegisteredNotifications |
| **AN-L0-058** | UnregisterNotification_HandleNotifier_ReturnsTrue | `AppNotificationsImpl.cpp` | `UnregisterNotification` | HandleNotifier returns true | Removed from mRegisteredNotifications |
| **AN-L0-059** | UnregisterNotification_HandleNotifier_ReturnsFalse | `AppNotificationsImpl.cpp` | `UnregisterNotification` | HandleNotifier returns false | Not removed from mRegisteredNotifications |
| **AN-L0-060** | IsNotificationRegistered_Exists | `AppNotificationsImpl.cpp` | `IsNotificationRegistered` | Module+event previously registered | Returns true |
| **AN-L0-061** | IsNotificationRegistered_NotExists | `AppNotificationsImpl.cpp` | `IsNotificationRegistered` | Module+event not registered | Returns false |
| **AN-L0-062** | IsNotificationRegistered_CaseInsensitive | `AppNotificationsImpl.cpp` | `IsNotificationRegistered` | Registered with "OnFoo", check with "onfoo" | Returns true |
| **AN-L0-063** | ThunderMgr_Destructor_UnsubscribesAll | `AppNotificationsImpl.cpp` | `~ThunderSubscriptionManager` | Destructor with 2 registered notifications | HandleNotifier(listen=false) called for each |
| **AN-L0-064** | SubscriberJob_Dispatch_Subscribe | `AppNotificationsImpl.h` | `SubscriberJob::Dispatch` | mSubscribe=true | ThunderManager.Subscribe called |
| **AN-L0-065** | SubscriberJob_Dispatch_Unsubscribe | `AppNotificationsImpl.h` | `SubscriberJob::Dispatch` | mSubscribe=false | ThunderManager.Unsubscribe called |
| **AN-L0-066** | EmitJob_Dispatch | `AppNotificationsImpl.h` | `EmitJob::Dispatch` | Normal dispatch | SubscriberMap.EventUpdate called |
| **AN-L0-067** | Emitter_Emit_SubmitsJob | `AppNotificationsImpl.h` | `Emitter::Emit` | Valid params | EmitJob submitted to WorkerPool |
| **AN-L0-068** | AppNotificationContext_Equality | `AppNotificationsImpl.cpp` | `operator==` | All 5 fields match | Returns true |
| **AN-L0-069** | AppNotificationContext_Inequality_RequestId | `AppNotificationsImpl.cpp` | `operator==` | requestId differs | Returns false |
| **AN-L0-070** | AppNotificationContext_Inequality_ConnectionId | `AppNotificationsImpl.cpp` | `operator==` | connectionId differs | Returns false |
| **AN-L0-071** | AppNotificationContext_Inequality_AppId | `AppNotificationsImpl.cpp` | `operator==` | appId differs | Returns false |
| **AN-L0-072** | AppNotificationContext_Inequality_Origin | `AppNotificationsImpl.cpp` | `operator==` | origin differs | Returns false |
| **AN-L0-073** | AppNotificationContext_Inequality_Version | `AppNotificationsImpl.cpp` | `operator==` | version differs | Returns false |
| **AN-L0-074** | SubscriberMap_Destructor_ReleasesInterfaces | `AppNotificationsImpl.h` | `~SubscriberMap` | mAppGateway and mInternalGatewayNotifier non-null | Both Released and set to nullptr |
| **AN-L0-075** | Boundary_Subscribe_EmptyEvent | `AppNotificationsImpl.cpp` | `Subscribe` | event="" | Returns ERROR_NONE, stored under "" key |
| **AN-L0-076** | Boundary_Subscribe_EmptyModule | `AppNotificationsImpl.cpp` | `Subscribe` | module="" | Returns ERROR_NONE, SubscriberJob has empty module |
| **AN-L0-077** | Boundary_Emit_LargePayload | `AppNotificationsImpl.cpp` | `Emit` | 100KB payload string | Returns ERROR_NONE, no truncation |
| **AN-L0-078** | Boundary_Cleanup_ZeroConnectionId | `AppNotificationsImpl.cpp` | `Cleanup` | connectionId=0 | Removes matching entries (if any), no crash |
| **AN-L0-079** | Boundary_MaxUint32_ConnectionId | `AppNotificationsImpl.cpp` | `Subscribe`, `Cleanup` | connectionId=UINT32_MAX | Stores and cleans up correctly |
| **AN-L0-080** | Boundary_MaxUint32_RequestId | `AppNotificationsImpl.cpp` | `Subscribe` | requestId=UINT32_MAX | Context stored correctly |
| **AN-L0-081** | InterfaceMap_IAppNotifications | `AppNotificationsImpl.h` | `INTERFACE_MAP` | QueryInterface with IAppNotifications::ID | Returns valid pointer |
| **AN-L0-082** | InterfaceMap_IConfiguration | `AppNotificationsImpl.h` | `INTERFACE_MAP` | QueryInterface with IConfiguration::ID | Returns valid pointer |
| **AN-L0-083** | PluginShell_InterfaceMap | `AppNotifications.h` | `INTERFACE_MAP` | QueryInterface IPlugin, IDispatcher, IAppNotifications | Returns valid pointers |

---

## Detailed Test Cases

### AppNotifications.cpp (Plugin Shell)

#### AN-L0-001: Initialize_Success

- **Description**: Verify that `Initialize()` returns an empty string when the implementation is successfully instantiated via `IShell::Root()` and the `IConfiguration` interface is configured.
- **Preconditions**: `ServiceMock` configured to return a valid `AppNotificationsImplementation` fake via `Instantiate()`.
- **Input**: Valid `IShell*` pointer.
- **Steps**:
  1. Create `AppNotifications` plugin instance via `Core::Service<AppNotifications>::Create<IPlugin>()`.
  2. Call `plugin->Initialize(serviceMock)`.
  3. Verify return value is empty string.
  4. Call `plugin->Deinitialize(serviceMock)`.
- **Expected Result**: Returns `""` (empty string). `mAppNotifications` is non-null. `Configure()` was called on the implementation.

#### AN-L0-002: Initialize_FailNullImpl

- **Description**: Verify that `Initialize()` returns an error string when `IShell::Root()` fails to instantiate the implementation.
- **Preconditions**: `ServiceMock` configured to return nullptr from `Instantiate()`.
- **Input**: Valid `IShell*` that returns nullptr for implementation.
- **Steps**:
  1. Create plugin, call `Initialize()` with a ServiceMock whose `Instantiate()` returns nullptr.
  2. Verify return value contains error text.
  3. Call `Deinitialize()` to ensure no crash on cleanup.
- **Expected Result**: Returns `"Could not retrieve the AppNotifications interface."`.

#### AN-L0-003: Initialize_ConfigureInterface

- **Description**: Verify that `Initialize()` queries `IConfiguration` on the implementation and calls `Configure()`.
- **Preconditions**: Implementation fake supports both `IAppNotifications` and `IConfiguration`.
- **Input**: Valid `IShell*`.
- **Steps**:
  1. Create plugin, call `Initialize()`.
  2. Verify the implementation's `Configure()` was invoked (check via fake counter).
- **Expected Result**: `Configure()` called exactly once. `configConnection->Release()` called.

#### AN-L0-004: Deinitialize_HappyPath

- **Description**: Verify normal `Deinitialize()` releases the implementation and service.
- **Input**: Plugin previously initialized successfully.
- **Steps**:
  1. Initialize plugin.
  2. Call `Deinitialize(service)`.
  3. Verify `mAppNotifications` released, `mService` released, `mConnectionId` reset to 0.
- **Expected Result**: No crash. All resources released.

#### AN-L0-005: Deinitialize_NullImpl

- **Description**: Verify `Deinitialize()` handles the case where `mAppNotifications` is nullptr (initialization failed).
- **Input**: Plugin initialized with failing ServiceMock.
- **Steps**:
  1. Initialize plugin (fails, mAppNotifications=nullptr).
  2. Call `Deinitialize()`.
- **Expected Result**: No crash. `mService` still released.

#### AN-L0-006: Deinitialize_WithRemoteConnection

- **Description**: Verify `Deinitialize()` calls `Terminate()` and `Release()` on the remote connection.
- **Input**: ServiceMock returns non-null from `RemoteConnection()`.
- **Steps**:
  1. Initialize plugin.
  2. Configure ServiceMock to return a mock `IRemoteConnection` from `RemoteConnection(mConnectionId)`.
  3. Call `Deinitialize()`.
- **Expected Result**: `connection->Terminate()` and `connection->Release()` called.

#### AN-L0-007: Deactivated_MatchingConnectionId

- **Description**: Verify `Deactivated()` submits a deactivation job when the connection ID matches.
- **Input**: `IRemoteConnection` with `Id()` == `mConnectionId`.
- **Steps**:
  1. Initialize plugin (stores `mConnectionId`).
  2. Create mock `IRemoteConnection` with matching ID.
  3. Call `Deactivated(connection)`.
- **Expected Result**: `IWorkerPool::Submit()` called with a `PluginHost::IShell::Job` for `DEACTIVATED`/`FAILURE`.

#### AN-L0-008: Deactivated_NonMatchingConnectionId

- **Description**: Verify `Deactivated()` does nothing when the connection ID does not match.
- **Input**: `IRemoteConnection` with `Id()` != `mConnectionId`.
- **Steps**:
  1. Initialize plugin.
  2. Call `Deactivated()` with mismatched connection.
- **Expected Result**: No job submitted. No crash.

#### AN-L0-009: Constructor_Destructor_Lifecycle

- **Description**: Verify construction and destruction of the plugin shell without initialization.
- **Steps**:
  1. Create `AppNotifications` instance.
  2. Immediately destroy it (scope exit).
- **Expected Result**: No crash. Members initialized to default (nullptr, 0).

---

### AppNotificationsImplementation.cpp (Core Logic)

#### AN-L0-010: Impl_Constructor_MemberInit

- **Description**: Verify implementation constructor initializes all members.
- **Steps**: Construct `AppNotificationsImplementation`. Verify `mShell` is nullptr.
- **Expected Result**: No crash. Default state.

#### AN-L0-011: Impl_Destructor_ShellRelease

- **Description**: Verify destructor releases the shell if it was configured.
- **Steps**: Configure implementation, then destroy.
- **Expected Result**: `mShell->Release()` called.

#### AN-L0-012: Impl_Destructor_NullShell

- **Description**: Verify destructor handles null shell gracefully.
- **Steps**: Construct and destroy without calling `Configure()`.
- **Expected Result**: No crash.

#### AN-L0-013: Configure_Success

- **Description**: Verify `Configure()` stores the shell and calls `AddRef()`.
- **Input**: Valid `IShell*`.
- **Steps**:
  1. Call `Configure(shell)`.
  2. Verify return is `ERROR_NONE`.
- **Expected Result**: `mShell` set, `AddRef()` called.

#### AN-L0-014: Subscribe_FirstListener_TriggersThunderSub

- **Description**: First subscription for an event triggers a Thunder notification subscription.
- **Input**: `context={requestId:1, connectionId:10, appId:"app1", origin:"org.rdk.AppGateway"}`, `listen=true`, `module="org.rdk.SomePlugin"`, `event="onFoo"`.
- **Steps**:
  1. Call `Subscribe(context, true, module, event)`.
  2. Verify SubscriberJob submitted to WorkerPool (subscribe=true).
  3. Verify context added to SubscriberMap.
- **Expected Result**: Returns `ERROR_NONE`. SubscriberJob dispatched.

#### AN-L0-015: Subscribe_SecondListener_NoThunderSub

- **Description**: Second subscription for the same event does NOT trigger a new Thunder subscription.
- **Input**: Two different contexts, same event.
- **Steps**:
  1. Subscribe context1 for "onFoo".
  2. Subscribe context2 for "onFoo".
  3. Verify only one SubscriberJob submitted.
- **Expected Result**: Both contexts in map. Only one Thunder subscription.

#### AN-L0-016: Subscribe_CaseInsensitive

- **Description**: Event keys are stored case-insensitively.
- **Input**: Subscribe with "OnFoo", check Exists("onfoo").
- **Expected Result**: `Exists("onfoo")` returns true.

#### AN-L0-017: Unsubscribe_LastListener_TriggersThunderUnsub

- **Description**: Removing the last subscriber triggers a Thunder unsubscription.
- **Input**: One subscriber for "onFoo", then unsubscribe.
- **Expected Result**: SubscriberJob submitted (subscribe=false). Event key removed from map.

#### AN-L0-018: Unsubscribe_NotLastListener_NoThunderUnsub

- **Description**: Removing one of multiple subscribers does NOT trigger Thunder unsubscription.
- **Input**: Two subscribers for "onFoo", remove one.
- **Expected Result**: No unsubscribe job. One subscriber remains.

#### AN-L0-019: Unsubscribe_NonExistent_NoCrash

- **Description**: Unsubscribing a context that was never added does not crash.
- **Input**: Context not in map.
- **Expected Result**: Returns `ERROR_NONE`. No crash.

#### AN-L0-020: Emit_SubmitsJob

- **Description**: `Emit()` submits an `EmitJob` to the worker pool.
- **Input**: `event="onFoo"`, `payload="{\"data\":1}"`, `appId="com.app"`.
- **Expected Result**: Returns `ERROR_NONE`. EmitJob dispatched.

#### AN-L0-021: Emit_EmptyPayload

- **Description**: `Emit()` with empty payload string.
- **Input**: `payload=""`.
- **Expected Result**: Returns `ERROR_NONE`. No crash.

#### AN-L0-022: Emit_EmptyAppId

- **Description**: `Emit()` with empty appId broadcasts to all subscribers.
- **Input**: `appId=""`.
- **Expected Result**: Returns `ERROR_NONE`. All matching subscribers receive event.

#### AN-L0-023: Cleanup_RemovesMatchingSubscribers

- **Description**: `Cleanup()` removes all subscribers matching connectionId + origin.
- **Input**: Two subscribers for "onFoo" — one with connectionId=10/origin="gw", one with connectionId=20/origin="gw". Cleanup connectionId=10, origin="gw".
- **Expected Result**: First subscriber removed. Second retained.

#### AN-L0-024: Cleanup_EmptiesEntireKey

- **Description**: If cleanup removes all subscribers for a key, the key is erased.
- **Input**: One subscriber for "onFoo" matching cleanup criteria.
- **Expected Result**: Key "onfoo" no longer exists in map.

#### AN-L0-025: Cleanup_NoMatch_NoCrash

- **Description**: Cleanup with no matching subscribers is a no-op.
- **Input**: connectionId=999, origin="unknown".
- **Expected Result**: Map unchanged. No crash.

#### AN-L0-026: Cleanup_MultipleEvents

- **Description**: Cleanup removes matching subscribers across multiple events.
- **Input**: Subscriber connectionId=10 subscribed to "onFoo" and "onBar". Cleanup connectionId=10.
- **Expected Result**: Both event entries cleaned.

---

### SubscriberMap (Inner Class)

#### AN-L0-027 through AN-L0-036

See summary table above. These test the core `Add`, `Remove`, `Get`, `Exists` operations with positive, negative, and case-insensitivity scenarios.

#### AN-L0-037: EventUpdate_DispatchToAll_EmptyAppId

- **Description**: When appId is empty, EventUpdate dispatches to ALL subscribers for the event.
- **Input**: Two subscribers with different appIds. Emit with appId="".
- **Expected Result**: Both subscribers' dispatch methods called.

#### AN-L0-038: EventUpdate_FilterByAppId

- **Description**: When appId is non-empty, only matching subscribers receive the event.
- **Input**: Subscribers for "com.app.one" and "com.app.two". Emit with appId="com.app.one".
- **Expected Result**: Only "com.app.one" subscriber dispatched.

#### AN-L0-039: EventUpdate_NoListeners_LogWarning

- **Description**: When no subscribers exist for the event key, a warning is logged.
- **Input**: Emit for event "unknownEvent" with no subscribers.
- **Expected Result**: LOGWARN emitted. No crash.

#### AN-L0-040: EventUpdate_VersionedEventKey

- **Description**: Versioned event keys (e.g., "onFoo.v8") have their suffix stripped via `ContextUtils::GetBaseEventNameFromVersionedEvent()` before dispatching.
- **Input**: Event key "onFoo.v8", subscriber registered for "onfoo.v8".
- **Expected Result**: Dispatch called with clearKey="onFoo" (suffix stripped).

#### AN-L0-041: Dispatch_OriginGateway

- **Description**: Context with `origin == APP_GATEWAY_CALLSIGN` routes to `DispatchToGateway`.
- **Input**: `context.origin = "org.rdk.AppGateway"`.
- **Expected Result**: `DispatchToGateway()` called. `DispatchToLaunchDelegate()` NOT called.

#### AN-L0-042: Dispatch_OriginNonGateway

- **Description**: Context with non-gateway origin routes to `DispatchToLaunchDelegate`.
- **Input**: `context.origin = "org.rdk.LaunchDelegate"`.
- **Expected Result**: `DispatchToLaunchDelegate()` called. `DispatchToGateway()` NOT called.

#### AN-L0-043: DispatchToGateway_LazyAcquire_Success

- **Description**: First dispatch acquires `IAppGatewayResponder` via `QueryInterfaceByCallsign(APP_GATEWAY_CALLSIGN)`.
- **Input**: Shell returns valid AppGatewayResponder mock.
- **Expected Result**: Interface acquired. `Emit()` called on responder.

#### AN-L0-044: DispatchToGateway_LazyAcquire_Failure

- **Description**: When the AppGateway responder is not available, an error is logged and dispatch is aborted.
- **Input**: Shell returns nullptr for APP_GATEWAY_CALLSIGN.
- **Expected Result**: LOGERR emitted. No crash. No Emit called.

#### AN-L0-045: DispatchToGateway_AlreadyAcquired

- **Description**: Subsequent dispatches reuse the cached interface.
- **Input**: Two consecutive dispatches.
- **Expected Result**: `QueryInterfaceByCallsign` called only once. Both dispatches call Emit.

#### AN-L0-046 through AN-L0-048

Same patterns as AN-L0-043 through AN-L0-045 but for `DispatchToLaunchDelegate` using `INTERNAL_GATEWAY_CALLSIGN`.

---

### ThunderSubscriptionManager (Inner Class)

#### AN-L0-049: ThunderMgr_Subscribe_NewEvent

- **Description**: `Subscribe()` for a new event calls `RegisterNotification()`.
- **Input**: module="org.rdk.FbSettings", event="onFoo".
- **Expected Result**: `HandleNotifier()` called with listen=true. On success, added to registry.

#### AN-L0-050: ThunderMgr_Subscribe_AlreadyRegistered

- **Description**: `Subscribe()` for an already-registered event is a no-op (LOGTRACE only).
- **Input**: Subscribe "onFoo" twice.
- **Expected Result**: Second call skips `RegisterNotification()`.

#### AN-L0-051: ThunderMgr_Unsubscribe_RegisteredEvent

- **Description**: `Unsubscribe()` for a registered event calls `UnregisterNotification()`.
- **Input**: Subscribe then unsubscribe "onFoo".
- **Expected Result**: `HandleNotifier()` called with listen=false. Removed from registry.

#### AN-L0-052: ThunderMgr_Unsubscribe_NotRegistered

- **Description**: `Unsubscribe()` for a non-registered event logs an error.
- **Input**: Unsubscribe "onFoo" without prior subscribe.
- **Expected Result**: LOGERR emitted. No crash.

#### AN-L0-053: HandleNotifier_Success

- **Description**: `HandleNotifier()` queries `IAppNotificationHandler` and calls `HandleAppEventNotifier()`.
- **Input**: Mock handler returns `ERROR_NONE` with `status=true`.
- **Expected Result**: Returns true. Handler's Release() called.

#### AN-L0-054: HandleNotifier_HandlerNotAvailable

- **Description**: When the notification handler module is not available, returns false.
- **Input**: `QueryInterfaceByCallsign` returns nullptr.
- **Expected Result**: Returns false. LOGERR emitted.

#### AN-L0-055: HandleNotifier_HandlerReturnsError

- **Description**: When `HandleAppEventNotifier()` returns non-ERROR_NONE, returns false.
- **Input**: Mock handler returns `ERROR_GENERAL`.
- **Expected Result**: Returns false. LOGERR "Notification subscription failure" emitted.

#### AN-L0-056 through AN-L0-059

Test `RegisterNotification` and `UnregisterNotification` based on HandleNotifier return value. See summary table.

#### AN-L0-060 through AN-L0-062

Test `IsNotificationRegistered` with present, absent, and case-insensitive lookups.

#### AN-L0-063: ThunderMgr_Destructor_UnsubscribesAll

- **Description**: Destructor copies the notification list and calls `HandleNotifier(listen=false)` for each.
- **Input**: Two registered notifications.
- **Expected Result**: Both unsubscribed. `mRegisteredNotifications` cleared.

---

### SubscriberJob / EmitJob / Emitter (Dispatch Classes)

#### AN-L0-064: SubscriberJob_Dispatch_Subscribe

- **Description**: When `mSubscribe=true`, `Dispatch()` calls `ThunderManager.Subscribe()`.
- **Expected Result**: Subscribe path executed.

#### AN-L0-065: SubscriberJob_Dispatch_Unsubscribe

- **Description**: When `mSubscribe=false`, `Dispatch()` calls `ThunderManager.Unsubscribe()`.
- **Expected Result**: Unsubscribe path executed.

#### AN-L0-066: EmitJob_Dispatch

- **Description**: `Dispatch()` calls `SubscriberMap.EventUpdate()` with stored event/payload/appId.
- **Expected Result**: EventUpdate invoked with correct parameters.

#### AN-L0-067: Emitter_Emit_SubmitsJob

- **Description**: `Emitter::Emit()` creates an `EmitJob` and submits to WorkerPool.
- **Input**: event="onFoo", payload="{}", appId="app1".
- **Expected Result**: EmitJob submitted. LOGINFO emitted.

#### AN-L0-068 through AN-L0-073: AppNotificationContext Equality

See summary table. Tests the `operator==` with matching and per-field mismatching contexts.

#### AN-L0-074: SubscriberMap_Destructor_ReleasesInterfaces

- **Description**: Destructor releases `mAppGateway` and `mInternalGatewayNotifier`.
- **Expected Result**: Both `Release()` called. Set to nullptr.

---

### Boundary Tests

#### AN-L0-075 through AN-L0-080

Boundary value tests for empty strings, zero values, UINT32_MAX, and large payloads. See summary table.

#### AN-L0-081 through AN-L0-083: Interface Map Tests

Verify `QueryInterface` returns correct interface pointers for `IAppNotifications`, `IConfiguration`, `IPlugin`, and `IDispatcher`.

---

## Coverage Analysis

### Target: Minimum 75% line coverage

| File | Total Executable Lines | Lines Covered by Tests | Coverage % |
|------|----------------------|----------------------|------------|
| `AppNotifications.cpp` | ~62 | ~58 (AN-L0-001 to AN-L0-009) | **93%** |
| `AppNotificationsImplementation.cpp` | ~210 | ~195 (AN-L0-010 to AN-L0-074) | **93%** |
| `AppNotificationsImplementation.h` | ~120 (inline methods) | ~105 (AN-L0-064 to AN-L0-083) | **88%** |
| `AppNotifications.h` | ~15 (inline + interface map) | ~13 (AN-L0-083) | **87%** |
| `Module.cpp` | ~2 | ~2 (compiled into test binary) | **100%** |
| `Module.h` | ~5 | ~5 (included by all) | **100%** |
| **Overall** | **~414** | **~378** | **~91%** |

### Methods NOT fully covered and rationale

| Method | File:Line | Reason | Workaround |
|--------|-----------|--------|------------|
| `SERVICE_REGISTRATION` macro | `AppNotifications.cpp:44`, `Impl.cpp:36` | Macro expansion generates static registration code that executes at library load time. Cannot be directly unit-tested in L0 without loading the .so. | Covered implicitly when the L0 test binary links the compiled `.cpp` files. |
| `Metadata` static object construction | `AppNotifications.cpp:31` | Static initialization — same as above. | Covered by linker. |
| `SYSLOG` calls | Multiple | Log output verification requires a logging mock or capture. Not critical for functional coverage. | Can add a log-capture utility if needed for >90% coverage. |
| `INTERFACE_MAP` internals | `AppNotifications.h:55`, `Impl.h:138` | Macro-generated `QueryInterface` dispatch. | Covered by AN-L0-081/082/083 which exercise `QueryInterface()`. |

---

## Uncovered / Partially Covered Methods

The following methods have partial or limited coverage. Each is documented with a recommended action:

| # | Method | Coverage Status | Recommendation |
|---|--------|----------------|----------------|
| 1 | `Deactivated()` | Partial — difficult to mock `IRemoteConnection` in pure L0 | Add a `RemoteConnectionMock` that implements `RPC::IRemoteConnection` to fully cover both branches |
| 2 | `SubscriberMap::~SubscriberMap()` (destructor) | Partial — requires both `mAppGateway` and `mInternalGatewayNotifier` to be non-null | Covered by AN-L0-074 which ensures both paths are exercised via pre-loading the cached interfaces |
| 3 | `ThunderSubscriptionManager::~ThunderSubscriptionManager()` | Partial — requires populated `mRegisteredNotifications` | Covered by AN-L0-063 which registers notifications before destruction |

---

## L0 Test Code Generation Guide (Reference from AppGateway L0)

This section explains how the AppNotifications L0 test code should be structured, taking direct reference from the existing AppGateway L0 test implementation.

### 1. Architecture Pattern

The AppGateway L0 tests follow this pattern:

```
Tests/L0Tests/
├── CMakeLists.txt                          # Build configuration
├── common/
│   ├── L0Bootstrap.cpp/.hpp                # WorkerPool + Messaging bootstrap
│   ├── L0Expect.hpp                        # Assertion helpers (ExpectTrue, ExpectEqU32, etc.)
│   ├── L0ServiceMock.hpp                   # Base registry-based service mock
│   └── L0TestTypes.hpp                     # TestResult struct, ResultToExitCode, PrintTotals
└── AppGateway/
    ├── ServiceMock.h                       # Plugin-specific IShell mock with fakes
    ├── AppGatewayTest.cpp                  # Main test runner (main() + test registry)
    ├── AppGateway_Init_DeinitTests.cpp     # Lifecycle tests
    ├── Responder_BehaviorTests.cpp         # Responder interface tests
    └── ...                                 # Additional test files
```

The AppNotifications L0 tests should mirror this:

```
Tests/L0Tests/
├── AppNotifications/
│   ├── AppNotificationsServiceMock.h       # AppNotifications-specific IShell mock
│   ├── AppNotificationsTest.cpp            # Main runner + test registry
│   ├── AppNotifications_Init_DeinitTests.cpp
│   ├── AppNotifications_SubscribeTests.cpp
│   ├── AppNotifications_EmitTests.cpp
│   ├── AppNotifications_SubscriberMapTests.cpp
│   ├── AppNotifications_ThunderManagerTests.cpp
│   └── AppNotifications_BoundaryTests.cpp
```

### 2. Bootstrap (Reuse Existing Common Code)

The existing `Tests/L0Tests/common/` directory provides reusable bootstrap utilities. The AppNotifications L0 tests should reuse them:

```cpp
// In AppNotificationsTest.cpp (main runner)
#include "L0Bootstrap.hpp"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

int main()
{
    L0Test::L0BootstrapGuard bootstrap;  // Initializes WorkerPool

    // ... run test cases ...

    WPEFramework::Core::Singleton::Dispose();
    L0Test::PrintTotals(std::cerr, "AppNotifications L0", failures);
    return L0Test::ResultToExitCode(failures);
}
```

### 3. ServiceMock Pattern

Following the AppGateway `ServiceMock.h` pattern (`Tests/L0Tests/AppGateway/ServiceMock.h`), create a plugin-specific mock:

```cpp
// AppNotificationsServiceMock.h
namespace L0Test {

class AppNotificationsServiceMock final : public WPEFramework::PluginHost::IShell,
                                           public WPEFramework::PluginHost::IShell::ICOMLink {
public:
    struct Config {
        bool provideImplementation;    // Whether Instantiate() returns a valid IAppNotifications
        bool provideAppGateway;        // Whether QueryInterfaceByCallsign returns AppGateway responder
        bool provideInternalGateway;   // Whether QueryInterfaceByCallsign returns InternalGateway responder
        bool provideNotificationHandler; // Whether QueryInterfaceByCallsign returns IAppNotificationHandler

        explicit Config(bool impl = true, bool gw = true, bool igw = true, bool handler = true)
            : provideImplementation(impl)
            , provideAppGateway(gw)
            , provideInternalGateway(igw)
            , provideNotificationHandler(handler)
        {}
    };

    // ... IShell methods (follow AppGateway ServiceMock pattern) ...

    void* Instantiate(...) override {
        // Return AppNotificationsImplementation fake or real instance
        // based on Config flags
    }

    void* QueryInterfaceByCallsign(uint32_t id, const string& name) override {
        // Return AppGatewayResponderMock for APP_GATEWAY_CALLSIGN
        // Return AppGatewayResponderMock for INTERNAL_GATEWAY_CALLSIGN
        // Return AppNotificationHandlerMock for notification handler modules
    }
};

} // namespace L0Test
```

### 4. Test Function Pattern

Each test is a standalone `uint32_t` function returning failure count, registered in the main runner. This is the exact pattern from AppGateway:

```cpp
// AppNotifications_SubscribeTests.cpp

extern uint32_t Test_Subscribe_FirstListener_TriggersThunderSub();

uint32_t Test_Subscribe_FirstListener_TriggersThunderSub()
{
    L0Test::TestResult tr;

    // 1. Setup: create implementation, configure with mock shell
    WPEFramework::Core::Sink<WPEFramework::Plugin::AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // 2. Execute
    Exchange::IAppNotifications::AppNotificationContext ctx;
    ctx.requestId = 1;
    ctx.connectionId = 10;
    ctx.appId = "com.test.app";
    ctx.origin = "org.rdk.AppGateway";
    ctx.version = "0";

    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.SomePlugin", "onFoo");

    // 3. Assert
    L0Test::ExpectEqU32(tr, rc, WPEFramework::Core::ERROR_NONE, "Subscribe returns ERROR_NONE");

    return tr.failures;
}
```

### 5. Main Runner Pattern

Following `AppGatewayTest.cpp`:

```cpp
// AppNotificationsTest.cpp

#include "L0Bootstrap.hpp"
#include "L0TestTypes.hpp"

// Extern declarations for all test functions
extern uint32_t Test_Initialize_Success();
extern uint32_t Test_Initialize_FailNullImpl();
// ... all 83 test functions ...

int main()
{
    L0Test::L0BootstrapGuard bootstrap;

    struct Case {
        const char* name;
        uint32_t (*fn)();
    };

    const Case cases[] = {
        { "Initialize_Success",                         Test_Initialize_Success },
        { "Initialize_FailNullImpl",                    Test_Initialize_FailNullImpl },
        // ... all test cases ...
    };

    uint32_t failures = 0;
    for (const auto& c : cases) {
        std::cerr << "[ RUN      ] " << c.name << std::endl;
        const uint32_t f = c.fn();
        if (f == 0) {
            std::cerr << "[       OK ] " << c.name << std::endl;
        } else {
            std::cerr << "[  FAILED  ] " << c.name << " failures=" << f << std::endl;
        }
        failures += f;
    }

    WPEFramework::Core::Singleton::Dispose();
    L0Test::PrintTotals(std::cerr, "AppNotifications L0", failures);
    return L0Test::ResultToExitCode(failures);
}
```

### 6. Mock Dependencies

The following existing mock files can be reused:

| Mock | Path | Used For |
|------|------|----------|
| `AppGatewayMock.h` | `Tests/mocks/AppGatewayMock.h` | Mocking `IAppGatewayResponder` for DispatchToGateway/DispatchToLaunchDelegate |
| `AppNotificationHandlerMock.h` | `Tests/mocks/AppNotificationHandlerMock.h` | Mocking `IAppNotificationHandler` for HandleNotifier tests |

New mocks to create:

| Mock | Purpose |
|------|---------|
| `AppNotificationsServiceMock.h` | Plugin-specific IShell mock (see Section 3 above) |
| `RemoteConnectionMock.h` | For `Deactivated()` tests (optional, low priority) |

### 7. Key Differences from AppGateway L0

| Aspect | AppGateway L0 | AppNotifications L0 |
|--------|--------------|---------------------|
| Plugin under test | `AppGateway` (plugin shell) + `AppGatewayImplementation` + `AppGatewayResponderImplementation` | `AppNotifications` (plugin shell) + `AppNotificationsImplementation` |
| Primary interface | `IAppGatewayResolver`, `IAppGatewayResponder` | `IAppNotifications` |
| External dependencies | Resolver config JSONs, WebSocket transport | `IAppGatewayResponder` (for dispatch), `IAppNotificationHandler` (for Thunder subscriptions) |
| Inner class testing | Not applicable (separate implementation files) | `SubscriberMap`, `ThunderSubscriptionManager` (requires testing via the parent implementation's public API or direct `Core::Sink<>` instantiation) |
| `Core::Sink<>` usage | Used for `AppGatewayResponderImplementation` direct testing | Used for `AppNotificationsImplementation` direct testing |

### 8. Testing Inner Classes

The AppNotifications implementation has inner classes (`SubscriberMap`, `ThunderSubscriptionManager`) that are private. They should be tested **indirectly** through the parent class's public API:

- `SubscriberMap::Add/Remove/Get/Exists` → tested via `Subscribe()` / `Cleanup()` / `Emit()` on the implementation
- `ThunderSubscriptionManager::Subscribe/Unsubscribe` → tested via the `SubscriberJob::Dispatch()` path triggered by `Subscribe()`
- `EventUpdate/Dispatch` → tested via the `EmitJob::Dispatch()` path triggered by `Emit()`

For more direct testing, the `Core::Sink<AppNotificationsImplementation>` pattern (from `AppGatewayResponderImplementation_Tests.cpp`) allows calling methods on the implementation directly without going through the plugin shell.

---

## Proposed File Structure and CMakeLists.txt

### File Structure

```
Tests/L0Tests/
├── CMakeLists.txt                                # Updated to include AppNotifications target
├── common/                                       # Reused from existing
│   ├── L0Bootstrap.cpp
│   ├── L0Bootstrap.hpp
│   ├── L0Expect.hpp
│   ├── L0ServiceMock.hpp
│   └── L0TestTypes.hpp
├── AppGateway/                                   # Existing (unchanged)
│   └── ...
└── AppNotifications/                             # NEW
    ├── AppNotificationsServiceMock.h
    ├── AppNotificationsTest.cpp
    ├── AppNotifications_Init_DeinitTests.cpp
    ├── AppNotifications_SubscribeTests.cpp
    ├── AppNotifications_EmitTests.cpp
    ├── AppNotifications_SubscriberMapTests.cpp
    ├── AppNotifications_ThunderManagerTests.cpp
    ├── AppNotifications_ContextEqualityTests.cpp
    └── AppNotifications_BoundaryTests.cpp
```

### CMakeLists.txt Addition

```cmake
# --- AppNotifications L0 Test Target ---
project(appnotifications_l0test LANGUAGES CXX)

set(APPNOTIF_L0_SOURCES
    ${CMAKE_SOURCE_DIR}/../../AppNotifications/Module.cpp
    ${CMAKE_SOURCE_DIR}/../../AppNotifications/AppNotifications.cpp
    ${CMAKE_SOURCE_DIR}/../../AppNotifications/AppNotificationsImplementation.cpp
    AppNotifications/AppNotificationsTest.cpp
    AppNotifications/AppNotifications_Init_DeinitTests.cpp
    AppNotifications/AppNotifications_SubscribeTests.cpp
    AppNotifications/AppNotifications_EmitTests.cpp
    AppNotifications/AppNotifications_SubscriberMapTests.cpp
    AppNotifications/AppNotifications_ThunderManagerTests.cpp
    AppNotifications/AppNotifications_ContextEqualityTests.cpp
    AppNotifications/AppNotifications_BoundaryTests.cpp
    common/L0Bootstrap.cpp
)

add_executable(appnotifications_l0test ${APPNOTIF_L0_SOURCES})

target_compile_definitions(appnotifications_l0test PRIVATE
    MODULE_NAME=AppNotifications_L0Test
    APPNOTIFICATIONS_MAJOR_VERSION=1
    APPNOTIFICATIONS_MINOR_VERSION=0
    APPNOTIFICATIONS_PATCH_VERSION=0
)

target_include_directories(appnotifications_l0test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AppNotifications
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/../../AppNotifications
    ${CMAKE_SOURCE_DIR}/../../helpers
    ${CMAKE_SOURCE_DIR}/../../Tests/mocks
    ${PREFIX}/include
    ${PREFIX}/include/WPEFramework
)

# Link libraries (same pattern as appgateway_l0test)
target_link_libraries(appnotifications_l0test PRIVATE
    ${WPEFRAMEWORK_CORE_LIB}
    ${WPEFRAMEWORK_MESSAGING_LIB}
    ${WPEFRAMEWORK_PLUGINS_LIB}
    Threads::Threads
    m
    dl
)
```

---

## Summary

This document defines **83 test cases** covering:

- **35 testable methods** across 6 source files
- **Positive scenarios**: 42 tests
- **Negative scenarios**: 24 tests
- **Boundary scenarios**: 17 tests
- **Estimated coverage**: ~91% line coverage (exceeds 75% minimum)

All test cases follow the established L0 test patterns from the AppGateway plugin, using the same bootstrap utilities, assertion helpers, mock infrastructure, and test runner architecture.
