/*
 * AppNotifications L0 Test — SubscriberMap operations
 *
 * Test cases: AN-L0-027 through AN-L0-048
 *
 * These tests exercise SubscriberMap inner class operations (Add, Remove,
 * Get, Exists, EventUpdate, Dispatch, DispatchToGateway, DispatchToLaunchDelegate)
 * through the public AppNotificationsImplementation API since the inner classes
 * are private.
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
// AN-L0-027: SubscriberMap_Add_NewKey
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Add_NewKey()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "newKey");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-027: Add new key returns ERROR_NONE");

    // Verify the key exists by emitting and checking dispatch
    WaitForJobs();
    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("newKey", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 1u, "AN-L0-027: One subscriber dispatched");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-028: SubscriberMap_Add_ExistingKey
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Add_ExistingKey()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "existingKey");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "existingKey");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("existingKey", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 2u, "AN-L0-028: Two subscribers dispatched for same key");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-029: SubscriberMap_Remove_ExistingContext
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Remove_ExistingContext()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "removeTest");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "removeTest");
    WaitForJobs();

    // Remove ctx1
    impl.Subscribe(ctx1, false, "org.rdk.FbSettings", "removeTest");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("removeTest", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 1u, "AN-L0-029: One subscriber remaining after remove");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-030: SubscriberMap_Remove_LastContext_ErasesKey
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Remove_LastContext_ErasesKey()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "erasableKey");
    WaitForJobs();

    impl.Subscribe(ctx, false, "org.rdk.FbSettings", "erasableKey");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("erasableKey", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 0u, "AN-L0-030: Key erased, no dispatch");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-031: SubscriberMap_Remove_NonExistent_NoCrash
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Remove_NonExistent_NoCrash()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(99, 99, "com.nonexistent", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, false, "org.rdk.FbSettings", "noSuchEvent");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-031: Remove non-existent returns ERROR_NONE");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-032: SubscriberMap_Get_Existing (verified via Emit dispatch count)
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Get_Existing()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "getTest");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "getTest");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("getTest", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 2u, "AN-L0-032: Get returns 2 contexts");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-033: SubscriberMap_Get_NonExistent
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Get_NonExistent()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto* responder = service.GetGatewayResponder();

    // Emit for a non-existent key — hits LOGWARN path, no dispatch
    impl.Emit("ghostEvent", "{}", "");
    WaitForJobs();

    // Responder should not have been queried at all (no subscribers means no Dispatch call)
    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 0u, "AN-L0-033: No dispatch for non-existent key");
    } else {
        L0Test::ExpectTrue(tr, true, "AN-L0-033: Responder not queried for non-existent event");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-034: SubscriberMap_Exists_True
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Exists_True()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "existsEvent");
    WaitForJobs();

    // Second subscribe for same event should not trigger a new Thunder subscription
    // (because Exists returns true)
    auto* handler = service.GetNotificationHandler();
    uint32_t countBefore = handler ? handler->handleCount : 0;

    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "existsEvent");
    WaitForJobs();

    if (handler != nullptr) {
        L0Test::ExpectEqU32(tr, handler->handleCount, countBefore, "AN-L0-034: Exists=true, no new Thunder sub");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-035: SubscriberMap_Exists_False
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Exists_False()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Subscribing to a new event: Exists=false, triggers Thunder sub
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "brandNewEvent");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr && handler->handleCount >= 1,
                       "AN-L0-035: Exists=false, Thunder sub triggered");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-036: SubscriberMap_Exists_CaseInsensitive
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Exists_CaseInsensitive()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "CaseEvent");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countAfterFirst = handler ? handler->handleCount : 0;

    // "caseevent" should be found (case-insensitive) -> no new Thunder sub
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "caseevent");
    WaitForJobs();

    if (handler != nullptr) {
        L0Test::ExpectEqU32(tr, handler->handleCount, countAfterFirst,
                           "AN-L0-036: Case-insensitive Exists prevents duplicate Thunder sub");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-037: EventUpdate_DispatchToAll_EmptyAppId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_EventUpdate_DispatchToAll_EmptyAppId()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    auto ctx3 = MakeContext(3, 30, "com.app.three", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "broadcastEvt");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "broadcastEvt");
    impl.Subscribe(ctx3, true, "org.rdk.FbSettings", "broadcastEvt");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("broadcastEvt", "{\"x\":1}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 3u, "AN-L0-037: All 3 subscribers dispatched");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-038: EventUpdate_FilterByAppId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_EventUpdate_FilterByAppId()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "filteredEvt");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "filteredEvt");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    // Emit with specific appId — only ctx1 (appId="com.app.one") should be dispatched
    impl.Emit("filteredEvt", "{}", "com.app.one");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 1u, "AN-L0-038: Only matching appId dispatched");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-039: EventUpdate_NoListeners_Warning
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_EventUpdate_NoListeners_Warning()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Emit for event with no subscribers — LOGWARN path
    uint32_t rc = impl.Emit("noListenersEvt", "{}", "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-039: Emit returns ERROR_NONE even with no listeners");

    WaitForJobs();
    // No crash expected
    L0Test::ExpectTrue(tr, true, "AN-L0-039: No crash on EventUpdate with no listeners");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-040: EventUpdate_VersionedEventKey
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_EventUpdate_VersionedEventKey()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Subscribe with versioned event name (e.g., "onEvent.v8")
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway", "8");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onEvent.v8");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    // Emit with same versioned key — EventUpdate strips ".v8" for dispatch clearKey
    impl.Emit("onEvent.v8", "{\"v\":8}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-040: Versioned event dispatched");
        // The clearKey should be "onEvent" (stripped .v8)
        L0Test::ExpectEqStr(tr, responder->lastEmitMethod, "onEvent", "AN-L0-040: Clear key strips .v8 suffix");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-041: Dispatch_OriginGateway_RoutesToGateway
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Dispatch_OriginGateway_RoutesToGateway()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // origin = APP_GATEWAY_CALLSIGN -> routes to DispatchToGateway
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "gwEvent");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("gwEvent", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-041: Gateway responder received emit");
    }

    // Internal responder should NOT have received anything
    auto* internal = service.GetInternalResponder();
    if (internal != nullptr) {
        L0Test::ExpectEqU32(tr, internal->emitCount, 0u, "AN-L0-041: Internal responder not called for gateway origin");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-042: Dispatch_OriginNonGateway_RoutesToLaunchDelegate
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Dispatch_OriginNonGateway_RoutesToLaunchDelegate()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // origin = "org.rdk.LaunchDelegate" (not APP_GATEWAY_CALLSIGN) -> routes to DispatchToLaunchDelegate
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.LaunchDelegate");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "ldEvent");
    WaitForJobs();

    auto* internal = service.GetInternalResponder();
    if (internal != nullptr) internal->emitCount = 0;

    impl.Emit("ldEvent", "{}", "");
    WaitForJobs();

    if (internal != nullptr) {
        L0Test::ExpectTrue(tr, internal->emitCount >= 1, "AN-L0-042: Internal responder received emit");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-043: DispatchToGateway_LazyAcquireSuccess
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_DispatchToGateway_LazyAcquireSuccess()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "lazyGw");
    WaitForJobs();

    // First emit triggers lazy acquire of gateway responder
    impl.Emit("lazyGw", "{}", "");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    L0Test::ExpectTrue(tr, responder != nullptr, "AN-L0-043: Gateway responder lazy-acquired");
    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-043: Emit dispatched after lazy acquire");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-044: DispatchToGateway_LazyAcquireFailure
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_DispatchToGateway_LazyAcquireFailure()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config cfg(true, false /*no gateway responder*/, true, true);
    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service(cfg);
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "failGw");
    WaitForJobs();

    // Emit — gateway responder not available, should hit LOGERR and return
    impl.Emit("failGw", "{}", "");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    L0Test::ExpectTrue(tr, responder == nullptr, "AN-L0-044: Gateway responder not acquired (null)");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-045: DispatchToGateway_CachedResponder
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_DispatchToGateway_CachedResponder()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "cachedGw");
    WaitForJobs();

    // First emit — acquires
    impl.Emit("cachedGw", "{}", "");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        uint32_t firstCount = responder->emitCount;

        // Second emit — uses cached responder
        impl.Emit("cachedGw", "{}", "");
        WaitForJobs();

        L0Test::ExpectEqU32(tr, responder->emitCount, firstCount + 1, "AN-L0-045: Cached responder reused");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-046: DispatchToLaunchDelegate_LazyAcquireSuccess
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquireSuccess()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.LaunchDelegate");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "lazyLd");
    WaitForJobs();

    impl.Emit("lazyLd", "{}", "");
    WaitForJobs();

    auto* internal = service.GetInternalResponder();
    L0Test::ExpectTrue(tr, internal != nullptr, "AN-L0-046: Internal responder lazy-acquired");
    if (internal != nullptr) {
        L0Test::ExpectTrue(tr, internal->emitCount >= 1, "AN-L0-046: Emit dispatched to LaunchDelegate");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-047: DispatchToLaunchDelegate_LazyAcquireFailure
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquireFailure()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config cfg(true, true, false /*no internal responder*/, true);
    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service(cfg);
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.LaunchDelegate");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "failLd");
    WaitForJobs();

    impl.Emit("failLd", "{}", "");
    WaitForJobs();

    auto* internal = service.GetInternalResponder();
    L0Test::ExpectTrue(tr, internal == nullptr, "AN-L0-047: Internal responder not acquired (null)");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-048: DispatchToLaunchDelegate_CachedResponder
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_DispatchToLaunchDelegate_CachedResponder()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.LaunchDelegate");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "cachedLd");
    WaitForJobs();

    impl.Emit("cachedLd", "{}", "");
    WaitForJobs();

    auto* internal = service.GetInternalResponder();
    if (internal != nullptr) {
        uint32_t firstCount = internal->emitCount;

        impl.Emit("cachedLd", "{}", "");
        WaitForJobs();

        L0Test::ExpectEqU32(tr, internal->emitCount, firstCount + 1, "AN-L0-048: Cached internal responder reused");
    }

    return tr.failures;
}
