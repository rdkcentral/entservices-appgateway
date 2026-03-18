/**
 * AppNotifications L0 Test Runner
 *
 * Main entry point for the AppNotifications L0 test suite.
 * Runs all 83 test cases (AN-L0-001 through AN-L0-083) across 7 test files.
 */

#include <iostream>
#include <string>

#include "L0Bootstrap.hpp"

// ---------------------------------------------------------------------------
// Extern declarations for all 83 test functions
// ---------------------------------------------------------------------------

// Init/Deinit tests (AN-L0-001 to AN-L0-013) — AppNotifications_Init_DeinitTests.cpp
extern uint32_t Test_AN_Initialize_Success();
extern uint32_t Test_AN_Initialize_FailNullImpl();
extern uint32_t Test_AN_Initialize_ConfigureInterface();
extern uint32_t Test_AN_Deinitialize_HappyPath();
extern uint32_t Test_AN_Deinitialize_NullImpl();
extern uint32_t Test_AN_Deinitialize_WithRemoteConnection();
extern uint32_t Test_AN_Deactivated_MatchingConnectionId();
extern uint32_t Test_AN_Deactivated_NonMatchingConnectionId();
extern uint32_t Test_AN_Constructor_Destructor_Lifecycle();
extern uint32_t Test_AN_Impl_Constructor_MemberInit();
extern uint32_t Test_AN_Impl_Destructor_ShellRelease();
extern uint32_t Test_AN_Impl_Destructor_NullShell();
extern uint32_t Test_AN_Configure_Success();

// Subscribe tests (AN-L0-014 to AN-L0-019) — AppNotifications_SubscribeTests.cpp
extern uint32_t Test_AN_Subscribe_FirstListener_TriggersThunderSub();
extern uint32_t Test_AN_Subscribe_SecondListener_NoThunderSub();
extern uint32_t Test_AN_Subscribe_CaseInsensitive();
extern uint32_t Test_AN_Unsubscribe_LastListener_TriggersThunderUnsub();
extern uint32_t Test_AN_Unsubscribe_NotLastListener_NoThunderUnsub();
extern uint32_t Test_AN_Unsubscribe_NonExistent_NoCrash();

// Emit/Cleanup tests (AN-L0-020 to AN-L0-026) — AppNotifications_EmitTests.cpp
extern uint32_t Test_AN_Emit_SubmitsJob();
extern uint32_t Test_AN_Emit_EmptyPayload();
extern uint32_t Test_AN_Emit_EmptyAppId();
extern uint32_t Test_AN_Cleanup_RemovesMatchingSubscribers();
extern uint32_t Test_AN_Cleanup_EmptiesEntireKey();
extern uint32_t Test_AN_Cleanup_NoMatch_NoCrash();
extern uint32_t Test_AN_Cleanup_MultipleEvents();

// SubscriberMap tests (AN-L0-027 to AN-L0-048) — AppNotifications_SubscriberMapTests.cpp
extern uint32_t Test_AN_SubscriberMap_Add_NewKey();
extern uint32_t Test_AN_SubscriberMap_Add_ExistingKey();
extern uint32_t Test_AN_SubscriberMap_Remove_ExistingContext();
extern uint32_t Test_AN_SubscriberMap_Remove_LastContext_ErasesKey();
extern uint32_t Test_AN_SubscriberMap_Remove_NonExistent_NoCrash();
extern uint32_t Test_AN_SubscriberMap_Get_Existing();
extern uint32_t Test_AN_SubscriberMap_Get_NonExistent();
extern uint32_t Test_AN_SubscriberMap_Exists_True();
extern uint32_t Test_AN_SubscriberMap_Exists_False();
extern uint32_t Test_AN_SubscriberMap_Exists_CaseInsensitive();
extern uint32_t Test_AN_EventUpdate_DispatchToAll_EmptyAppId();
extern uint32_t Test_AN_EventUpdate_FilterByAppId();
extern uint32_t Test_AN_EventUpdate_NoListeners_Warning();
extern uint32_t Test_AN_EventUpdate_VersionedEventKey();
extern uint32_t Test_AN_Dispatch_OriginGateway_RoutesToGateway();
extern uint32_t Test_AN_Dispatch_OriginNonGateway_RoutesToLaunchDelegate();
extern uint32_t Test_AN_DispatchToGateway_LazyAcquireSuccess();
extern uint32_t Test_AN_DispatchToGateway_LazyAcquireFailure();
extern uint32_t Test_AN_DispatchToGateway_CachedResponder();
extern uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquireSuccess();
extern uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquireFailure();
extern uint32_t Test_AN_DispatchToLaunchDelegate_CachedResponder();

