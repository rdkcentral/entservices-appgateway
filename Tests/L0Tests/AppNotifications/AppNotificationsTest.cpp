/*
 * AppNotificationsTest.cpp
 * Main test runner for AppNotifications L0 tests.
 *
 * Pattern follows Tests/L0Tests/AppGateway/AppGatewayTest.cpp
 */

#include <iostream>
#include <string>

#include <core/core.h>
#include <plugins/IShell.h>

#include <AppNotifications.h>
#include "AppNotificationsServiceMock.h"
#include "L0Bootstrap.hpp"
#include "L0TestTypes.hpp"

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_Init_DeinitTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AN_Initialize_Success();
extern uint32_t Test_AN_Initialize_FailNullImpl();
extern uint32_t Test_AN_Initialize_ConfigureInterface();
extern uint32_t Test_AN_Deinitialize_HappyPath();
extern uint32_t Test_AN_Deinitialize_NullImpl();
extern uint32_t Test_AN_Constructor_Destructor_Lifecycle();
extern uint32_t Test_AN_Information_ReturnsEmpty();
extern uint32_t Test_AN_Deactivated_MatchingConnectionId();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_SubscribeTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AN_Subscribe_FirstListener_TriggersThunderSub();
extern uint32_t Test_AN_Subscribe_SecondListener_NoThunderSub();
extern uint32_t Test_AN_Subscribe_CaseInsensitive();
extern uint32_t Test_AN_Unsubscribe_LastListener_TriggersThunderUnsub();
extern uint32_t Test_AN_Unsubscribe_NotLastListener_NoThunderUnsub();
extern uint32_t Test_AN_Unsubscribe_NonExistent_NoCrash();
extern uint32_t Test_AN_Cleanup_RemovesMatchingSubscribers();
extern uint32_t Test_AN_Cleanup_EmptiesEntireKey();
extern uint32_t Test_AN_Cleanup_NoMatch_NoCrash();
extern uint32_t Test_AN_Cleanup_MultipleEvents();
extern uint32_t Test_AN_Cleanup_PartialMatch_KeepsOthers();
extern uint32_t Test_AN_Cleanup_DifferentOrigin_NoMatch();
extern uint32_t Test_AN_Cleanup_EmptyMap_NoCrash();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_EmitTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AN_Emit_SubmitsJob();
extern uint32_t Test_AN_Emit_EmptyPayload();
extern uint32_t Test_AN_Emit_EmptyAppId();
extern uint32_t Test_AN_Configure_Success();
extern uint32_t Test_AN_Configure_DoubleConfigure();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_SubscriberMapTests.cpp
// -----------------------------------------------------------------------
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
extern uint32_t Test_AN_EventUpdate_NoListeners_LogWarning();
extern uint32_t Test_AN_Dispatch_OriginGateway();
extern uint32_t Test_AN_Dispatch_OriginNonGateway();
extern uint32_t Test_AN_DispatchToGateway_LazyAcquire_Success();
extern uint32_t Test_AN_DispatchToGateway_LazyAcquire_Failure();
extern uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquire_Success();
extern uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquire_Failure();
extern uint32_t Test_AN_EventUpdate_VersionedEventName();
extern uint32_t Test_AN_EventUpdate_AppId_NonMatch_Skipped();
extern uint32_t Test_AN_DispatchToGateway_CachedResponder_Reuse();
extern uint32_t Test_AN_DispatchToLaunchDelegate_CachedResponder_Reuse();
extern uint32_t Test_AN_SubscriberMap_Destructor_ReleasesResponders();
extern uint32_t Test_AN_Emit_MixedOrigins_DispatchBoth();
extern uint32_t Test_AN_EventUpdate_NonVersionedEventName();
extern uint32_t Test_AN_Emit_SpecificAppId_MatchesOne();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_ThunderManagerTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AN_ThunderMgr_Subscribe_NewEvent();
extern uint32_t Test_AN_ThunderMgr_Subscribe_AlreadyRegistered();
extern uint32_t Test_AN_ThunderMgr_Unsubscribe_RegisteredEvent();
extern uint32_t Test_AN_ThunderMgr_Unsubscribe_NotRegistered();
extern uint32_t Test_AN_HandleNotifier_Success();
extern uint32_t Test_AN_HandleNotifier_HandlerNotAvailable();
extern uint32_t Test_AN_HandleNotifier_HandlerReturnsError();
extern uint32_t Test_AN_RegisterNotification_HandleNotifier_ReturnsTrue();
extern uint32_t Test_AN_RegisterNotification_HandleNotifier_ReturnsFalse();
extern uint32_t Test_AN_UnregisterNotification_HandleNotifier_ReturnsTrue();
extern uint32_t Test_AN_UnregisterNotification_HandleNotifier_ReturnsFalse();
extern uint32_t Test_AN_IsNotificationRegistered_Exists();
extern uint32_t Test_AN_IsNotificationRegistered_NotExists();
extern uint32_t Test_AN_IsNotificationRegistered_CaseInsensitive();
extern uint32_t Test_AN_ThunderMgr_Destructor_UnsubscribesAll();
extern uint32_t Test_AN_Emitter_Emit_Callback();
extern uint32_t Test_AN_SubscriberJob_Dispatch_Subscribe();
extern uint32_t Test_AN_SubscriberJob_Dispatch_Unsubscribe();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_ContextEqualityTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AN_AppNotificationContext_Equality();
extern uint32_t Test_AN_AppNotificationContext_Inequality_RequestId();
extern uint32_t Test_AN_AppNotificationContext_Inequality_ConnectionId();
extern uint32_t Test_AN_AppNotificationContext_Inequality_AppId();
extern uint32_t Test_AN_AppNotificationContext_Inequality_Origin();
extern uint32_t Test_AN_AppNotificationContext_Inequality_Version();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppNotifications_BoundaryTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AN_Boundary_Subscribe_EmptyEvent();
extern uint32_t Test_AN_Boundary_Emit_LargePayload();
extern uint32_t Test_AN_Boundary_Cleanup_ZeroConnectionId();
extern uint32_t Test_AN_Boundary_MaxUint32_ConnectionId();
extern uint32_t Test_AN_Boundary_MaxUint32_RequestId();
extern uint32_t Test_AN_Impl_Destructor_NullShell();
extern uint32_t Test_AN_Impl_Destructor_NonNullShell();
extern uint32_t Test_AN_Subscribe_EmptyModule();


