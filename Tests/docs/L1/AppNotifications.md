# AppNotifications L1 Tests: Detailed Test Cases with IDs

Below is a categorized, tabular listing of every AppNotifications L1 test. Each row shows:
- **Test ID:** The exact test function name (from `TEST_F(AppNotificationsTest, <ID>)`)
- **Description:** What the test verifies and why it is important

## Configure Tests

| Test ID | Description |
| --- | --- |
| Configure_StoresShellAndAddsRef | Ensures that plugin configuration stores the service (shell) and increases its reference count. |
| Configure_CalledTwice_ReleasesOldShellAndAddsRefNew | Verifies a second configuration call updates the shell pointer safely and manages references correctly. |

## Subscription (Subscribe) Behavior

| Test ID | Description |
| --- | --- |
| Subscribe_Listen_True_NewEvent_ReturnsNone | Subscribing to a new event adds a subscriber and schedules a worker job. |
| Subscribe_Listen_True_SameEvent_TwiceNoExtraWorkerJob | Double subscription does not schedule duplicate jobs—both contexts are stored. |
| Subscribe_Listen_True_AddsContextToMap | Subscribe adds the notification context as expected to the internal map. |
| Subscribe_Listen_True_MixedCaseEvent_KeyIsLowercased | Event names are handled case-insensitively for mapping and lookups. |
| Subscribe_Listen_False_RemovesContextFromMap | Unsubscribe removes the subscriber, erasing the event when last is removed. |
| Subscribe_Listen_False_LastContext_EmitsUnsubscribeJob | If the last context for an event is removed, triggers an unsubscribe job. |
| Subscribe_Listen_False_NonExistentEvent_ReturnsNone | Unsubscribing from an event never subscribed does nothing (safe no-op). |
| Subscribe_Listen_False_OneOfTwoContextsRemoved | Only the specified context is removed when two share the event. |
| Subscribe_MultipleDistinctEvents_AllPresentInMap | A single connection can subscribe to several distinct events, all tracked. |
| Subscribe_EmptyModule_Listen_True_ReturnsNoneAndAddsToMap | Empty module value is accepted and handled. |
| Subscribe_EmptyEvent_Listen_True_ReturnsNoneAndAddsToMap | Supports subscription with empty event names. |
| Subscribe_PartialUnsubscribe_ThenResubscribe_Works | Partial unsubscribe works, and the context can later re-subscribe. |
| Subscribe_Listen_False_EmptyEvent_NoEntry_ReturnsNone | Unsubscribe from an empty event (never subscribed) is also a no-op. |

## Emission (Emit) Behavior

| Test ID | Description |
| --- | --- |
| Emit_ReturnsNoneImmediately | Emit returns immediately (async fire-and-forget pattern). |
| Emit_EmptyPayload_ReturnsNone | Handles emits with an empty payload string correctly. |
| Emit_EmptyAppId_ReturnsNone | Empty appId emits to all subscribers for that event. |
| Emit_EmptyEvent_ReturnsNone | Empty event name is safely accepted. |
| Emit_WithSubscriber_GatewayOrigin_DispatchesToGateway | Correctly dispatches for Gateway-origin subscriptions. |
| Emit_WithSubscriber_NonGatewayOrigin_DispatchesToLaunchDelegate | Dispatches to LaunchDelegate for non-Gateway origins as expected. |
| Emit_NoSubscribersForEvent_NoDispatch | If no one subscribes, nothing is dispatched but emit still succeeds. |
| Emit_WithAppIdFilter_OnlyDispatchesMatchingAppId | Only the specified appId gets the emitted event. |
| Emit_GatewayQueryFails_DoesNotCrash | Gracefully handles gateway plugin missing/failing. |
| Emit_LaunchDelegateQueryFails_DoesNotCrash | Handles delegate plugin failure/missing gracefully. |
| Emit_AfterCleanup_NoDispatch | After cleanup of all subscribers, emit does not dispatch to anyone. |
| Emit_EmptyAppId_DispatchesToAllSubscribers | If appId is empty, every context for the event receives the dispatch. |
| Emit_AppIdNoMatch_NoDispatch | If a subscriber doesn’t match appId, nothing is dispatched. |

