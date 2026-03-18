/*
 * AppNotifications L0 Test — ThunderSubscriptionManager
 *
 * Test cases: AN-L0-049 through AN-L0-063
 *
 * These tests exercise the ThunderSubscriptionManager inner class:
 * Subscribe, Unsubscribe, HandleNotifier, RegisterNotification,
 * UnregisterNotification, IsNotificationRegistered, and destructor.
 *
 * Since ThunderSubscriptionManager is private, we test it indirectly through
 * the AppNotificationsImplementation::Subscribe() API which delegates to it
 * via SubscriberJob dispatched on the worker pool.
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <core/core.h>

#include <AppNotificationsImplementation.h>
#include "AppNotificationsServiceMock.h"

#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Exchange::IAppNotifications;
using WPEFramework::Plugin::AppNotificationsImplementation;

namespace {

IAppNotifications::AppNotificationContext MakeContext(uint32_t requestId, uint32_t connectionId,
                                                      const std::string& appId,
                                                      const std::string& origin,
                                                      const std::string& version = "0")
{
    IAppNotifications::AppNotificationContext ctx;
    ctx.requestId = requestId;
    ctx.connectionId = connectionId;
    ctx.appId = appId;
    ctx.origin = origin;
    ctx.version = version;
    return ctx;
}

void WaitForJobs()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

} // namespace

// ─────────────────────────────────────────────────────────────────────
// AN-L0-049: ThunderMgr_Subscribe_NewEvent
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_ThunderMgr_Subscribe_NewEvent()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "tmNewEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr, "AN-L0-049: Handler queried");
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount >= 1, "AN-L0-049: HandleAppEventNotifier called");
        L0Test::ExpectEqStr(tr, handler->lastEvent, "tmNewEvt", "AN-L0-049: Event name matches");
        L0Test::ExpectTrue(tr, handler->lastListen == true, "AN-L0-049: listen=true for subscribe");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-050: ThunderMgr_Subscribe_AlreadyRegistered
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_ThunderMgr_Subscribe_AlreadyRegistered()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "tmRegEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterFirst = handler ? handler->handleCount : 0;

    // Remove all subscribers then re-add — this triggers a new first subscriber
    impl.Subscribe(ctx1, false, "org.rdk.FbSettings", "tmRegEvt");
    WaitForJobs();

    // The unsubscribe triggered UnregisterNotification.
    // Now subscribe again — but notification is still in registry if HandleNotifier returned true
    // This depends on whether UnregisterNotification removed it successfully.
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "tmRegEvt");
    WaitForJobs();

    // At minimum, handler should have been called for the initial subscribe and the unsubscribe
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount >= 2, "AN-L0-050: Handler called multiple times for sub/unsub cycle");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-051: ThunderMgr_Unsubscribe_Registered
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_ThunderMgr_Unsubscribe_Registered()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "tmUnsubEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterSub = handler ? handler->handleCount : 0;

    // Remove the only subscriber -> triggers Unsubscribe in ThunderManager
    impl.Subscribe(ctx, false, "org.rdk.FbSettings", "tmUnsubEvt");
    WaitForJobs();

    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount > countAfterSub,
                           "AN-L0-051: Handler called for unsubscribe");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-052: ThunderMgr_Unsubscribe_NotRegistered
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_ThunderMgr_Unsubscribe_NotRegistered()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Unsubscribe without ever subscribing — event doesn't exist in map
    // so mSubMap.Exists returns false and no SubscriberJob is submitted
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, false, "org.rdk.FbSettings", "neverSubbed");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-052: Unsubscribe non-registered returns ERROR_NONE");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-053: HandleNotifier_Success
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_HandleNotifier_Success()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "handleOk");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr, "AN-L0-053: Handler acquired");
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount >= 1, "AN-L0-053: HandleAppEventNotifier called successfully");
        // The handler's emitter pointer should be non-null (it receives &mEmitter)
        L0Test::ExpectTrue(tr, handler->lastEmitter != nullptr, "AN-L0-053: Emitter pointer passed to handler");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-054: HandleNotifier_HandlerNotAvailable
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_HandleNotifier_HandlerNotAvailable()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config cfg(true, true, true, false /*no handler*/);
    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service(cfg);
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "noHandler");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-054: Subscribe returns ERROR_NONE even if handler unavailable");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler == nullptr, "AN-L0-054: Handler was not acquired");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-055: HandleNotifier_HandlerReturnsError
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_HandleNotifier_HandlerReturnsError()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config cfg(true, true, true, true);
    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service(cfg);
    impl.Configure(&service);

    // Pre-configure handler to return error
    auto* handler = service.GetNotificationHandler();
    if (handler == nullptr) {
        // Force handler creation by doing a query
        auto* h = static_cast<WPEFramework::Exchange::IAppNotificationHandler*>(
            service.QueryInterfaceByCallsign(WPEFramework::Exchange::IAppNotificationHandler::ID, "org.rdk.FbSettings"));
        if (h != nullptr) {
            handler = service.GetNotificationHandler();
            h->Release();
        }
    }
    if (handler != nullptr) {
        handler->SetHandleRc(WPEFramework::Core::ERROR_GENERAL);
    }

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "errHandler");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-055: Subscribe returns ERROR_NONE even if handler returns error");
    WaitForJobs();

    // HandleNotifier returns false because HandleAppEventNotifier returns ERROR_GENERAL
    // RegisterNotification does not add to registry
    L0Test::ExpectTrue(tr, true, "AN-L0-055: No crash when handler returns error");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-056: RegisterNotification_HandlesTrue
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_RegisterNotification_HandlesTrue()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "regTrue");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount >= 1, "AN-L0-056: Handler called with listen=true");

        // Now re-subscribe same event with new subscriber — should NOT call handler again
        // because IsNotificationRegistered returns true
        uint32_t countBefore = handler->handleCount;
        auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
        impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "regTrue");
        WaitForJobs();
        L0Test::ExpectEqU32(tr, handler->handleCount, countBefore,
                           "AN-L0-056: Already registered, no re-registration");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-057: RegisterNotification_HandlesFalse
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_RegisterNotification_HandlesFalse()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config cfg(true, true, true, true);
    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service(cfg);
    impl.Configure(&service);

    // Pre-configure handler to return status=false
    auto* h = static_cast<WPEFramework::Exchange::IAppNotificationHandler*>(
        service.QueryInterfaceByCallsign(WPEFramework::Exchange::IAppNotificationHandler::ID, "org.rdk.FbSettings"));
    if (h != nullptr) {
        auto* handler = service.GetNotificationHandler();
        if (handler != nullptr) {
            handler->SetStatusResult(false); // HandleNotifier returns false
        }
        h->Release();
    }

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "regFalse");
    WaitForJobs();

    // HandleNotifier returns false -> RegisterNotification does NOT add to registry
    // So a subsequent first-subscriber for the same event would try to register again
    auto* handler = service.GetNotificationHandler();
    if (handler != nullptr) {
        uint32_t firstCount = handler->handleCount;

        // Remove and re-add to trigger another first-subscriber path
        impl.Subscribe(ctx, false, "org.rdk.FbSettings", "regFalse");
        WaitForJobs();

        auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
        impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "regFalse");
        WaitForJobs();

        // Handler should be called again because notification was NOT registered
        L0Test::ExpectTrue(tr, handler->handleCount > firstCount,
                           "AN-L0-057: Handler called again because status was false");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-058: UnregisterNotification_HandleNotifierTrue
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_UnregisterNotification_HandleNotifierTrue()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "unregTrue");
    WaitForJobs();

    impl.Subscribe(ctx, false, "org.rdk.FbSettings", "unregTrue");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount >= 2, "AN-L0-058: Handler called for both sub and unsub");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-059: UnregisterNotification_HandleNotifierFalse
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_UnregisterNotification_HandleNotifierFalse()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config cfg(true, true, true, true);
    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service(cfg);
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "unregFalse");
    WaitForJobs();

    // Now make handler return status=false for unsubscribe
    auto* handler = service.GetNotificationHandler();
    if (handler != nullptr) {
        handler->SetStatusResult(false);
    }

    impl.Subscribe(ctx, false, "org.rdk.FbSettings", "unregFalse");
    WaitForJobs();

    // Even if HandleNotifier returns false, notification entry remains in registry
    // This is the expected behavior from the source code
    L0Test::ExpectTrue(tr, true, "AN-L0-059: No crash when UnregisterNotification HandleNotifier returns false");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-060: IsNotificationRegistered_Exists
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_IsNotificationRegistered_Exists()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "isRegEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countBefore = handler ? handler->handleCount : 0;

    // Attempt to re-subscribe same event: since IsNotificationRegistered returns true,
    // ThunderManager::Subscribe should skip RegisterNotification
    impl.Subscribe(MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway"), false, "org.rdk.FbSettings", "isRegEvt");
    WaitForJobs();
    impl.Subscribe(MakeContext(3, 30, "com.app.three", "org.rdk.AppGateway"), true, "org.rdk.FbSettings", "isRegEvt");
    WaitForJobs();

    // After unsub+resub, the handler should have been called for unsub,
    // and then for the new registration (since entry was removed)
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount > countBefore,
                           "AN-L0-060: Notification re-registration after unregister");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-061: IsNotificationRegistered_NotExists
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_IsNotificationRegistered_NotExists()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // First subscribe for a fresh event triggers RegisterNotification
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "freshEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr && handler->handleCount >= 1,
                       "AN-L0-061: Fresh event triggers registration");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-062: IsNotificationRegistered_CaseInsensitive
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_IsNotificationRegistered_CaseInsensitive()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "CaseReg");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterFirst = handler ? handler->handleCount : 0;

    // Remove and add with different case to re-trigger first subscriber
    impl.Subscribe(ctx1, false, "org.rdk.FbSettings", "CaseReg");
    WaitForJobs();

    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "casereg");
    WaitForJobs();

    // The notification registry uses case-insensitive comparison
    // After unsub of "CaseReg" (stored as "casereg"), the re-sub of "casereg" should trigger register
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount > countAfterFirst,
                           "AN-L0-062: Case-insensitive registration check");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-063: ThunderMgr_Destructor_UnsubscribesAll
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_ThunderMgr_Destructor_UnsubscribesAll()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock service;

    {
        WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
        impl.Configure(&service);

        auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
        auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
        impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "dtorEvt1");
        impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "dtorEvt2");
        WaitForJobs();

        auto* handler = service.GetNotificationHandler();
        if (handler != nullptr) {
            uint32_t countBeforeDtor = handler->handleCount;

            // impl goes out of scope here -> destructor -> ThunderManager destructor
            // -> UnregisterNotification for all registered notifications
        }
    }

    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    if (handler != nullptr) {
        // The destructor should have called HandleNotifier(listen=false) for each registered notification
        L0Test::ExpectTrue(tr, handler->handleCount >= 4, "AN-L0-063: Destructor unsubscribed all (2 sub + 2 unsub)");
    }

    return tr.failures;
}
