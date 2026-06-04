/*
 * AppActionsTest.cpp
 * Main test runner for AppActions L0 tests.
 *
 * Pattern follows Tests/L0Tests/AppNotifications/AppNotificationsTest.cpp
 */

#include <iostream>
#include <string>

#include <core/core.h>
#include <plugins/IShell.h>

#include <AppActions.h>
#include "AppActionsServiceMock.h"
#include "L0Bootstrap.hpp"
#include "L0TestTypes.hpp"

// -----------------------------------------------------------------------
// Forward declarations for tests in AppActions_Init_DeinitTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AA_Initialize_Success();
extern uint32_t Test_AA_Initialize_FailNullImpl();
extern uint32_t Test_AA_Initialize_RegistersJsonRpc();
extern uint32_t Test_AA_Deinitialize_HappyPath();
extern uint32_t Test_AA_Deinitialize_NullImpl();
extern uint32_t Test_AA_Constructor_Destructor_Lifecycle();
extern uint32_t Test_AA_Information_ReturnsEmpty();
extern uint32_t Test_AA_Deactivated_MatchingConnectionId();
extern uint32_t Test_AA_Deactivated_NonMatchingConnectionId();
extern uint32_t Test_AA_Initialize_Twice_Idempotent();
extern uint32_t Test_AA_Deinitialize_Twice_NoCrash();
extern uint32_t Test_AA_ServiceRegistration_Asserts();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppActions_ActionStartTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AA_ActionStart_ValidParams();
extern uint32_t Test_AA_ActionStart_EmptyInitiator();
extern uint32_t Test_AA_ActionStart_EmptyIntent();
extern uint32_t Test_AA_ActionStart_EmptyHandlerAppId();
extern uint32_t Test_AA_ActionStart_AllEmptyParams();
extern uint32_t Test_AA_ActionStart_DispatchesToNotification();
extern uint32_t Test_AA_ActionStart_MultipleNotifications();
extern uint32_t Test_AA_ActionStart_NoNotifications();
extern uint32_t Test_AA_ActionStart_SpecialCharsInitiator();
extern uint32_t Test_AA_ActionStart_JsonIntent();
extern uint32_t Test_AA_ActionStart_LongStrings();
extern uint32_t Test_AA_ActionStart_UnicodeChars();
extern uint32_t Test_AA_ActionStart_MultipleCalls();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppActions_NotificationTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AA_Register_Success();
extern uint32_t Test_AA_Register_DuplicateReturnsError();
extern uint32_t Test_AA_Register_MultipleDifferent();
extern uint32_t Test_AA_Unregister_Success();
extern uint32_t Test_AA_Unregister_NotRegistered();
extern uint32_t Test_AA_Unregister_Twice();
extern uint32_t Test_AA_Unregister_NoLongerReceivesEvents();
extern uint32_t Test_AA_Register_AfterUnregister();
extern uint32_t Test_AA_Unregister_PartialFromMultiple();
extern uint32_t Test_AA_Register_ThreadSafety();
extern uint32_t Test_AA_Register_AddsRefCount();

// -----------------------------------------------------------------------
// Forward declarations for tests in AppActions_ImplTests.cpp
// -----------------------------------------------------------------------
extern uint32_t Test_AA_Impl_Configure_ValidService();
extern uint32_t Test_AA_Impl_Configure_NullService();
extern uint32_t Test_AA_Impl_Initialize_CallsConfigure();
extern uint32_t Test_AA_Impl_Deinitialize_ReleasesService();
extern uint32_t Test_AA_Impl_Information_ReturnsEmpty();
extern uint32_t Test_AA_Impl_InterfaceMap_IPlugin();
extern uint32_t Test_AA_Impl_InterfaceMap_IAppActions();
extern uint32_t Test_AA_Impl_InterfaceMap_IConfiguration();
extern uint32_t Test_AA_Impl_Dispatch_NoNotifications();
extern uint32_t Test_AA_Impl_Dispatch_WithNotifications();
extern uint32_t Test_AA_Impl_Constructor_Destructor();
extern uint32_t Test_AA_Impl_DoubleConfigure();
extern uint32_t Test_AA_Impl_Deinitialize_WithoutInitialize();

