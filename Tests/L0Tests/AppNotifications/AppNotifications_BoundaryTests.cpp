/*
 * AppNotifications L0 Test — Boundary Tests
 *
 * Test cases: AN-L0-075 through AN-L0-083
 *
 * These tests exercise boundary conditions: empty event names,
 * empty module names, large payloads, zero/max uint32_t values
 * for connectionId/requestId, and interface map queries.
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <climits>

#include <core/core.h>

#include <AppNotifications.h>
#include <AppNotificationsImplementation.h>
#include "AppNotificationsServiceMock.h"

#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Exchange::IAppNotifications;
using WPEFramework::Exchange::IConfiguration;
using WPEFramework::Plugin::AppNotifications;
using WPEFramework::Plugin::AppNotificationsImplementation;
using WPEFramework::PluginHost::IPlugin;
using WPEFramework::PluginHost::IDispatcher;

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
// AN-L0-075: Boundary_EmptyEventName
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Boundary_EmptyEventName()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-075: Subscribe with empty event returns ERROR_NONE");

    rc = impl.Emit("", "{}", "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-075: Emit with empty event returns ERROR_NONE");
    WaitForJobs();

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-076: Boundary_EmptyModuleName
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Boundary_EmptyModuleName()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "", "emptyModEvt");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-076: Subscribe with empty module returns ERROR_NONE");
    WaitForJobs();

    // The SubscriberJob will try to subscribe with empty module.
    // HandleNotifier will try QueryInterfaceByCallsign("", ...) which returns nullptr.
    // This should not crash.
    L0Test::ExpectTrue(tr, true, "AN-L0-076: No crash with empty module");

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-077: Boundary_LargePayload
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Boundary_LargePayload()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(1, 10, "com.app.one", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.FbSettings", "largeEvt");
    WaitForJobs();

    // Create a large payload (~100KB)
    std::string largePayload(100000, 'X');
    uint32_t rc = impl.Emit("largeEvt", largePayload, "");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-077: Emit with large payload returns ERROR_NONE");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-077: Large payload dispatched");
        L0Test::ExpectEqU32(tr, static_cast<uint32_t>(responder->lastEmitPayload.size()), 100000u,
                           "AN-L0-077: Full payload passed through");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-078: Boundary_ZeroConnectionId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Boundary_ZeroConnectionId()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(0, 0, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "zeroConnEvt");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-078: Subscribe with zero connectionId returns ERROR_NONE");
    WaitForJobs();

    impl.Emit("zeroConnEvt", "{}", "");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-078: Event dispatched with zero connectionId");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-079: Boundary_MaxUint32ConnectionId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Boundary_MaxUint32ConnectionId()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(UINT32_MAX, UINT32_MAX, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "maxConnEvt");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-079: Subscribe with UINT32_MAX returns ERROR_NONE");
    WaitForJobs();

    impl.Emit("maxConnEvt", "{}", "");
    WaitForJobs();

    auto* responder = service.GetGatewayResponder();
    if (responder != nullptr) {
        L0Test::ExpectTrue(tr, responder->emitCount >= 1, "AN-L0-079: Event dispatched with UINT32_MAX connectionId");
    }

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-080: Boundary_ZeroRequestId
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_Boundary_ZeroRequestId()
{
    L0Test::TestResult tr;

    WPEFramework::Core::Sink<AppNotificationsImplementation> impl;
    L0Test::AppNotificationsServiceMock service;
    impl.Configure(&service);

    auto ctx = MakeContext(0, 10, "com.app.one", "org.rdk.AppGateway");
    uint32_t rc = impl.Subscribe(ctx, true, "org.rdk.FbSettings", "zeroReqEvt");
    L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "AN-L0-080: Subscribe with zero requestId returns ERROR_NONE");
    WaitForJobs();

    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-081: InterfaceMap_IAppNotifications
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_InterfaceMap_IAppNotifications()
{
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock service;
    auto* plugin = WPEFramework::Core::Service<AppNotifications>::Create<IPlugin>();

    const std::string result = plugin->Initialize(&service);
    L0Test::ExpectEqStr(tr, result, "", "AN-L0-081: Initialize succeeds");

    auto* notif = static_cast<IAppNotifications*>(plugin->QueryInterface(IAppNotifications::ID));
    L0Test::ExpectTrue(tr, notif != nullptr, "AN-L0-081: IAppNotifications available via QueryInterface");
    if (notif != nullptr) {
        notif->Release();
    }

    plugin->Deinitialize(&service);
    plugin->Release();
    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-082: InterfaceMap_IPlugin
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_InterfaceMap_IPlugin()
{
    L0Test::TestResult tr;

    auto* plugin = WPEFramework::Core::Service<AppNotifications>::Create<IPlugin>();
    L0Test::ExpectTrue(tr, plugin != nullptr, "AN-L0-082: IPlugin interface available");

    auto* pluginQuery = static_cast<IPlugin*>(plugin->QueryInterface(IPlugin::ID));
    L0Test::ExpectTrue(tr, pluginQuery != nullptr, "AN-L0-082: QueryInterface for IPlugin returns non-null");
    if (pluginQuery != nullptr) {
        pluginQuery->Release();
    }

    plugin->Release();
    return tr.failures;
}

// ─────────────────────────────────────────────────────────────────────
// AN-L0-083: InterfaceMap_IDispatcher
// ─────────────────────────────────────────────────────────────────────
uint32_t Test_AN_InterfaceMap_IDispatcher()
{
    L0Test::TestResult tr;

    auto* plugin = WPEFramework::Core::Service<AppNotifications>::Create<IPlugin>();
    L0Test::ExpectTrue(tr, plugin != nullptr, "AN-L0-083: Plugin created");

    auto* dispatcher = static_cast<IDispatcher*>(plugin->QueryInterface(IDispatcher::ID));
    L0Test::ExpectTrue(tr, dispatcher != nullptr, "AN-L0-083: IDispatcher interface available (JSONRPC)");
    if (dispatcher != nullptr) {
        dispatcher->Release();
    }

    plugin->Release();
    return tr.failures;
}