int main()
{
    // Bootstrap the Thunder Core WorkerPool for this test process.
    L0Test::L0BootstrapGuard bootstrap;

    struct Case {
        const char* name;
        uint32_t (*fn)();
    };

    const Case cases[] = {
        // Init / Deinit tests
        { "AN_Initialize_Success",                            Test_AN_Initialize_Success                            },
        { "AN_Initialize_FailNullImpl",                       Test_AN_Initialize_FailNullImpl                       },
        { "AN_Initialize_ConfigureInterface",                 Test_AN_Initialize_ConfigureInterface                 },
        { "AN_Deinitialize_HappyPath",                       Test_AN_Deinitialize_HappyPath                        },
        { "AN_Deinitialize_NullImpl",                        Test_AN_Deinitialize_NullImpl                         },
        { "AN_Constructor_Destructor_Lifecycle",             Test_AN_Constructor_Destructor_Lifecycle              },
        { "AN_Information_ReturnsEmpty",                     Test_AN_Information_ReturnsEmpty                      },
        { "AN_Deactivated_MatchingConnectionId",             Test_AN_Deactivated_MatchingConnectionId              },

        // Subscribe / Unsubscribe / Cleanup tests
        { "AN_Subscribe_FirstListener_TriggersThunderSub",   Test_AN_Subscribe_FirstListener_TriggersThunderSub    },
        { "AN_Subscribe_SecondListener_NoThunderSub",        Test_AN_Subscribe_SecondListener_NoThunderSub         },
        { "AN_Subscribe_CaseInsensitive",                    Test_AN_Subscribe_CaseInsensitive                     },
        { "AN_Unsubscribe_LastListener_TriggersThunderUnsub",Test_AN_Unsubscribe_LastListener_TriggersThunderUnsub },
        { "AN_Unsubscribe_NotLastListener_NoThunderUnsub",   Test_AN_Unsubscribe_NotLastListener_NoThunderUnsub    },
        { "AN_Unsubscribe_NonExistent_NoCrash",              Test_AN_Unsubscribe_NonExistent_NoCrash               },
        { "AN_Cleanup_RemovesMatchingSubscribers",           Test_AN_Cleanup_RemovesMatchingSubscribers            },
        { "AN_Cleanup_EmptiesEntireKey",                     Test_AN_Cleanup_EmptiesEntireKey                      },
        { "AN_Cleanup_NoMatch_NoCrash",                      Test_AN_Cleanup_NoMatch_NoCrash                       },
        { "AN_Cleanup_MultipleEvents",                       Test_AN_Cleanup_MultipleEvents                        },
        { "AN_Cleanup_PartialMatch_KeepsOthers",             Test_AN_Cleanup_PartialMatch_KeepsOthers              },
        { "AN_Cleanup_DifferentOrigin_NoMatch",              Test_AN_Cleanup_DifferentOrigin_NoMatch               },
        { "AN_Cleanup_EmptyMap_NoCrash",                     Test_AN_Cleanup_EmptyMap_NoCrash                      },

        // Emit / Configure tests
        { "AN_Emit_SubmitsJob",                              Test_AN_Emit_SubmitsJob                               },
        { "AN_Emit_EmptyPayload",                            Test_AN_Emit_EmptyPayload                             },
        { "AN_Emit_EmptyAppId",                              Test_AN_Emit_EmptyAppId                               },
        { "AN_Configure_Success",                            Test_AN_Configure_Success                             },
        { "AN_Configure_DoubleConfigure",                    Test_AN_Configure_DoubleConfigure                     },

        // SubscriberMap tests
        { "AN_SubscriberMap_Add_NewKey",                     Test_AN_SubscriberMap_Add_NewKey                      },
        { "AN_SubscriberMap_Add_ExistingKey",                Test_AN_SubscriberMap_Add_ExistingKey                 },
        { "AN_SubscriberMap_Remove_ExistingContext",         Test_AN_SubscriberMap_Remove_ExistingContext          },
        { "AN_SubscriberMap_Remove_LastContext_ErasesKey",   Test_AN_SubscriberMap_Remove_LastContext_ErasesKey    },
        { "AN_SubscriberMap_Remove_NonExistent_NoCrash",     Test_AN_SubscriberMap_Remove_NonExistent_NoCrash      },
        { "AN_SubscriberMap_Get_Existing",                   Test_AN_SubscriberMap_Get_Existing                    },
        { "AN_SubscriberMap_Get_NonExistent",                Test_AN_SubscriberMap_Get_NonExistent                 },
        { "AN_SubscriberMap_Exists_True",                    Test_AN_SubscriberMap_Exists_True                     },
        { "AN_SubscriberMap_Exists_False",                   Test_AN_SubscriberMap_Exists_False                    },
        { "AN_SubscriberMap_Exists_CaseInsensitive",         Test_AN_SubscriberMap_Exists_CaseInsensitive          },
        { "AN_EventUpdate_DispatchToAll_EmptyAppId",         Test_AN_EventUpdate_DispatchToAll_EmptyAppId          },
        { "AN_EventUpdate_FilterByAppId",                    Test_AN_EventUpdate_FilterByAppId                     },
        { "AN_EventUpdate_NoListeners_LogWarning",           Test_AN_EventUpdate_NoListeners_LogWarning            },
        { "AN_Dispatch_OriginGateway",                       Test_AN_Dispatch_OriginGateway                        },
        { "AN_Dispatch_OriginNonGateway",                    Test_AN_Dispatch_OriginNonGateway                     },
        { "AN_DispatchToGateway_LazyAcquire_Success",        Test_AN_DispatchToGateway_LazyAcquire_Success         },
        { "AN_DispatchToGateway_LazyAcquire_Failure",        Test_AN_DispatchToGateway_LazyAcquire_Failure         },
        { "AN_DispatchToLaunchDelegate_LazyAcquire_Success", Test_AN_DispatchToLaunchDelegate_LazyAcquire_Success  },
        { "AN_DispatchToLaunchDelegate_LazyAcquire_Failure", Test_AN_DispatchToLaunchDelegate_LazyAcquire_Failure  },
        { "AN_EventUpdate_VersionedEventName",               Test_AN_EventUpdate_VersionedEventName                },
        { "AN_EventUpdate_AppId_NonMatch_Skipped",           Test_AN_EventUpdate_AppId_NonMatch_Skipped            },
        { "AN_DispatchToGateway_CachedResponder_Reuse",      Test_AN_DispatchToGateway_CachedResponder_Reuse        },
        { "AN_DispatchToLaunchDelegate_CachedResponder_Reuse", Test_AN_DispatchToLaunchDelegate_CachedResponder_Reuse },
        { "AN_SubscriberMap_Destructor_ReleasesResponders",  Test_AN_SubscriberMap_Destructor_ReleasesResponders   },
        { "AN_Emit_MixedOrigins_DispatchBoth",               Test_AN_Emit_MixedOrigins_DispatchBoth                },
        { "AN_EventUpdate_NonVersionedEventName",            Test_AN_EventUpdate_NonVersionedEventName             },
        { "AN_Emit_SpecificAppId_MatchesOne",                Test_AN_Emit_SpecificAppId_MatchesOne                 },

        // ThunderSubscriptionManager tests
        { "AN_ThunderMgr_Subscribe_NewEvent",                Test_AN_ThunderMgr_Subscribe_NewEvent                 },
        { "AN_ThunderMgr_Subscribe_AlreadyRegistered",       Test_AN_ThunderMgr_Subscribe_AlreadyRegistered        },
        { "AN_ThunderMgr_Unsubscribe_RegisteredEvent",       Test_AN_ThunderMgr_Unsubscribe_RegisteredEvent        },
        { "AN_ThunderMgr_Unsubscribe_NotRegistered",         Test_AN_ThunderMgr_Unsubscribe_NotRegistered          },
        { "AN_HandleNotifier_Success",                       Test_AN_HandleNotifier_Success                        },
        { "AN_HandleNotifier_HandlerNotAvailable",           Test_AN_HandleNotifier_HandlerNotAvailable            },
        { "AN_HandleNotifier_HandlerReturnsError",           Test_AN_HandleNotifier_HandlerReturnsError            },
        { "AN_RegisterNotification_HandleNotifier_True",     Test_AN_RegisterNotification_HandleNotifier_ReturnsTrue },
        { "AN_RegisterNotification_HandleNotifier_False",    Test_AN_RegisterNotification_HandleNotifier_ReturnsFalse },
        { "AN_UnregisterNotification_HandleNotifier_True",   Test_AN_UnregisterNotification_HandleNotifier_ReturnsTrue },
        { "AN_UnregisterNotification_HandleNotifier_False",  Test_AN_UnregisterNotification_HandleNotifier_ReturnsFalse },
        { "AN_IsNotificationRegistered_Exists",              Test_AN_IsNotificationRegistered_Exists               },
        { "AN_IsNotificationRegistered_NotExists",           Test_AN_IsNotificationRegistered_NotExists            },
        { "AN_IsNotificationRegistered_CaseInsensitive",     Test_AN_IsNotificationRegistered_CaseInsensitive      },
        { "AN_ThunderMgr_Destructor_UnsubscribesAll",        Test_AN_ThunderMgr_Destructor_UnsubscribesAll         },
        { "AN_Emitter_Emit_Callback",                        Test_AN_Emitter_Emit_Callback                         },
        { "AN_SubscriberJob_Dispatch_Subscribe",             Test_AN_SubscriberJob_Dispatch_Subscribe              },
        { "AN_SubscriberJob_Dispatch_Unsubscribe",           Test_AN_SubscriberJob_Dispatch_Unsubscribe            },

        // Context equality tests
        { "AN_AppNotificationContext_Equality",              Test_AN_AppNotificationContext_Equality               },
        { "AN_AppNotificationContext_Inequality_RequestId",  Test_AN_AppNotificationContext_Inequality_RequestId   },
        { "AN_AppNotificationContext_Inequality_ConnectionId", Test_AN_AppNotificationContext_Inequality_ConnectionId },
        { "AN_AppNotificationContext_Inequality_AppId",      Test_AN_AppNotificationContext_Inequality_AppId       },
        { "AN_AppNotificationContext_Inequality_Origin",     Test_AN_AppNotificationContext_Inequality_Origin      },
        { "AN_AppNotificationContext_Inequality_Version",    Test_AN_AppNotificationContext_Inequality_Version     },

        // Boundary tests
        { "AN_Boundary_Subscribe_EmptyEvent",                Test_AN_Boundary_Subscribe_EmptyEvent                 },
        { "AN_Boundary_Emit_LargePayload",                   Test_AN_Boundary_Emit_LargePayload                    },
        { "AN_Boundary_Cleanup_ZeroConnectionId",            Test_AN_Boundary_Cleanup_ZeroConnectionId             },
        { "AN_Boundary_MaxUint32_ConnectionId",              Test_AN_Boundary_MaxUint32_ConnectionId               },
        { "AN_Boundary_MaxUint32_RequestId",                 Test_AN_Boundary_MaxUint32_RequestId                  },
        { "AN_Impl_Destructor_NullShell",                    Test_AN_Impl_Destructor_NullShell                     },
        { "AN_Impl_Destructor_NonNullShell",                 Test_AN_Impl_Destructor_NonNullShell                  },
        { "AN_Subscribe_EmptyModule",                        Test_AN_Subscribe_EmptyModule                         },
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

    L0Test::PrintTotals(std::cerr, "AppNotifications l0test", failures);
    return L0Test::ResultToExitCode(failures);
}
