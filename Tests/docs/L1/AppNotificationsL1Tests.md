# AppNotifications L1 Tests

This document lists and briefly describes all Level 1 (L1) test cases implemented for the AppNotifications plugin in the `test_AppNotifications.cpp` file.

## Overview

- **Test file**: `Tests/L1Tests/tests/test_AppNotifications.cpp`
- **Total tests**: 155 (142 in `AppNotificationsTest` fixture + 13 in `AppNotificationsPluginTest` fixture)
- **Frameworks**: Google Test, Google Mock
- **Coverage targets**: ≥80% line coverage, ≥80% function coverage

## Test Case Index

Below are all top-level test cases (by Google Test name) for AppNotifications L1, grouped by feature area:

---

### 1. Configuration (AppNotificationsTest)
1. `Configure_StoresShellAndAddsRef`
2. `Configure_CalledTwice_ReleasesOldShellAndAddsRefNew`

### 2. Subscribe API (AppNotificationsTest)
3. `Subscribe_Listen_True_NewEvent_ReturnsNone`
4. `Subscribe_Listen_True_SameEvent_TwiceNoExtraWorkerJob`
5. `Subscribe_Listen_True_AddsContextToMap`
6. `Subscribe_Listen_True_MixedCaseEvent_KeyIsLowercased`
7. `Subscribe_Listen_False_RemovesContextFromMap`
8. `Subscribe_Listen_False_LastContext_EmitsUnsubscribeJob`
9. `Subscribe_Listen_False_NonExistentEvent_ReturnsNone`
10. `Subscribe_Listen_False_OneOfTwoContextsRemoved`
11. `Subscribe_MultipleDistinctEvents_AllPresentInMap`
12. `Subscribe_EmptyModule_Listen_True_ReturnsNoneAndAddsToMap`
13. `Subscribe_EmptyEvent_Listen_True_ReturnsNoneAndAddsToMap`
14. `Subscribe_PartialUnsubscribe_ThenResubscribe_Works`
15. `Subscribe_Listen_False_EmptyEvent_NoEntry_ReturnsNone`
16. `Subscribe_RDK8Version_AddsContextWithVersionField`
17. `Subscribe_EventWithSpecialChars_WorksCorrectly`
18. `Subscribe_MultipleVersions_BothStoredCorrectly`

### 3. Emit API (AppNotificationsTest)
19. `Emit_ReturnsNoneImmediately`
20. `Emit_EmptyPayload_ReturnsNone`
21. `Emit_EmptyAppId_ReturnsNone`
22. `Emit_EmptyEvent_ReturnsNone`
23. `Emit_WithSubscriber_GatewayOrigin_DispatchesToGateway`
24. `Emit_WithSubscriber_NonGatewayOrigin_DispatchesToLaunchDelegate`
25. `Emit_NoSubscribersForEvent_NoDispatch`
26. `Emit_WithAppIdFilter_OnlyDispatchesMatchingAppId`
27. `Emit_GatewayQueryFails_DoesNotCrash`
28. `Emit_LaunchDelegateQueryFails_DoesNotCrash`
29. `Emit_AfterCleanup_NoDispatch`
30. `Emit_EmptyAppId_DispatchesToAllSubscribers`
31. `Emit_AppIdNoMatch_NoDispatch`
32. `Emit_MultipleTimes_AllDispatchedCorrectly`

### 4. Cleanup API (AppNotificationsTest)
33. `Cleanup_RemovesAllContextsMatchingConnectionIdAndOrigin`
34. `Cleanup_ConnectionIdNotPresent_NoChange`
35. `Cleanup_OriginMismatch_NoChange`
36. `Cleanup_EmptyMap_ReturnsNone`
37. `Cleanup_MultipleEventsForConnection_AllCleared`
38. `Cleanup_ThenSubscribe_WorksCorrectly`
39. `Cleanup_EmptyOrigin_OnlyRemovesEmptyOriginContexts`
40. `Cleanup_ZeroConnectionId_OnlyRemovesMatchingConnId`
41. `Cleanup_BothOriginAndConnIdMustMatch_ConnIdMatchOnly_NoRemoval`
42. `Cleanup_LastSubscriberRemoved_TriggersUnsubscribeJob`
43. `Cleanup_LargeNumberOfEventsAndConnections_AllCleared`
44. `Cleanup_SameConnectionId_DifferentOrigins_OnlyMatchingOriginRemoved`