int main()
{
    // Bootstrap the Thunder Core WorkerPool for this test process.
    L0Test::L0BootstrapGuard bootstrap;

    struct Case {
        const char* name;
        uint32_t (*fn)();
    };

    const Case cases[] = {
        // Init / Deinit tests (AA-L0-001 to AA-L0-012)
        { "AA_Initialize_Success",                    Test_AA_Initialize_Success                    },
        { "AA_Initialize_FailNullImpl",               Test_AA_Initialize_FailNullImpl               },
        { "AA_Initialize_RegistersJsonRpc",           Test_AA_Initialize_RegistersJsonRpc           },
        { "AA_Deinitialize_HappyPath",                Test_AA_Deinitialize_HappyPath                },
        { "AA_Deinitialize_NullImpl",                 Test_AA_Deinitialize_NullImpl                 },
        { "AA_Constructor_Destructor_Lifecycle",      Test_AA_Constructor_Destructor_Lifecycle      },
        { "AA_Information_ReturnsEmpty",              Test_AA_Information_ReturnsEmpty              },
        { "AA_Deactivated_MatchingConnectionId",      Test_AA_Deactivated_MatchingConnectionId      },
        { "AA_Deactivated_NonMatchingConnectionId",   Test_AA_Deactivated_NonMatchingConnectionId   },
        { "AA_Initialize_Twice_Idempotent",           Test_AA_Initialize_Twice_Idempotent           },
        { "AA_Deinitialize_Twice_NoCrash",            Test_AA_Deinitialize_Twice_NoCrash            },
        { "AA_ServiceRegistration_Asserts",           Test_AA_ServiceRegistration_Asserts           },

        // ActionStart tests (AA-L0-020 to AA-L0-032)
        { "AA_ActionStart_ValidParams",               Test_AA_ActionStart_ValidParams               },
        { "AA_ActionStart_EmptyInitiator",            Test_AA_ActionStart_EmptyInitiator            },
        { "AA_ActionStart_EmptyIntent",               Test_AA_ActionStart_EmptyIntent               },
        { "AA_ActionStart_EmptyHandlerAppId",         Test_AA_ActionStart_EmptyHandlerAppId         },
        { "AA_ActionStart_AllEmptyParams",            Test_AA_ActionStart_AllEmptyParams            },
        { "AA_ActionStart_DispatchesToNotification",  Test_AA_ActionStart_DispatchesToNotification  },
        { "AA_ActionStart_MultipleNotifications",     Test_AA_ActionStart_MultipleNotifications     },
        { "AA_ActionStart_NoNotifications",           Test_AA_ActionStart_NoNotifications           },
        { "AA_ActionStart_SpecialCharsInitiator",     Test_AA_ActionStart_SpecialCharsInitiator     },
        { "AA_ActionStart_JsonIntent",                Test_AA_ActionStart_JsonIntent                },
        { "AA_ActionStart_LongStrings",               Test_AA_ActionStart_LongStrings               },
        { "AA_ActionStart_UnicodeChars",              Test_AA_ActionStart_UnicodeChars              },
        { "AA_ActionStart_MultipleCalls",             Test_AA_ActionStart_MultipleCalls             },

        // Notification tests (AA-L0-040 to AA-L0-050)
        { "AA_Register_Success",                      Test_AA_Register_Success                      },
        { "AA_Register_DuplicateReturnsError",        Test_AA_Register_DuplicateReturnsError        },
        { "AA_Register_MultipleDifferent",            Test_AA_Register_MultipleDifferent            },
        { "AA_Unregister_Success",                    Test_AA_Unregister_Success                    },
        { "AA_Unregister_NotRegistered",              Test_AA_Unregister_NotRegistered              },
        { "AA_Unregister_Twice",                      Test_AA_Unregister_Twice                      },
        { "AA_Unregister_NoLongerReceivesEvents",     Test_AA_Unregister_NoLongerReceivesEvents     },
        { "AA_Register_AfterUnregister",              Test_AA_Register_AfterUnregister              },
        { "AA_Unregister_PartialFromMultiple",        Test_AA_Unregister_PartialFromMultiple        },
        { "AA_Register_ThreadSafety",                 Test_AA_Register_ThreadSafety                 },
        { "AA_Register_AddsRefCount",                 Test_AA_Register_AddsRefCount                 },

        // Implementation tests (AA-L0-060 to AA-L0-072)
        { "AA_Impl_Configure_ValidService",           Test_AA_Impl_Configure_ValidService           },
        { "AA_Impl_Configure_NullService",            Test_AA_Impl_Configure_NullService            },
        { "AA_Impl_Initialize_CallsConfigure",        Test_AA_Impl_Initialize_CallsConfigure        },
        { "AA_Impl_Deinitialize_ReleasesService",     Test_AA_Impl_Deinitialize_ReleasesService     },
        { "AA_Impl_Information_ReturnsEmpty",         Test_AA_Impl_Information_ReturnsEmpty         },
        { "AA_Impl_InterfaceMap_IPlugin",             Test_AA_Impl_InterfaceMap_IPlugin             },
        { "AA_Impl_InterfaceMap_IAppActions",         Test_AA_Impl_InterfaceMap_IAppActions         },
        { "AA_Impl_InterfaceMap_IConfiguration",      Test_AA_Impl_InterfaceMap_IConfiguration      },
        { "AA_Impl_Dispatch_NoNotifications",         Test_AA_Impl_Dispatch_NoNotifications         },
        { "AA_Impl_Dispatch_WithNotifications",       Test_AA_Impl_Dispatch_WithNotifications       },
        { "AA_Impl_Constructor_Destructor",           Test_AA_Impl_Constructor_Destructor           },
        { "AA_Impl_DoubleConfigure",                  Test_AA_Impl_DoubleConfigure                  },
        { "AA_Impl_Deinitialize_WithoutInitialize",   Test_AA_Impl_Deinitialize_WithoutInitialize   },
    };

    uint32_t totalFailures = 0;
    const size_t numCases = sizeof(cases) / sizeof(cases[0]);

    std::cout << "========================================" << std::endl;
    std::cout << "AppActions L0 Tests - " << numCases << " test cases" << std::endl;
    std::cout << "========================================" << std::endl;

    for (size_t i = 0; i < numCases; ++i) {
        std::cout << "[" << (i + 1) << "/" << numCases << "] Running: " << cases[i].name << " ... ";
        std::cout.flush();

        const uint32_t failures = cases[i].fn();
        totalFailures += failures;

        if (0 == failures) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED (" << failures << " failure(s))" << std::endl;
        }
    }

    std::cout << "========================================" << std::endl;
    L0Test::PrintTotals(std::cout, "AppActions L0 Tests", totalFailures);
    std::cout << "========================================" << std::endl;

    return L0Test::ResultToExitCode(totalFailures);
}
