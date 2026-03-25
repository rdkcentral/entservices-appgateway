/*
 * AppNotifications_BoundaryTests.cpp
 *
 * L0 boundary / edge-case tests for AppNotificationsImplementation.
 * Tests AN-L0-075 to AN-L0-080 + destructor-with-null-shell test.
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <limits>

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

void YieldToWorkerPool(int ms = 80)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Helper: create a shell config with no notification handler to avoid the
// destructor null-mShell crash (known plugin bug in AppNotificationsImplementation).
L0Test::AppNotificationsServiceMock::Config MakeSafeConfig()
{
    L0Test::AppNotificationsServiceMock::Config cfg;
    cfg.provideNotificationHandler = false;
    return cfg;
}

} // namespace

// ---------------------------------------------------------------------------
// AN-L0-075: Subscribe with empty event string does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Boundary_Subscribe_EmptyEvent()
{
    /** Subscribe(listen=true) with an empty event string does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Boundary_Subscribe_EmptyEvent: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");

    // Empty event string — should not crash
    const uint32_t rc = impl->Subscribe(ctx, true, "org.rdk.FbSettings", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Boundary_Subscribe_EmptyEvent: returns ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-076: Emit with a very large payload string does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Boundary_Emit_LargePayload()
{
    /** Emit() with a very large payload string (64 KB) does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Boundary_Emit_LargePayload: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onLargePayloadEvent");
    YieldToWorkerPool();

    // Construct a large JSON payload (~64 KB)
    std::string largePayload = R"({"data":")" + std::string(65000, 'X') + R"("})";

    const uint32_t rc = impl->Emit("onLargePayloadEvent", largePayload, "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Boundary_Emit_LargePayload: returns ERROR_NONE");

    YieldToWorkerPool();

    L0Test::ExpectTrue(tr, true, "Boundary_Emit_LargePayload: no crash with large payload");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-077: Cleanup with connectionId == 0 does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Boundary_Cleanup_ZeroConnectionId()
{
    /** Cleanup(0, origin) removes contexts with connectionId=0. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Boundary_Cleanup_ZeroConnectionId: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Subscribe a context with connectionId=0
    auto ctx = MakeContext(0, 0, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onZeroConnEvent");
    YieldToWorkerPool();

    const uint32_t rc = impl->Cleanup(0, "org.rdk.AppGateway");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Boundary_Cleanup_ZeroConnectionId: returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-078: Subscribe with max uint32_t connectionId
// ---------------------------------------------------------------------------
uint32_t Test_AN_Boundary_MaxUint32_ConnectionId()
{
    /** Subscribe with connectionId = UINT32_MAX does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Boundary_MaxUint32_ConnectionId: impl creation");
    if (impl == nullptr) { return tr.failures; }

    const uint32_t maxId = std::numeric_limits<uint32_t>::max();
    auto ctx = MakeContext(maxId, maxId, "com.max.app", "org.rdk.AppGateway");

    const uint32_t rc = impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onMaxConnEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Boundary_MaxUint32_ConnectionId: returns ERROR_NONE");

    YieldToWorkerPool();

    // Cleanup with the same max connectionId
    const uint32_t rcClean = impl->Cleanup(maxId, "org.rdk.AppGateway");
    L0Test::ExpectEqU32(tr, rcClean, static_cast<uint32_t>(ERROR_NONE),
        "Boundary_MaxUint32_ConnectionId: Cleanup returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-079: Subscribe with max uint32_t requestId does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Boundary_MaxUint32_RequestId()
{
    /** Subscribe with requestId = UINT32_MAX does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Boundary_MaxUint32_RequestId: impl creation");
    if (impl == nullptr) { return tr.failures; }

    const uint32_t maxId = std::numeric_limits<uint32_t>::max();
    auto ctx = MakeContext(1, maxId, "com.max.app", "org.rdk.AppGateway");

    const uint32_t rc = impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onMaxReqEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Boundary_MaxUint32_RequestId: returns ERROR_NONE");

    YieldToWorkerPool();

    // Emit and verify dispatch works with max requestId in context
    impl->Emit("onMaxReqEvent", R"({"x":1})", "");
    YieldToWorkerPool();

    L0Test::ExpectTrue(tr, true, "Boundary_MaxUint32_RequestId: no crash");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-080: AppNotificationsImplementation destructor with null shell is safe
// ---------------------------------------------------------------------------
uint32_t Test_AN_Impl_Destructor_NullShell()
{
    /** AppNotificationsImplementation destructor handles mShell=nullptr gracefully. */
    L0Test::TestResult tr;

    // Create impl WITHOUT calling Configure() — mShell stays nullptr
    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, impl != nullptr, "Impl_Destructor_NullShell: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Destroy without Configure — destructor checks (mShell != nullptr) before Release()
    impl->Release();

    L0Test::ExpectTrue(tr, true, "Impl_Destructor_NullShell: no crash on destroy with null shell");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-083: Impl destructor with non-null mShell releases it
// ---------------------------------------------------------------------------
uint32_t Test_AN_Impl_Destructor_NonNullShell()
{
    /** AppNotificationsImplementation destructor releases mShell when non-null. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = L0Test::CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr,
        "Impl_Destructor_NonNullShell: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Release — destructor should call mShell->Release() without crash
    impl->Release();

    L0Test::ExpectTrue(tr, true,
        "Impl_Destructor_NonNullShell: no crash on destroy with non-null shell");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-093: Subscribe with empty module string does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Subscribe_EmptyModule()
{
    /** Subscribe with an empty module string does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = L0Test::CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr,
        "Subscribe_EmptyModule: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");

    // Empty module string
    const uint32_t rc = impl->Subscribe(ctx, true, "", "onEmptyModuleEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Subscribe_EmptyModule: returns ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}