### 5. Internal SubscriberMap (AppNotificationsTest)
45. `SubscriberMap_Add_And_Exists`
46. `SubscriberMap_Get_ReturnsCorrectContexts`
47. `SubscriberMap_Get_NonExistentKey_ReturnsEmpty`
48. `SubscriberMap_Remove_ExistingContext_KeyErasedWhenEmpty`
49. `SubscriberMap_Remove_NonExistentKey_NoOp`
50. `SubscriberMap_Exists_CaseInsensitive`
51. `SubscriberMap_CleanupNotifications_ByConnectionAndOrigin`
52. `SubscriberMap_CleanupNotifications_ErasesKeyWhenAllRemoved`
53. `SubscriberMap_Remove_OnlyRemovesOneMatchingContext_WhenMultipleSameKey`
54. `SubscriberMap_CleanupNotifications_ConnIdMatchOnly_OriginMismatch_NoRemoval`
55. `SubscriberMap_DispatchToGateway_CachesInterface_QueryCalledOnce`
56. `SubscriberMap_DispatchToGateway_NullGateway_DoesNotCrash`
57. `SubscriberMap_DispatchToLaunchDelegate_NullDelegate_DoesNotCrash`
58. `SubscriberMap_DispatchToLaunchDelegate_CachesInterface_QueryCalledOnce`
59. `SubscriberMap_EventUpdate_AppIdEmpty_DispatchesToAllSubscribers`
60. `SubscriberMap_EventUpdate_AppIdNonMatch_NoDispatch`
61. `SubscriberMap_EventUpdate_NoSubscribersForKey_NoDispatch`
62. `SubscriberMap_Destructor_ReleasesCachedAppGateway`
63. `SubscriberMap_Destructor_ReleasesCachedLaunchDelegate`
64. `SubscriberMap_Add_DuplicateContext_BothStored`
65. `SubscriberMap_Remove_NonMatchingContext_NoRemoval`
66. `SubscriberMap_CleanupNotifications_MultipleKeys_SomeEmptied`

### 6. ThunderSubscriptionManager (AppNotificationsTest)
67. `ThunderManager_IsNotificationRegistered_FalseByDefault`
68. `ThunderManager_RegisterNotification_WhenHandlerAvailable`
69. `ThunderManager_RegisterNotification_WhenHandlerUnavailable_NotRegistered`
70. `ThunderManager_RegisterNotification_HandlerReturnsFalseStatus_NotRegistered`
71. `ThunderManager_UnregisterNotification_WhenRegistered`
72. `ThunderManager_UnregisterNotification_WhenNotRegistered_NoOp`
73. `ThunderManager_Subscribe_AlreadyRegistered_NoDuplicateRegistration`
74. `ThunderManager_HandleNotifier_HandlerReturnsError_ReturnsFalse`
75. `ThunderManager_HandleNotifier_ModuleNotAvailable_ReturnsFalse`
76. `ThunderManager_HandleNotifier_ThrowsException_PropagatesFromHandlerMock`
77. `ThunderManager_RegisterNotification_ThrowsException_Propagates`
78. `ThunderManager_UnregisterNotification_HandlerReturnsFalseStatus_NotRemovedFromList`
79. `ThunderManager_Unsubscribe_WhenRegistered_CallsUnregister`
80. `ThunderManager_Unsubscribe_WhenNotRegistered_IsNoOp`
81. `ThunderManager_HandleNotifier_HandlerReturnsErrorNoneButStatusFalse_ReturnsFalse`
82. `ThunderManager_HandleNotifier_SuccessPath_StatusTrue_ReturnsTrue`
83. `ThunderManager_HandleNotifier_Unsubscribe_SuccessPath`
84. `ThunderManager_HandleNotifier_HandlerReturnsNonZeroHresult_StatusSetTrue_ReturnsFalse`
85. `ThunderManager_Subscribe_HandlerUnavailable_NotRegistered`
86. `ThunderManager_UnregisterNotification_HandlerReturnsError_StillRegistered`
87. `ThunderManager_UnregisterNotification_HandlerUnavailable_StillRegistered`
88. `ThunderManager_Destructor_UnregistersAllNotifications`
89. `ThunderManager_RegisterNotification_StoresLowerCaseEvent`
90. `ThunderManager_IsNotificationRegistered_WrongModule_ReturnsFalse`

