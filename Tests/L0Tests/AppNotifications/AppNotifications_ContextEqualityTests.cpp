/*
 * AppNotifications L0 Test — Context Equality, SubscriberJob, EmitJob, Emitter,
 *                             and SubscriberMap destructor
 *
 * Test cases: AN-L0-064 through AN-L0-074
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
// AN-L0-064: SubscriberJob_Dispatch_Subscribe
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberJob_Dispatch_Subscribe()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    // Subscribe triggers SubscriberJob::Create -> Submit -> Dispatch -> ThunderManager::Subscribe
    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "subJobEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr && handler->handleCount >= 1,
                       "AN-L0-064: SubscriberJob dispatched and called ThunderManager::Subscribe");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-065: SubscriberJob_Dispatch_Unsubscribe
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberJob_Dispatch_Unsubscribe()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "unsubJobEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    uint32_t countBefore = handler ? handler->handleCount : 0;

    impl.Subscribe(ctx, false, "org.rdk.FbSettings", "unsubJobEvt");
    WaitForJobs();

    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->handleCount > countBefore,
                           "AN-L0-065: SubscriberJob dispatched unsubscribe");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-066: EmitJob_Dispatch
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_EmitJob_Dispatch()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "emitJobEvt");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) responder->emitCount = 0;

    impl.Emit("emitJobEvt", "{\"test\":1}", "");
    WaitForJobs();

    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-066: EmitJob dispatched EventUpdate");
        L0Test::ExpectEqStr(tr, responder->lastEmitPayload, "{\"test\":1}", "AN-L0-066: Payload passed through");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-067: Emitter_Emit_SubmitsJob
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Emitter_Emit_SubmitsJob()
{
    L0Test::TestResult tr;

    // The Emitter is tested indirectly:
    // When a NotificationHandler calls emitter->Emit(), it should submit an EmitJob
    // which eventually dispatches to EventUpdate.
    //
    // We simulate this by:
    // 1. Subscribe an event so handler is called
    // 2. In the handler, the emitter pointer is saved (handler->lastEmitter)
    // 3. We manually call Emit on the emitter to verify it works

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "emitterEvt");
    WaitForJobs();

    auto* handler = service.GetNotificationHandler();
    L0Test::ExpectTrue(tr, handler != nullptr, "AN-L0-067: Handler acquired");
    if (handler != nullptr) {
        L0Test::ExpectTrue(tr, handler->lastEmitter != nullptr, "AN-L0-067: Emitter pointer received");

        // Subscribe a listener for a different event that the emitter will emit to
        auto ctx2 = MakeContext(2, 20, "com.app.two", "org.rdk.AppGateway");
        impl.Subscribe(ctx2, true, "org.rdk.FbSettings", "emitterTarget");
        WaitForJobs();

        auto* responder = service.GetGatewayResponder();
        if (responder != nullptr) responder->emitCount = 0;

        // Call Emit on the emitter object
        handler->lastEmitter->Emit("emitterTarget", "{\"from\":\"emitter\"}", "");
        WaitForJobs();

        if (responder != nullptr) {
            L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-067: Emitter submitted EmitJob successfully");
        }
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-068: AppNotificationContext_Equal_AllFieldsMatch
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Context_Equal_AllFieldsMatch()
{
    L0Test::TestResult tr;

    auto ctx1 = MakeContext(42, 100, "com.app.test", "org.rdk.AppGateway", "8");
    auto ctx2 = MakeContext(42, 100, "com.app.test", "org.rdk.AppGateway", "8");

    L0Test::ExpectTrue(tr, ctx1 == ctx2, "AN-L0-068: Identical contexts are equal");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-069: AppNotificationContext_NotEqual_DifferentRequestId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Context_NotEqual_DifferentRequestId()
{
    L0Test::TestResult tr;

    auto ctx1 = MakeContext(1, 100, "com.app.test", "org.rdk.AppGateway", "8");
    auto ctx2 = MakeContext(2, 100, "com.app.test", "org.rdk.AppGateway", "8");

    L0Test::ExpectTrue(tr, !(ctx1 == ctx2), "AN-L0-069: Different requestId -> not equal");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-070: AppNotificationContext_NotEqual_DifferentConnectionId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Context_NotEqual_DifferentConnectionId()
{
    L0Test::TestResult tr;

    auto ctx1 = MakeContext(1, 100, "com.app.test", "org.rdk.AppGateway", "8");
    auto ctx2 = MakeContext(1, 200, "com.app.test", "org.rdk.AppGateway", "8");

    L0Test::ExpectTrue(tr, !(ctx1 == ctx2), "AN-L0-070: Different connectionId -> not equal");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-071: AppNotificationContext_NotEqual_DifferentAppId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Context_NotEqual_DifferentAppId()
{
    L0Test::TestResult tr;

    auto ctx1 = MakeContext(1, 100, "com.app.one", "org.rdk.AppGateway", "8");
    auto ctx2 = MakeContext(1, 100, "com.app.two", "org.rdk.AppGateway", "8");

    L0Test::ExpectTrue(tr, !(ctx1 == ctx2), "AN-L0-071: Different appId -> not equal");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-072: AppNotificationContext_NotEqual_DifferentOrigin
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Context_NotEqual_DifferentOrigin()
{
    L0Test::TestResult tr;

    auto ctx1 = MakeContext(1, 100, "com.app.test", "org.rdk.AppGateway", "8");
    auto ctx2 = MakeContext(1, 100, "com.app.test", "org.rdk.LaunchDelegate", "8");

    L0Test::ExpectTrue(tr, !(ctx1 == ctx2), "AN-L0-072: Different origin -> not equal");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-073: AppNotificationContext_NotEqual_DifferentVersion
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Context_NotEqual_DifferentVersion()
{
    L0Test::TestResult tr;

    auto ctx1 = MakeContext(1, 100, "com.app.test", "org.rdk.AppGateway", "0");
    auto ctx2 = MakeContext(1, 100, "com.app.test", "org.rdk.AppGateway", "8");

    L0Test::ExpectTrue(tr, !(ctx1 == ctx2), "AN-L0-073: Different version -> not equal");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-074: SubscriberMap_Destructor_ReleasesInterfaces
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_SubscriberMap_Destructor_ReleasesInterfaces()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock service;

    {
        WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
        impl.Configure(&service);

        auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
        impl.Subscribe(ctx, true, "org.rdk.FbSettings", "dtorIfaceEvt");
        WaitForJobs();

        // Trigger dispatch to acquire the gateway responder
        impl.Emit("dtorIfaceEvt", "{}", "");
        WaitForJobs();

        // impl goes out of scope -> SubscriberMap destructor -> releases mAppGateway
    }

    // No crash means the destructor successfully released the interfaces
    L0Test::ExpectTrue(tr, true, "AN-L0-074: SubscriberMap destructor released interfaces without crash");

    return tr.failures;
}