## Cleanup Logic

| Test ID | Description |
| --- | --- |
| Cleanup_RemovesAllContextsMatchingConnectionIdAndOrigin | Removes all contexts matching a connectionId and origin. |
| Cleanup_ConnectionIdNotPresent_NoChange | Cleanup with a non-present connectionId does not alter subscriptions. |
| Cleanup_OriginMismatch_NoChange | Cleanup with a mismatching origin keeps subscriptions unchanged. |
| Cleanup_EmptyMap_ReturnsNone | Cleanup on an empty map is a safe no-op. |
| Cleanup_MultipleEventsForConnection_AllCleared | A connection with multiple event subscriptions is fully cleaned up. |
| Cleanup_ThenSubscribe_WorksCorrectly | Subscribe after cleanup works as if new. |
| Cleanup_EmptyOrigin_OnlyRemovesEmptyOriginContexts | Only contexts with an empty origin are removed when specified. |
| Cleanup_ZeroConnectionId_OnlyRemovesMatchingConnId | Only exact zero connectionId matches are cleaned up. |
| Cleanup_BothOriginAndConnIdMustMatch_ConnIdMatchOnly_NoRemoval | Both origin and connectionId must match to remove a context. |

## Internal SubscriberMap Logic

| Test ID | Description |
| --- | --- |
| SubscriberMap_Add_And_Exists | Adding and querying keys in the internal map works and is case-insensitive. |
| SubscriberMap_Get_ReturnsCorrectContexts | Get returns all correct contexts for an event. |
| SubscriberMap_Get_NonExistentKey_ReturnsEmpty | Getting a non-existent key returns an empty vector. |
| SubscriberMap_Remove_ExistingContext_KeyErasedWhenEmpty | Last-removed context also erases the key. |
| SubscriberMap_Remove_NonExistentKey_NoOp | Removing a key/context pair not present has no effect. |
| SubscriberMap_Exists_CaseInsensitive | Map exists/lookup operations ignore case. |
| SubscriberMap_CleanupNotifications_ByConnectionAndOrigin | Can bulk-remove all contexts for an event by connection/origin. |
| SubscriberMap_CleanupNotifications_ErasesKeyWhenAllRemoved | Key fully erased when all contexts for event are removed. |
| SubscriberMap_Remove_OnlyRemovesOneMatchingContext_WhenMultipleSameKey | Multiple identical contexts are all removed at once. |
| SubscriberMap_CleanupNotifications_ConnIdMatchOnly_OriginMismatch_NoRemoval | Only removes if both origin and connId match, otherwise leaves untouched. |
| SubscriberMap_DispatchToGateway_CachesInterface_QueryCalledOnce | Caches Gateway plugin instance to avoid redundant lookups. |
| SubscriberMap_DispatchToGateway_NullGateway_DoesNotCrash | Null gateway instance does not crash. |
| SubscriberMap_DispatchToLaunchDelegate_NullDelegate_DoesNotCrash | Null delegate plugin does not crash. |
| SubscriberMap_DispatchToLaunchDelegate_CachesInterface_QueryCalledOnce | Caches LaunchDelegate instance after first lookup. |
| SubscriberMap_EventUpdate_AppIdEmpty_DispatchesToAllSubscribers | EventUpdate with empty appId dispatches to all contexts. |
| SubscriberMap_EventUpdate_AppIdNonMatch_NoDispatch | EventUpdate ignores contexts that do not match the appId. |
| SubscriberMap_EventUpdate_NoSubscribersForKey_NoDispatch | No crash/dispatch if there are no subscribers in map. |

## ThunderSubscriptionManager (ThunderManager) Coverage