### 7. Emitter/Notification Flow/End-to-End (AppNotificationsTest)
91. `Emitter_Emit_SubmitsEmitJobToWorkerPool`
92. `Emitter_Emit_EmptyArguments_NocrashNoThrow`
93. `EndToEnd_SubscribeEmitCleanup_GatewayOrigin`
94. `EndToEnd_MultipleSubscribersEmitOneAppId`
95. `Notification_EmitterEmit_WithSubscriber_GatewayOrigin_DispatchesViaGateway`
96. `Notification_EmitterEmit_WithSubscriber_NonGatewayOrigin_DispatchesViaLaunchDelegate`
97. `Notification_EmitterEmit_NoSubscribers_NoDispatch`
98. `Notification_EmitterEmit_EmptyAppId_BroadcastsToAllSubscribers`
99. `Notification_EmitterEmit_AppIdFiltered_OnlyMatchingSubscriberDispatched`
100. `Notification_NotificationHandler_ReceivesEmit_DirectCall`
101. `Notification_NotificationHandler_Reset_ClearsStoredState`
102. `Notification_NotificationHandler_WaitTimeout_ReturnsFalse_WhenNoEmit`
103. `Notification_NotificationHandler_MultipleEmitCalls_LastValueStored`
104. `Notification_HandleNotifier_PassesCorrectEmitterToHandler`
105. `Notification_HandleNotifier_EmitterCanBeInvokedByHandler_TriggersEventUpdate`
106. `Notification_EndToEnd_EmitterEmit_NotificationHandler_Receives`
107. `Notification_EndToEnd_SubscribeEmitViaEmitter_GatewayDispatch`
108. `Notification_EndToEnd_SubscribeEmitViaEmitter_LaunchDelegateDispatch`
109. `Notification_DispatchToGateway_GatewayContextFields_MatchSubscriberContext`
110. `Notification_DispatchToLaunchDelegate_GatewayContextFields_MatchSubscriberContext`
111. `Notification_NotificationHandler_AddRefRelease_RefCountingWorks`
112. `Notification_DispatchToGateway_GatewayContextVersion_MatchesSubscriberVersion`
113. `Notification_EndToEnd_SubscribeEmitViaEmitter_GatewayDispatch` (version propagation)
114. `EndToEnd_SubscribeEmitCleanup_LaunchDelegateOrigin`
115. `EndToEnd_SubscribeWithHandler_EmitViaEmitter_Cleanup`

### 8. Constructor/Destructor (AppNotificationsTest)
116. `Constructor_DefaultState_ShellIsNullBeforeConfigure`
117. `Destructor_ReleasesShell_WhenShellIsNotNull`

### 9. AppNotificationContext Equality (AppNotificationsTest)
118. `AppNotificationContext_Equality_AllFieldsMatch_ReturnsTrue`
119. `AppNotificationContext_Equality_RequestIdDiffers_ReturnsFalse`
120. `AppNotificationContext_Equality_ConnectionIdDiffers_ReturnsFalse`
121. `AppNotificationContext_Equality_AppIdDiffers_ReturnsFalse`
122. `AppNotificationContext_Equality_OriginDiffers_ReturnsFalse`
123. `AppNotificationContext_Equality_VersionDiffers_ReturnsFalse`

### 10. SubscriberJob/EmitJob Dispatch (AppNotificationsTest)
124. `SubscriberJob_Dispatch_Subscribe_True_CallsThunderManagerSubscribe`
125. `SubscriberJob_Dispatch_Subscribe_False_CallsThunderManagerUnsubscribe`
126. `EmitJob_Dispatch_CallsEventUpdate`

