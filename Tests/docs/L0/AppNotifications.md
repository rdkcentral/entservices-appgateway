# AppNotifications L0 Tests – Quick Reference

Below is a categorized quick reference for all L0 AppNotifications unit tests. Each entry lists the test ID (as referenced in code and documentation) and a concise description. This format is designed for onboarding, auditing, and rapid code/test traceability.

## Initialization & Lifecycle
| Test ID                         | Description |
|----------------------------------|-------------|
| AN-L0-001 Initialize_Success                   | Plugin initializes successfully given a valid IShell and implementation. |
| AN-L0-002 Initialize_FailNullImpl              | Plugin initialization fails gracefully if implementation is nullptr. |
| AN-L0-003 Initialize_ConfigureInterface        | Calls Configure() via IConfiguration interface during initialization. |
| AN-L0-004 Deinitialize_HappyPath               | Deinitializes and releases implementation and IShell as expected. |
| AN-L0-005 Deinitialize_NullImpl                | Deinitialize handles missing impl member safely. |
| AN-L0-006 Deinitialize_WithRemoteConnection    | Calls Terminate/Release on remote connection during deinit. |
| AN-L0-007 Deactivated_MatchingConnectionId     | Submits job if deactivated connection matches plugin's ID. |
| AN-L0-008 Deactivated_NonMatchingConnectionId  | Does nothing if deactivated connection doesn't match. |
| AN-L0-009 Constructor_Destructor_Lifecycle     | Instantiates and destroys shell class without crash. |
| AN-L0-010 Impl_Constructor_MemberInit          | Impl constructor zeroes/initializes all members. |
| AN-L0-011 Impl_Destructor_ShellRelease         | Impl destructor releases configured shell. |
| AN-L0-012 Impl_Destructor_NullShell            | Impl destructor handles no-shell case without crash. |
| AN-L0-013 Configure_Success                    | Configure stores IShell and calls AddRef; returns ERROR_NONE. |
| AN-L0-084 Information_ReturnsEmpty             | Information() returns empty string (no metadata). |
| AN-L0-085 Configure_DoubleConfigure            | Calling Configure() twice returns ERROR_NONE both times without crash. |

## Subscription, Unsubscription, Cleanup
| Test ID                | Description |
|------------------------|-------------|
| AN-L0-014 Subscribe_FirstListener_TriggersThunderSub      | First subscription for event triggers Thunder subscription. |
| AN-L0-015 Subscribe_SecondListener_NoThunderSub           | Second subscription for same event doesn't trigger Thunder sub. |
| AN-L0-016 Subscribe_CaseInsensitive                       | Event keys handled case-insensitively for map. |
| AN-L0-017 Unsubscribe_LastListener_TriggersThunderUnsub   | Unsub last context triggers Thunder unsubscribe. |
| AN-L0-018 Unsubscribe_NotLastListener_NoThunderUnsub      | Unsub one of multiple listeners does not trigger Thunder unsub. |
| AN-L0-019 Unsubscribe_NonExistent_NoCrash                 | Unsubscribing non-existent context is a no-op. |
| AN-L0-020 Emit_SubmitsJob                                 | Emit() submits EmitJob for a valid event/payload/appId. |
| AN-L0-021 Emit_EmptyPayload                               | Emit() works for empty payload string. |
| AN-L0-022 Emit_EmptyAppId                                 | Emit() with empty appId broadcasts to all. |
| AN-L0-023 Cleanup_RemovesMatchingSubscribers              | Cleanup removes all subscribers for a given connId/origin. |
| AN-L0-024 Cleanup_EmptiesEntireKey                        | Cleanup removes key from map when last context is erased. |
| AN-L0-025 Cleanup_NoMatch_NoCrash                         | Cleanup on no matches is a safe no-op. |
| AN-L0-026 Cleanup_MultipleEvents                          | Cleanup removes subscribers across multiple events. |
| AN-L0-086 Cleanup_PartialMatch_KeepsOthers                | Cleanup removes matching contexts but keeps non-matching ones in the same key. |
| AN-L0-087 Cleanup_DifferentOrigin_NoMatch                 | Cleanup with a different origin leaves all subscribers intact. |
| AN-L0-088 Cleanup_EmptyMap_NoCrash                        | Cleanup on an empty subscriber map is a safe no-op. |

## Subscriber Map Implementation
| Test ID                              | Description |
|--------------------------------------|-------------|
| AN-L0-027 SubscriberMap_Add_NewKey              | Add() creates new entry for new event key. |
| AN-L0-028 SubscriberMap_Add_ExistingKey         | Add() appends context to existing key entry. |
| AN-L0-029 SubscriberMap_Remove_ExistingContext  | Remove() removes context from map. |
| AN-L0-030 SubscriberMap_Remove_LastContext_ErasesKey | Remove() on last context erases map key. |
| AN-L0-031 SubscriberMap_Remove_NonExistent_NoCrash | Remove() of context not present is a no-op. |
| AN-L0-032 SubscriberMap_Get_Existing            | Get() returns vector of contexts for key. |
| AN-L0-033 SubscriberMap_Get_NonExistent         | Get() on missing key returns empty vector. |
| AN-L0-034 SubscriberMap_Exists_True             | Exists() returns true if present. |
| AN-L0-035 SubscriberMap_Exists_False            | Exists() returns false if key is missing. |
| AN-L0-036 SubscriberMap_Exists_CaseInsensitive  | Exists() is case-insensitive. |