// ThunderSubscriptionManager tests (AN-L0-049 to AN-L0-063) — AppNotifications_ThunderManagerTests.cpp
extern uint32_t Test_AN_ThunderMgr_Subscribe_NewEvent();
extern uint32_t Test_AN_ThunderMgr_Subscribe_AlreadyRegistered();
extern uint32_t Test_AN_ThunderMgr_Unsubscribe_Registered();
extern uint32_t Test_AN_ThunderMgr_Unsubscribe_NotRegistered();
extern uint32_t Test_AN_HandleNotifier_Success();
extern uint32_t Test_AN_HandleNotifier_HandlerNotAvailable();
extern uint32_t Test_AN_HandleNotifier_HandlerReturnsError();
extern uint32_t Test_AN_RegisterNotification_HandlesTrue();
extern uint32_t Test_AN_RegisterNotification_HandlesFalse();
extern uint32_t Test_AN_UnregisterNotification_HandleNotifierTrue();
extern uint32_t Test_AN_UnregisterNotification_HandleNotifierFalse();
extern uint32_t Test_AN_IsNotificationRegistered_Exists();
extern uint32_t Test_AN_IsNotificationRegistered_NotExists();
extern uint32_t Test_AN_IsNotificationRegistered_CaseInsensitive();
extern uint32_t Test_AN_ThunderMgr_Destructor_UnsubscribesAll();

// Context equality & dispatch job tests (AN-L0-064 to AN-L0-074) — AppNotifications_ContextEqualityTests.cpp
extern uint32_t Test_AN_SubscriberJob_Dispatch_Subscribe();
extern uint32_t Test_AN_SubscriberJob_Dispatch_Unsubscribe();
extern uint32_t Test_AN_EmitJob_Dispatch();
extern uint32_t Test_AN_Emitter_Emit_SubmitsJob();
extern uint32_t Test_AN_Context_Equal_AllFieldsMatch();
extern uint32_t Test_AN_Context_NotEqual_DifferentRequestId();
extern uint32_t Test_AN_Context_NotEqual_DifferentConnectionId();
extern uint32_t Test_AN_Context_NotEqual_DifferentAppId();
extern uint32_t Test_AN_Context_NotEqual_DifferentOrigin();
extern uint32_t Test_AN_Context_NotEqual_DifferentVersion();
extern uint32_t Test_AN_SubscriberMap_Destructor_ReleasesInterfaces();

// Boundary & interface map tests (AN-L0-075 to AN-L0-083) — AppNotifications_BoundaryTests.cpp
extern uint32_t Test_AN_Boundary_EmptyEventName();
extern uint32_t Test_AN_Boundary_EmptyModuleName();
extern uint32_t Test_AN_Boundary_LargePayload();
extern uint32_t Test_AN_Boundary_ZeroConnectionId();
extern uint32_t Test_AN_Boundary_MaxUint32ConnectionId();
extern uint32_t Test_AN_Boundary_ZeroRequestId();
extern uint32_t Test_AN_InterfaceMap_IAppNotifications();
extern uint32_t Test_AN_InterfaceMap_IPlugin();
extern uint32_t Test_AN_InterfaceMap_IDispatcher();

// ---------------------------------------------------------------------------
// Main test runner
// ---------------------------------------------------------------------------