### 11. EventUpdate & Dispatch Routing (AppNotificationsTest)
127. `EventUpdate_VersionedEventKey_V8Suffix_StrippedForDispatch`
128. `EventUpdate_NonVersionedEventKey_DispatchedAsIs`
129. `Dispatch_GatewayOrigin_RoutesToDispatchToGateway`
130. `Dispatch_NonGatewayOrigin_RoutesToDispatchToLaunchDelegate`
131. `Dispatch_CustomOrigin_RoutesToLaunchDelegate`
132. `EventUpdate_MixedOrigins_DispatchesToCorrectResponders`
133. `EventUpdate_NoSubscribers_WarningLoggedAndNoDispatch`
134. `EventUpdate_CaseInsensitiveKey_DispatchesCorrectly`

### 12. Payload Forwarding & Error Resilience (AppNotificationsTest)
135. `DispatchToGateway_PayloadIsForwardedCorrectly`
136. `DispatchToLaunchDelegate_PayloadIsForwardedCorrectly`
137. `DispatchToGateway_EmitReturnsError_DoesNotCrash`
138. `DispatchToLaunchDelegate_EmitReturnsError_DoesNotCrash`

### 13. QueryInterface — Implementation (AppNotificationsTest)
139. `QueryInterface_IAppNotifications_ReturnsNonNull`
140. `QueryInterface_IConfiguration_ReturnsNonNull`
141. `QueryInterface_UnknownId_ReturnsNull`

### 14. QueryInterface — Emitter (AppNotificationsTest)
142. `Emitter_QueryInterface_IEmitter_ReturnsNonNull`
143. `Emitter_QueryInterface_UnknownId_ReturnsNull`

---

### 15. Plugin Shell Tests (AppNotificationsPluginTest)

These tests cover `AppNotifications.cpp` (the plugin shell class) and `AppNotifications.h` (the plugin class declaration with `BEGIN_INTERFACE_MAP`). They use `Core::Sink<Plugin::AppNotifications>` for instantiation and custom `ICOMLink` implementations for controlling `IShell::Root<>()` behavior.

144. `Information_ReturnsEmptyString` — Verifies `Information()` returns an empty string.
145. `Initialize_Success_ReturnsEmptyString` — Verifies successful `Initialize()` returns empty string and sets `mAppNotifications`.
146. `Initialize_Deinitialize_TwoCycles_NoLeak` — Two full init/deinit cycles without crash or resource leak.
147. `Deinitialize_WithRemoteConnection_TerminatesAndReleasesConnection` — Verifies `Deinitialize` calls `Terminate()` on the remote connection.
148. `Deinitialize_WithoutRemoteConnection_NoTerminate` — Verifies `Deinitialize` handles null remote connection gracefully.
149. `Deactivated_MatchingConnectionId_SubmitsDeactivateJob` — Matching connection ID triggers deactivation job submission.
150. `Deactivated_NonMatchingConnectionId_DoesNothing` — Non-matching connection ID is a no-op.
151. `Constructor_InitializesFieldsToDefaults` — Verifies default field values: `mService=nullptr`, `mAppNotifications=nullptr`, `mConnectionId=0`.
152. `Destructor_DoesNotCrash` — Construct and immediately destroy without crash.
153. `QueryInterface_IPlugin_ReturnsNonNull` — `BEGIN_INTERFACE_MAP` returns valid `IPlugin` pointer.
154. `QueryInterface_IDispatcher_ReturnsNonNull` — `BEGIN_INTERFACE_MAP` returns valid `IDispatcher` pointer.
155. `QueryInterface_IAppNotifications_Aggregate_ReturnsNonNull` — `INTERFACE_AGGREGATE` delegates to `mAppNotifications` and returns valid pointer.
156. `QueryInterface_UnknownInterface_ReturnsNull` — Unknown interface ID returns nullptr.

---

### Notes
- This document was auto-generated from test case function names in `Tests/L1Tests/tests/test_AppNotifications.cpp`.
- The `AppNotificationsTest` fixture tests `AppNotificationsImplementation.cpp/.h` using a `Core::Sink<AppNotificationsImplementation>` member.
- The `AppNotificationsPluginTest` fixture tests `AppNotifications.cpp/.h` using `Core::Sink<Plugin::AppNotifications>` with custom `ICOMLink` implementations (`TestCOMLink`, `FailingCOMLink`) and `TestRemoteConnection`.
- Some tests cover edge cases, error handling paths, and the internal state of the implementation.
- See the code for full details, test cases, and comments.