## Event Update & Dispatch
| Test ID                             | Description |
|-------------------------------------|-------------|
| AN-L0-037 EventUpdate_DispatchToAll_EmptyAppId | appId="" triggers dispatch to all for event. |
| AN-L0-038 EventUpdate_FilterByAppId           | appId filter delivers event to only matching listener. |
| AN-L0-039 EventUpdate_NoListeners_LogWarning  | No subscribers logs warning, no crash. |
| AN-L0-040 EventUpdate_VersionedEventKey       | Versioned event keys are converted for dispatch. |
| AN-L0-089 EventUpdate_NonVersionedEventName   | Non-versioned event name is passed through unchanged to dispatch. |
| AN-L0-090 EventUpdate_AppId_NonMatch_Skipped  | EventUpdate with non-matching appId skips dispatch for that listener. |
| AN-L0-091 Emit_MixedOrigins_DispatchBoth      | Emit dispatches to both gateway and non-gateway origin subscribers. |
| AN-L0-092 Emit_SpecificAppId_MatchesOne       | Emit with specific appId dispatches only to the matching subscriber. |
| AN-L0-041 Dispatch_OriginGateway              | Gateway-origin context uses DispatchToGateway. |
| AN-L0-042 Dispatch_OriginNonGateway           | Non-gateway origin uses DispatchToLaunchDelegate. |
| AN-L0-043 DispatchToGateway_LazyAcquire_Success | Handles lazy lookup for responder interface. |
| AN-L0-044 DispatchToGateway_LazyAcquire_Failure | Handles failure to acquire responder interface. |
| AN-L0-045 DispatchToGateway_AlreadyAcquired     | Uses cached responder interface on second dispatch. |
| AN-L0-046 DispatchToLaunchDelegate_LazyAcquire_Success | Handles lazy lookup for internal gateway. |
| AN-L0-047 DispatchToLaunchDelegate_LazyAcquire_Failure | Handles failure to acquire internal gateway. |
| AN-L0-048 DispatchToLaunchDelegate_AlreadyAcquired  | Uses cached internal gateway for second dispatch. |

## ThunderSubscriptionManager
| Test ID                           | Description |
|-----------------------------------|-------------|
| AN-L0-049 ThunderMgr_Subscribe_NewEvent           | New event triggers RegisterNotification(). |
| AN-L0-050 ThunderMgr_Subscribe_AlreadyRegistered  | Subscribe on duplicate event is a no-op. |
| AN-L0-051 ThunderMgr_Unsubscribe_RegisteredEvent  | Registered event triggers UnregisterNotification(). |
| AN-L0-052 ThunderMgr_Unsubscribe_NotRegistered    | Unsubscribe with no registration logs error. |
| AN-L0-053 HandleNotifier_Success                  | Handler resolves OK and returns success. |
| AN-L0-054 HandleNotifier_HandlerNotAvailable      | QueryInterfaceByCallsign returns nullptr logs error. |
| AN-L0-055 HandleNotifier_HandlerReturnsError      | HandleAppEventNotifier returns error results in false. |
| AN-L0-056–059 Register/UnregisterNotification     | Registry update logic (see detailed doc for granularity). |
| AN-L0-060 IsNotificationRegistered_Exists         | Registry check returns true if present. |
| AN-L0-061 IsNotificationRegistered_NotExists      | Registry check returns false if absent. |
| AN-L0-062 IsNotificationRegistered_CaseInsensitive | Registry lookup is case-insensitive. |
| AN-L0-063 ThunderMgr_Destructor_UnsubscribesAll   | Destructor unsubscribes all notifications. |

## Job/Evaluator Classes & Operator==
| Test ID                           | Description |
|-----------------------------------|-------------|
| AN-L0-064 SubscriberJob_Dispatch_Subscribe    | Dispatch() with mSubscribe=true calls ThunderManager.Subscribe. |
| AN-L0-065 SubscriberJob_Dispatch_Unsubscribe  | Dispatch() with mSubscribe=false calls ThunderManager.Unsubscribe. |
| AN-L0-066 EmitJob_Dispatch                   | EmitJob dispatches EventUpdate to map. |
| AN-L0-067 Emitter_Emit_SubmitsJob            | Emitter::Emit() submits EmitJob via worker pool. |
| AN-L0-068–AN-L0-073 AppNotificationContext Equality | operator== tested for all fields and inequality. |
| AN-L0-074 SubscriberMap_Destructor_ReleasesInterfaces | Map destructor releases gateway/notifier pointers. |

## Boundary and Interface Map Tests
| Test ID                           | Description |
|-----------------------------------|-------------|
| AN-L0-075 Boundary_Subscribe_EmptyEvent       | Subscribing with empty event supported. |
| AN-L0-076 Boundary_Subscribe_EmptyModule      | Subscribing with empty module supported. |
| AN-L0-077 Boundary_Emit_LargePayload          | Handles large payload emission. |
| AN-L0-078 Boundary_Cleanup_ZeroConnectionId   | Handles zero-valued connectionId in cleanup. |
| AN-L0-079 Boundary_MaxUint32_ConnectionId     | Handles UINT32_MAX connectionId for subscribe/cleanup. |
| AN-L0-080 Boundary_MaxUint32_RequestId        | Handles UINT32_MAX requestId for subscribe. |
| AN-L0-081 InterfaceMap_IAppNotifications      | QueryInterface returns IAppNotifications pointer. |
| AN-L0-082 InterfaceMap_IConfiguration         | QueryInterface returns IConfiguration pointer. |
| AN-L0-083 PluginShell_InterfaceMap            | QueryInterface for IPlugin, IDispatcher, IAppNotifications. |

---
Each listed test covers a targeted component or use case within the AppNotifications L0 implementation. For full scenario, input, and assertion details, refer to `AppNotifications_L0_TestCases.md`.
