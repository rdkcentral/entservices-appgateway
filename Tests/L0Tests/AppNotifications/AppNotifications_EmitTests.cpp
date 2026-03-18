/*
 * AppNotifications L0 Test — Emit / Cleanup
 *
 * Test cases: AN-L0-020 through AN-L0-026
 *
 * These tests exercise the Emit and Cleanup paths of
 * AppNotificationsImplementation.
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
// AN-L0-020: Emit_SubmitsJob
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Emit_SubmitsJob()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Subscribe a listener first so EventUpdate has someone to dispatch to
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onEvent1");
    WaitForJobs();

    uint32_t rc = impl.Emit("onEvent1", "{\"key\":\"value\"}", "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-020: Emit returns ERROR_NONE");

    WaitForJobs();

    // The EmitJob dispatches to EventUpdate, which dispatches to the gateway responder
    auto* responder = service.GetGatewayResponder();
    L0Test::ExpectTrue(tr, responder != nullptr, "AN-L0-020: Gateway responder was queried");
    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-020: Responder Emit called at least once");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-021: Emit_EmptyPayload
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Emit_EmptyPayload()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onEvent2");
    WaitForJobs();

    uint32_t rc = impl.Emit("onEvent2", "", "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-021: Emit with empty payload returns ERROR_NONE");

    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-021: Emit with empty payload dispatched");
        L0Test::ExpectEqStr(tr, responder->lastEmitPayload, "", "AN-L0-021: Payload is empty");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-022: Emit_EmptyAppId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Emit_EmptyAppId()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Two subscribers with different appIds
    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "onBroadcast");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "onBroadcast");
    WaitForJobs();

    // Emit with empty appId should broadcast to ALL subscribers
    uint32_t rc = impl.Emit("onBroadcast", "{\"data\":1}", "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-022: Emit with empty appId returns ERROR_NONE");

    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        // Both subscribers should have been dispatched to
        L0Test::ExpectTrue(tr, responder->emitCount >= 2, "AN-L0-022: Emit dispatched to all subscribers (broadcast)");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-023: Cleanup_RemovesMatchingSubscribers
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Cleanup_RemovesMatchingSubscribers()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx1 = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.FbSettings", "onClean");
    impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "onClean");
    WaitForJobs();

    // Cleanup connectionId=10, origin="org.rdk.AppGateway" — removes ctx1 only
    uint32_t rc = impl.Cleanup(10, "org.rdk.AppGateway");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-023: Cleanup returns ERROR_NONE");

    // Emit to verify only ctx2 receives the event
    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        responder->emitCount = 0; // reset counter
    }

    impl.Emit("onClean", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 1u, "AN-L0-023: Only one subscriber remaining after cleanup");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-024: Cleanup_EmptiesEntireKey
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Cleanup_EmptiesEntireKey()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onRemoveAll");
    WaitForJobs();

    // Cleanup removes the only subscriber for "onremoveall"
    uint32_t rc = impl.Cleanup(10, "org.rdk.AppGateway");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-024: Cleanup returns ERROR_NONE");

    // Emit should now find no listeners (LOGWARN path)
    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        responder->emitCount = 0;
    }

    impl.Emit("onRemoveAll", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 0u, "AN-L0-024: No subscribers after cleanup, no dispatch");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-025: Cleanup_NoMatch_NoCrash
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Cleanup_NoMatch_NoCrash()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "onStay");
    WaitForJobs();

    // Cleanup with non-matching connectionId
    uint32_t rc = impl.Cleanup(999, "org.rdk.AppGateway");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-025: Cleanup with no match returns ERROR_NONE");

    // Subscriber should still be there
    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        responder->emitCount = 0;
    }

    impl.Emit("onStay", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 1u, "AN-L0-025: Subscriber still present after non-matching cleanup");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-026: Cleanup_MultipleEvents
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Cleanup_MultipleEvents()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "eventA");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "eventB");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "eventC");
    WaitForJobs();

    // Cleanup should remove ctx from ALL events
    uint32_t rc = impl.Cleanup(10, "org.rdk.AppGateway");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-026: Cleanup returns ERROR_NONE");

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        responder->emitCount = 0;
    }

    impl.Emit("eventA", "{}", "");
    impl.Emit("eventB", "{}", "");
    impl.Emit("eventC", "{}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectEqU32(tr, responder->emitCount, 0u, "AN-L0-026: All events cleaned up, no dispatches");
    }

    return tr.failures;
}