| Test ID | Description |
| --- | --- |
| ThunderManager_IsNotificationRegistered_FalseByDefault | Events are unregistered unless explicitly added. |
| ThunderManager_RegisterNotification_WhenHandlerAvailable | Registers a notification as expected when plugin handler is available. |
| ThunderManager_RegisterNotification_WhenHandlerUnavailable_NotRegistered | Notification is not registered if handler is missing. |
| ThunderManager_RegisterNotification_HandlerReturnsFalseStatus_NotRegistered | Handler can refuse registration, state remains unchanged. |
| ThunderManager_UnregisterNotification_WhenRegistered | Successfully unregisters previously registered notification. |
| ThunderManager_UnregisterNotification_WhenNotRegistered_NoOp | Unregister for never-registered event is a no-op. |
| ThunderManager_Subscribe_AlreadyRegistered_NoDuplicateRegistration | No duplicate registration if already subscribed. |
| ThunderManager_HandleNotifier_HandlerReturnsError_ReturnsFalse | Gracefully fails if handler returns error. |
| ThunderManager_HandleNotifier_ModuleNotAvailable_ReturnsFalse | Returns false if the module is missing. |
| ThunderManager_HandleNotifier_ThrowsException_PropagatesFromHandlerMock | Handles exceptions thrown from handler mock, propagates as expected. |
| ThunderManager_RegisterNotification_ThrowsException_Propagates | Registers exception is propagated if handler throws. |
| ThunderManager_UnregisterNotification_HandlerReturnsFalseStatus_NotRemovedFromList | Notification not removed if handler sets status to false. |
| ThunderManager_Unsubscribe_WhenRegistered_CallsUnregister | Calls unregister when previously registered. |
| ThunderManager_Unsubscribe_WhenNotRegistered_IsNoOp | Safe no-op when unsubscribing non-present event. |
| ThunderManager_HandleNotifier_HandlerReturnsErrorNoneButStatusFalse_ReturnsFalse | Handler returns ERROR_NONE but status=false; HandleNotifier returns false. |

## Emitter & NotificationHandler

| Test ID | Description |
| --- | --- |
| Emitter_Emit_SubmitsEmitJobToWorkerPool | Submits job to pool and does not throw. |
| Emitter_Emit_EmptyArguments_NocrashNoThrow | Handles empty input values gracefully. |
| Notification_EmitterEmit_WithSubscriber_GatewayOrigin_DispatchesViaGateway | Correct dispatch via Gateway when subscribing application is from Gateway. |
| Notification_EmitterEmit_WithSubscriber_NonGatewayOrigin_DispatchesViaLaunchDelegate | Correct dispatch via LaunchDelegate path. |
| Notification_EmitterEmit_NoSubscribers_NoDispatch | If no subscribers, nothing dispatched. |
| Notification_EmitterEmit_EmptyAppId_BroadcastsToAllSubscribers | When appId is empty, broadcasts to all. |
| Notification_EmitterEmit_AppIdFiltered_OnlyMatchingSubscriberDispatched | Only correct appId-subscriber gets dispatched to. |
| Notification_NotificationHandler_ReceivesEmit_DirectCall | Direct Emit call on handler stores parameters and signals event. |
| Notification_NotificationHandler_Reset_ClearsStoredState | Reset clears handler’s stored state. |
| Notification_NotificationHandler_WaitTimeout_ReturnsFalse_WhenNoEmit | Wait call times out if no emit is signalled. |
| Notification_NotificationHandler_MultipleEmitCalls_LastValueStored | Only most recent emit values are stored. |
| Notification_HandleNotifier_PassesCorrectEmitterToHandler | HandleNotifier passes the correct emitter pointer to plugins. |
| Notification_HandleNotifier_EmitterCanBeInvokedByHandler_TriggersEventUpdate | Handler can use emitter and notification path is exercised. |
| Notification_EndToEnd_EmitterEmit_NotificationHandler_Receives | End-to-end test: emitter to handler, verifies values. |
| Notification_EndToEnd_SubscribeEmitViaEmitter_GatewayDispatch | Full chain subscribe/emit dispatches via gateway. |
| Notification_EndToEnd_SubscribeEmitViaEmitter_LaunchDelegateDispatch | Full chain subscribe/emit via LaunchDelegate. |
| Notification_DispatchToGateway_GatewayContextFields_MatchSubscriberContext | GatewayContext fields in dispatch match subscriber. |
| Notification_DispatchToLaunchDelegate_GatewayContextFields_MatchSubscriberContext | LaunchDelegate context in dispatch matches subscriber. |

---

*This table is auto-aligned for developer/QA onboarding, code review, and easy traceability. Each test in `test_AppNotifications.cpp` is mapped here by ID and functional area.*
