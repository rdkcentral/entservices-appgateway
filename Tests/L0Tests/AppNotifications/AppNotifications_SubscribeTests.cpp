/*
 * AppNotifications L0 Test — Subscribe / Unsubscribe
 *
 * Test cases: AN-L0-014 through AN-L0-019
 *
 * These tests exercise the Subscribe/Unsubscribe paths of
 * AppNotificationsImplementation, including first/last listener
 * Thunder subscription triggers.
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

// Small delay to allow async jobs (SubscriberJob) to complete on the worker pool
void WaitForJobs()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

} // namespace

// ─────────────────────────────────────────────────────────────────────
// AN-L0-014: Subscribe_FirstListener_TriggersThunderSub
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Subscribe_FirstListener_TriggersThunderSub()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onFoo");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-014: Subscribe returns ERROR_NONE");

    // Allow the async SubscriberJob to execute
    WaitForJobs();

    // The SubscriberJob calls ThunderSubscriptionManager::Subscribe which calls
    // RegisterNotification -> HandleNotifier -> QueryInterfaceByCallsign<IAppNotificationHandler>
    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr, "AN-L0-014: NotificationHandler was queried");
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount >= 1, "AN-L0-014: HandleAppEventNotifier called at least once");
        L0Test::ExpectEqStr(tr, handler->lastEvent, "onFoo", "AN-L0-014: Handler received event 'onFoo'");
        L0Test::ExpectTrue(tr, handler->lastListen == true, "AN-L0-014: Handler received listen=true");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-015: Subscribe_SecondListener_NoThunderSub
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Subscribe_SecondListener_NoThunderSub()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "onFoo");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterFirst = handler ? handler->handleCount : 0;

    // Second subscriber for the same event
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "onFoo");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-015: Second subscribe returns ERROR_NONE");

    WaitForJobs();

    // Handler should NOT have been called again (event already exists in map)
    if (handler != nullptr) {
        L0Test::ExpectEqU32(tr, handler->handleCount, countAfterFirst, "AN-L0-015: Handler NOT called again for second subscriber");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-016: Subscribe_CaseInsensitive
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Subscribe_CaseInsensitive()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "OnFoo");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterFirst = handler ? handler->handleCount : 0;

    // Subscribe with different case — should be treated as same event
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "onfoo");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-016: Case-insensitive subscribe returns ERROR_NONE");

    WaitForJobs();

    // Because "onfoo" is same as "OnFoo" (lowered), event already Exists() in map
    if (handler != nullptr) {
        L0Test::ExpectEqU32(tr, handler->handleCount, countAfterFirst, "AN-L0-016: Handler NOT called again for case-variant");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-017: Unsubscribe_LastListener_TriggersThunderUnsub
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Unsubscribe_LastListener_TriggersThunderUnsub()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onBar");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterSub = handler ? handler->handleCount : 0;

    // Unsubscribe — this is the last listener, so SubscriberJob(subscribe=false) should fire
    uint32_t rc = impl.Subscribe(ctx, false, "org.rdk.FbSettings", "onBar");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-017: Unsubscribe returns ERROR_NONE");

    WaitForJobs();

    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount > countAfterSub,
                           "AN-L0-017: Handler called again for unsubscribe (listen=false)");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-018: Unsubscribe_NotLastListener_NoThunderUnsub
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Unsubscribe_NotLastListener_NoThunderUnsub()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");

    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "onBaz");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "onBaz");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterSub = handler ? handler->handleCount : 0;

    // Remove first subscriber — not the last one
    uint32_t rc = impl.Subscribe(ctx1, false, "org.rdk.FbSettings", "onBaz");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-018: Unsubscribe returns ERROR_NONE");

    WaitForJobs();

    if (handler != nullptr) {
        L0Test::ExpectEqU32(tr, handler->handleCount, countAfterSub,
                           "AN-L0-018: Handler NOT called when not-last listener removed");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-019: Unsubscribe_NonExistent_NoCrash
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Unsubscribe_NonExistent_NoCrash()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Unsubscribe from an event that was never subscribed
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, false, "org.rdk.FbSettings", "nonExistentEvent");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-019: Unsubscribe for non-existent event returns ERROR_NONE");

    return tr.failures;
}