int main()
{
    // RAII bootstrap: creates WorkerPool + optional Core::Messaging store.
    // Must be constructed before any plugin Initialize()/Invoke() calls.
    L0Test::L0BootstrapGuard bootstrap;

    struct Case {
        const char* name;
        uint32_t (*fn)();
    };

    const Case cases[] = {
        // --- Init/Deinit (AN-L0-001 to AN-L0-013) ---
        { "AN-L0-001 Initialize_Success",                       Test_AN_Initialize_Success },
        { "AN-L0-002 Initialize_FailNullImpl",                  Test_AN_Initialize_FailNullImpl },
        { "AN-L0-003 Initialize_ConfigureInterface",            Test_AN_Initialize_ConfigureInterface },
        { "AN-L0-004 Deinitialize_HappyPath",                   Test_AN_Deinitialize_HappyPath },
        { "AN-L0-005 Deinitialize_NullImpl",                    Test_AN_Deinitialize_NullImpl },
        { "AN-L0-006 Deinitialize_WithRemoteConnection",        Test_AN_Deinitialize_WithRemoteConnection },
        { "AN-L0-007 Deactivated_MatchingConnectionId",         Test_AN_Deactivated_MatchingConnectionId },
        { "AN-L0-008 Deactivated_NonMatchingConnectionId",      Test_AN_Deactivated_NonMatchingConnectionId },
        { "AN-L0-009 Constructor_Destructor_Lifecycle",          Test_AN_Constructor_Destructor_Lifecycle },
        { "AN-L0-010 Impl_Constructor_MemberInit",              Test_AN_Impl_Constructor_MemberInit },
        { "AN-L0-011 Impl_Destructor_ShellRelease",             Test_AN_Impl_Destructor_ShellRelease },
        { "AN-L0-012 Impl_Destructor_NullShell",                Test_AN_Impl_Destructor_NullShell },
        { "AN-L0-013 Configure_Success",                        Test_AN_Configure_Success },

        // --- Subscribe (AN-L0-014 to AN-L0-019) ---
        { "AN-L0-014 Subscribe_FirstListener_TriggersThunderSub",  Test_AN_Subscribe_FirstListener_TriggersThunderSub },
        { "AN-L0-015 Subscribe_SecondListener_NoThunderSub",       Test_AN_Subscribe_SecondListener_NoThunderSub },
        { "AN-L0-016 Subscribe_CaseInsensitive",                   Test_AN_Subscribe_CaseInsensitive },
        { "AN-L0-017 Unsubscribe_LastListener_TriggersThunderUnsub",  Test_AN_Unsubscribe_LastListener_TriggersThunderUnsub },
        { "AN-L0-018 Unsubscribe_NotLastListener_NoThunderUnsub",    Test_AN_Unsubscribe_NotLastListener_NoThunderUnsub },
        { "AN-L0-019 Unsubscribe_NonExistent_NoCrash",             Test_AN_Unsubscribe_NonExistent_NoCrash },

        // --- Emit/Cleanup (AN-L0-020 to AN-L0-026) ---
        { "AN-L0-020 Emit_SubmitsJob",                          Test_AN_Emit_SubmitsJob },
        { "AN-L0-021 Emit_EmptyPayload",                        Test_AN_Emit_EmptyPayload },
        { "AN-L0-022 Emit_EmptyAppId",                          Test_AN_Emit_EmptyAppId },
        { "AN-L0-023 Cleanup_RemovesMatchingSubscribers",       Test_AN_Cleanup_RemovesMatchingSubscribers },
        { "AN-L0-024 Cleanup_EmptiesEntireKey",                 Test_AN_Cleanup_EmptiesEntireKey },
        { "AN-L0-025 Cleanup_NoMatch_NoCrash",                  Test_AN_Cleanup_NoMatch_NoCrash },
        { "AN-L0-026 Cleanup_MultipleEvents",                   Test_AN_Cleanup_MultipleEvents },

        // --- SubscriberMap (AN-L0-027 to AN-L0-048) ---
        { "AN-L0-027 SubscriberMap_Add_NewKey",                 Test_AN_SubscriberMap_Add_NewKey },
        { "AN-L0-028 SubscriberMap_Add_ExistingKey",            Test_AN_SubscriberMap_Add_ExistingKey },
        { "AN-L0-029 SubscriberMap_Remove_ExistingContext",     Test_AN_SubscriberMap_Remove_ExistingContext },
        { "AN-L0-030 SubscriberMap_Remove_LastContext_ErasesKey",  Test_AN_SubscriberMap_Remove_LastContext_ErasesKey },
        { "AN-L0-031 SubscriberMap_Remove_NonExistent_NoCrash", Test_AN_SubscriberMap_Remove_NonExistent_NoCrash },
        { "AN-L0-032 SubscriberMap_Get_Existing",               Test_AN_SubscriberMap_Get_Existing },
        { "AN-L0-033 SubscriberMap_Get_NonExistent",            Test_AN_SubscriberMap_Get_NonExistent },
        { "AN-L0-034 SubscriberMap_Exists_True",                Test_AN_SubscriberMap_Exists_True },
        { "AN-L0-035 SubscriberMap_Exists_False",               Test_AN_SubscriberMap_Exists_False },
        { "AN-L0-036 SubscriberMap_Exists_CaseInsensitive",     Test_AN_SubscriberMap_Exists_CaseInsensitive },
        { "AN-L0-037 EventUpdate_DispatchToAll_EmptyAppId",     Test_AN_EventUpdate_DispatchToAll_EmptyAppId },
        { "AN-L0-038 EventUpdate_FilterByAppId",                Test_AN_EventUpdate_FilterByAppId },
        { "AN-L0-039 EventUpdate_NoListeners_Warning",          Test_AN_EventUpdate_NoListeners_Warning },
        { "AN-L0-040 EventUpdate_VersionedEventKey",            Test_AN_EventUpdate_VersionedEventKey },
        { "AN-L0-041 Dispatch_OriginGateway_RoutesToGateway",   Test_AN_Dispatch_OriginGateway_RoutesToGateway },
        { "AN-L0-042 Dispatch_OriginNonGateway_RoutesToLaunchDelegate", Test_AN_Dispatch_OriginNonGateway_RoutesToLaunchDelegate },
        { "AN-L0-043 DispatchToGateway_LazyAcquireSuccess",     Test_AN_DispatchToGateway_LazyAcquireSuccess },
        { "AN-L0-044 DispatchToGateway_LazyAcquireFailure",     Test_AN_DispatchToGateway_LazyAcquireFailure },
        { "AN-L0-045 DispatchToGateway_CachedResponder",        Test_AN_DispatchToGateway_CachedResponder },
        { "AN-L0-046 DispatchToLaunchDelegate_LazyAcquireSuccess",  Test_AN_DispatchToLaunchDelegate_LazyAcquireSuccess },
        { "AN-L0-047 DispatchToLaunchDelegate_LazyAcquireFailure",  Test_AN_DispatchToLaunchDelegate_LazyAcquireFailure },
        { "AN-L0-048 DispatchToLaunchDelegate_CachedResponder",    Test_AN_DispatchToLaunchDelegate_CachedResponder },

        // --- ThunderSubscriptionManager (AN-L0-049 to AN-L0-063) ---
        { "AN-L0-049 ThunderMgr_Subscribe_NewEvent",            Test_AN_ThunderMgr_Subscribe_NewEvent },
        { "AN-L0-050 ThunderMgr_Subscribe_AlreadyRegistered",   Test_AN_ThunderMgr_Subscribe_AlreadyRegistered },
        { "AN-L0-051 ThunderMgr_Unsubscribe_Registered",        Test_AN_ThunderMgr_Unsubscribe_Registered },
        { "AN-L0-052 ThunderMgr_Unsubscribe_NotRegistered",     Test_AN_ThunderMgr_Unsubscribe_NotRegistered },
        { "AN-L0-053 HandleNotifier_Success",                   Test_AN_HandleNotifier_Success },
        { "AN-L0-054 HandleNotifier_HandlerNotAvailable",       Test_AN_HandleNotifier_HandlerNotAvailable },
        { "AN-L0-055 HandleNotifier_HandlerReturnsError",       Test_AN_HandleNotifier_HandlerReturnsError },
        { "AN-L0-056 RegisterNotification_HandlesTrue",         Test_AN_RegisterNotification_HandlesTrue },
        { "AN-L0-057 RegisterNotification_HandlesFalse",        Test_AN_RegisterNotification_HandlesFalse },
        { "AN-L0-058 UnregisterNotification_HandleNotifierTrue",   Test_AN_UnregisterNotification_HandleNotifierTrue },
        { "AN-L0-059 UnregisterNotification_HandleNotifierFalse",  Test_AN_UnregisterNotification_HandleNotifierFalse },
        { "AN-L0-060 IsNotificationRegistered_Exists",          Test_AN_IsNotificationRegistered_Exists },
        { "AN-L0-061 IsNotificationRegistered_NotExists",       Test_AN_IsNotificationRegistered_NotExists },
        { "AN-L0-062 IsNotificationRegistered_CaseInsensitive", Test_AN_IsNotificationRegistered_CaseInsensitive },
        { "AN-L0-063 ThunderMgr_Destructor_UnsubscribesAll",    Test_AN_ThunderMgr_Destructor_UnsubscribesAll },

        // --- Context equality & dispatch jobs (AN-L0-064 to AN-L0-074) ---
        { "AN-L0-064 SubscriberJob_Dispatch_Subscribe",         Test_AN_SubscriberJob_Dispatch_Subscribe },
        { "AN-L0-065 SubscriberJob_Dispatch_Unsubscribe",       Test_AN_SubscriberJob_Dispatch_Unsubscribe },
        { "AN-L0-066 EmitJob_Dispatch",                         Test_AN_EmitJob_Dispatch },
        { "AN-L0-067 Emitter_Emit_SubmitsJob",                  Test_AN_Emitter_Emit_SubmitsJob },
        { "AN-L0-068 Context_Equal_AllFieldsMatch",             Test_AN_Context_Equal_AllFieldsMatch },
        { "AN-L0-069 Context_NotEqual_DifferentRequestId",      Test_AN_Context_NotEqual_DifferentRequestId },
        { "AN-L0-070 Context_NotEqual_DifferentConnectionId",   Test_AN_Context_NotEqual_DifferentConnectionId },
        { "AN-L0-071 Context_NotEqual_DifferentAppId",          Test_AN_Context_NotEqual_DifferentAppId },
        { "AN-L0-072 Context_NotEqual_DifferentOrigin",         Test_AN_Context_NotEqual_DifferentOrigin },
        { "AN-L0-073 Context_NotEqual_DifferentVersion",        Test_AN_Context_NotEqual_DifferentVersion },
        { "AN-L0-074 SubscriberMap_Destructor_ReleasesInterfaces",  Test_AN_SubscriberMap_Destructor_ReleasesInterfaces },

        // --- Boundary & interface map (AN-L0-075 to AN-L0-083) ---
        { "AN-L0-075 Boundary_EmptyEventName",                  Test_AN_Boundary_EmptyEventName },
        { "AN-L0-076 Boundary_EmptyModuleName",                 Test_AN_Boundary_EmptyModuleName },
        { "AN-L0-077 Boundary_LargePayload",                    Test_AN_Boundary_LargePayload },
        { "AN-L0-078 Boundary_ZeroConnectionId",                Test_AN_Boundary_ZeroConnectionId },
        { "AN-L0-079 Boundary_MaxUint32ConnectionId",           Test_AN_Boundary_MaxUint32ConnectionId },
        { "AN-L0-080 Boundary_ZeroRequestId",                   Test_AN_Boundary_ZeroRequestId },
        { "AN-L0-081 InterfaceMap_IAppNotifications",           Test_AN_InterfaceMap_IAppNotifications },
        { "AN-L0-082 InterfaceMap_IPlugin",                     Test_AN_InterfaceMap_IPlugin },
        { "AN-L0-083 InterfaceMap_IDispatcher",                 Test_AN_InterfaceMap_IDispatcher },
    };

    const size_t totalCases = sizeof(cases) / sizeof(cases[0]);
    uint32_t totalFailures = 0;
    uint32_t passed = 0;
    uint32_t failed = 0;

    std::cerr << "[==========] Running " << totalCases << " AppNotifications L0 test(s)." << std::endl;

    for (size_t i = 0; i < totalCases; ++i) {
        const auto& c = cases[i];
        std::cerr << "[ RUN      ] " << c.name << std::endl;
        const uint32_t f = c.fn();
        if (f == 0) {
            std::cerr << "[       OK ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cerr << "[  FAILED  ] " << c.name << " failures=" << f << std::endl;
            ++failed;
        }
        totalFailures += f;
    }

    std::cerr << "[==========] " << totalCases << " test(s) ran." << std::endl;
    std::cerr << "[  PASSED  ] " << passed << " test(s)." << std::endl;
    if (failed > 0) {
        std::cerr << "[  FAILED  ] " << failed << " test(s), " << totalFailures << " total assertion failure(s)." << std::endl;
    }

    // Ensure any remaining Thunder Core singletons are explicitly disposed before process exit.
    WPEFramework::Core::Singleton::Dispose();

    if (totalFailures == 0) {
        std::cout << "AppNotifications l0test passed." << std::endl;
        return 0;
    }

    std::cerr << "AppNotifications l0test total failures: " << totalFailures << std::endl;
    return static_cast<int>(totalFailures);
}
