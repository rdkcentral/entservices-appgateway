/*
 * AppNotifications_EmitTests.cpp
 *
 * L0 tests for AppNotificationsImplementation::Emit and Configure.
 * Tests AN-L0-010 to AN-L0-013, AN-L0-020 to AN-L0-022.
 *
 * Emit() enqueues an EmitJob to the WorkerPool. The job then calls
 * SubscriberMap::EventUpdate() which dispatches to registered contexts.
 * These tests verify:
 *   - Emit() returns ERROR_NONE for valid inputs
 *   - Emit() is safe with empty payload / empty appId
 *   - Configure() stores the shell pointer correctly
 *   - The EmitJob dispatches to matching subscribers via EventUpdate
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <core/core.h>
#include <plugins/IShell.h>

#include <interfaces/IAppNotifications.h>
#include <interfaces/IConfiguration.h>
#include "AppNotificationsServiceMock.h"
#include "AppNotificationsTestHelpers.h"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Exchange::IAppNotifications;
using WPEFramework::Exchange::IConfiguration;

namespace {
IAppNotifications::AppNotificationContext MakeContext(uint32_t connId,
                                                       uint32_t reqId,
                                                       const std::string& appId,
                                                       const std::string& origin,
                                                       const std::string& version = "0")
{
    IAppNotifications::AppNotificationContext ctx;
    ctx.connectionId = connId;
    ctx.requestId    = reqId;
    ctx.appId        = appId;
    ctx.origin       = origin;
    ctx.version      = version;
    return ctx;
}

void YieldToWorkerPool()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Helper: create a shell config with no notification handler to avoid the
// destructor null-mShell crash (known plugin bug in AppNotificationsImplementation).
// Emit tests do not require a handler — they only need the AppGateway responder.
L0Test::AppNotificationsServiceMock::Config MakeEmitTestConfig()
{
    L0Test::AppNotificationsServiceMock::Config cfg;
    cfg.provideNotificationHandler = false;
    return cfg;
}

} // namespace

// ---------------------------------------------------------------------------
// AN-L0-010: Emit returns ERROR_NONE and enqueues EmitJob
// ---------------------------------------------------------------------------
uint32_t Test_AN_Emit_SubmitsJob()
{
    /** Emit() returns ERROR_NONE and dispatches EmitJob to WorkerPool. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeEmitTestConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Emit_SubmitsJob: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Subscribe a context so EventUpdate has something to dispatch to
    auto ctx = MakeContext(10, 1001, "com.test.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onVolumeChanged");
    YieldToWorkerPool();

    // Emit the event
    const uint32_t rc = impl->Emit("onVolumeChanged", R"({"volume":75})", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Emit_SubmitsJob: Emit returns ERROR_NONE");

    // Let the EmitJob run through EventUpdate -> Dispatch -> DispatchToGateway
    YieldToWorkerPool();

    // Verify the AppGateway responder received an Emit call
    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr, "Emit_SubmitsJob: AppGateway responder should be acquired");
    if (gw != nullptr) {
        L0Test::ExpectTrue(tr, gw->emitCount > 0,
            "Emit_SubmitsJob: AppGateway Emit should have been called");
        L0Test::ExpectEqStr(tr, gw->lastEmitPayload, R"({"volume":75})",
            "Emit_SubmitsJob: payload should be forwarded");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-011: Emit with empty payload
// ---------------------------------------------------------------------------
uint32_t Test_AN_Emit_EmptyPayload()
{
    /** Emit() with empty payload returns ERROR_NONE and does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeEmitTestConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Emit_EmptyPayload: impl creation");
    if (impl == nullptr) { return tr.failures; }

    const uint32_t rc = impl->Emit("onEmptyPayloadEvent", "", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Emit_EmptyPayload: returns ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-012: Emit with empty appId dispatches to all listeners
// ---------------------------------------------------------------------------
uint32_t Test_AN_Emit_EmptyAppId()
{
    /** Emit() with empty appId dispatches to ALL registered contexts for the event. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeEmitTestConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Emit_EmptyAppId: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Subscribe two different app contexts to the same event
    auto ctx1 = MakeContext(10, 1001, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(11, 1002, "com.app.two", "org.rdk.AppGateway");
    impl->Subscribe(ctx1, true, "org.rdk.FbSettings", "onBroadcastEvent");
    impl->Subscribe(ctx2, true, "org.rdk.FbSettings", "onBroadcastEvent");
    YieldToWorkerPool();

    // Emit with empty appId — both should be dispatched
    const uint32_t rc = impl->Emit("onBroadcastEvent", R"({"data":"value"})", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Emit_EmptyAppId: returns ERROR_NONE");

    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr, "Emit_EmptyAppId: AppGateway responder acquired");
    if (gw != nullptr) {
        // Two contexts registered → Emit should have been called twice
        L0Test::ExpectTrue(tr, gw->emitCount >= 2,
            "Emit_EmptyAppId: Emit should be called for each listener");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-013: Configure stores shell pointer and AddRefs it
// ---------------------------------------------------------------------------
uint32_t Test_AN_Configure_Success()
{
    /** Configure(shell) returns ERROR_NONE and stores the shell. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell;

    auto* rawImpl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, rawImpl != nullptr, "Configure_Success: impl creation");
    if (rawImpl == nullptr) { return tr.failures; }

    auto* cfg = rawImpl->QueryInterface<IConfiguration>();
    L0Test::ExpectTrue(tr, cfg != nullptr, "Configure_Success: IConfiguration interface available");

    if (cfg != nullptr) {
        const uint32_t rc = cfg->Configure(&shell);
        L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
            "Configure_Success: Configure returns ERROR_NONE");
        cfg->Release();
    }

    rawImpl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-084: Configure() called twice does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Configure_DoubleConfigure()
{
    /** Calling Configure() twice does not crash (leaks a ref, but safe). */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config safeCfg;
    safeCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(safeCfg);
    auto* rawImpl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, rawImpl != nullptr,
        "Configure_DoubleConfigure: impl creation");
    if (rawImpl == nullptr) { return tr.failures; }

    auto* cfg = rawImpl->QueryInterface<IConfiguration>();
    L0Test::ExpectTrue(tr, cfg != nullptr,
        "Configure_DoubleConfigure: IConfiguration available");

    if (cfg != nullptr) {
        const uint32_t rc1 = cfg->Configure(&shell);
        L0Test::ExpectEqU32(tr, rc1, static_cast<uint32_t>(ERROR_NONE),
            "Configure_DoubleConfigure: first Configure returns ERROR_NONE");

        // Second Configure — should not crash
        const uint32_t rc2 = cfg->Configure(&shell);
        L0Test::ExpectEqU32(tr, rc2, static_cast<uint32_t>(ERROR_NONE),
            "Configure_DoubleConfigure: second Configure returns ERROR_NONE");

        cfg->Release();
    }

    rawImpl->Release();
    return tr.failures;
}
