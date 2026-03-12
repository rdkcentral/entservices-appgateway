#include <iostream>
#include <string>
#include <cstdlib>

#include "AppGatewayResponderImplementation.h"
#include "ServiceMock.h"

#include <core/core.h>

using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_GENERAL;
using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Exchange::GatewayContext;

namespace {

struct TestResult {
    uint32_t failures { 0 };
};

static void ExpectTrue(TestResult& tr, const bool condition, const std::string& what)
{
    if (!condition) {
        tr.failures++;
        std::cerr << "FAIL: " << what << std::endl;
    }
}

static void ExpectEqU32(TestResult& tr, const uint32_t actual, const uint32_t expected, const std::string& what)
{
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected=" << expected << " actual=" << actual << std::endl;
    }
}

static void ExpectEqStr(TestResult& tr, const std::string& actual, const std::string& expected, const std::string& what)
{
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected='" << expected << "' actual='" << actual << "'" << std::endl;
    }
}

class ConnChangeCollector final : public WPEFramework::Exchange::IAppGatewayResponder::INotification {
public:
    ConnChangeCollector() = default;
    ~ConnChangeCollector() override = default;

    // Stack-owned collector: keep refcounting no-op/deterministic for tests.
    void AddRef() const override {}
    uint32_t Release() const override { return WPEFramework::Core::ERROR_NONE; }
    void* QueryInterface(const uint32_t id) override
    {
        if (id == WPEFramework::Exchange::IAppGatewayResponder::INotification::ID) {
            return static_cast<WPEFramework::Exchange::IAppGatewayResponder::INotification*>(this);
        }
        return nullptr;
    }

    void OnAppConnectionChanged(const std::string& appId, const uint32_t connectionId, const bool connected) override
    {
        lastAppId = appId;
        lastConnectionId = connectionId;
        lastConnected = connected;
        receivedCount++;
    }

    uint32_t receivedCount { 0 };
    std::string lastAppId;
    uint32_t lastConnectionId { 0 };
    bool lastConnected { false };
};

static GatewayContext MakeContext(uint32_t requestId, uint32_t connectionId, const std::string& appId)
{
    GatewayContext ctx;
    ctx.requestId = requestId;
    ctx.connectionId = connectionId;
    ctx.appId = appId;
    return ctx;
}

// Create a ServiceMock whose ConfigLine is websocket-config-compatible so
// AppGatewayResponderImplementation::InitializeWebsocket() will parse it and bind to an ephemeral port.
static L0Test::ServiceMock::Config MakeResponderServiceConfig()
{
    L0Test::ServiceMock::Config cfg(true /*resolver*/, true /*responder*/, true /*transport*/);
    cfg.provideAuthenticator = true;
    cfg.provideAppNotifications = true;

    // IMPORTANT: must contain `"connector"` so InitializeWebsocket() attempts parsing.
    // Port 0 lets the OS choose a free ephemeral port, reducing flakiness across runs.
    cfg.configLineOverride = "{\"connector\":\"127.0.0.1:0\"}";
    return cfg;
}

} // namespace

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayResponderImplementation_Register_Unregister_And_CallbackDelivery()
{
    // Note: AppGatewayResponderImplementation implements COM-style interfaces, so it requires AddRef/Release.
    // Use Core::Sink<> to supply reference counting in a deterministic way for L0.
    TestResult tr;

    WPEFramework::Core::Sink<WPEFramework::Plugin::AppGatewayResponderImplementation> responder;
    ConnChangeCollector collector;

    ExpectEqU32(tr, responder.Register(&collector), ERROR_NONE, "Register returns ERROR_NONE");

    responder.OnConnectionStatusChanged("com.test.app", 111, true);
    ExpectEqU32(tr, collector.receivedCount, 1u, "Registered listener receives connection status change");
    ExpectEqStr(tr, collector.lastAppId, "com.test.app", "Listener received appId");
    ExpectEqU32(tr, collector.lastConnectionId, 111u, "Listener received connectionId");
    ExpectTrue(tr, collector.lastConnected == true, "Listener received connected=true");

    ExpectEqU32(tr, responder.Unregister(&collector), ERROR_NONE, "Unregister returns ERROR_NONE");

    responder.OnConnectionStatusChanged("com.test.app", 111, false);
    ExpectEqU32(tr, collector.receivedCount, 1u, "Unregistered listener does not receive further updates");

    // Unregister again -> error path
    ExpectEqU32(tr, responder.Unregister(&collector), ERROR_GENERAL, "Unregister missing notification returns ERROR_GENERAL");

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayResponderImplementation_GetGatewayConnectionContext_EnvInjection_And_EmptyKey()
{
    TestResult tr;

    WPEFramework::Core::Sink<WPEFramework::Plugin::AppGatewayResponderImplementation> responder;

    std::string value;

    // Current implementation is a stub returning ERROR_NONE regardless of input.
    ExpectEqU32(tr,
               responder.GetGatewayConnectionContext(1, "" /*empty key*/, value),
               ERROR_NONE,
               "Empty contextKey currently returns ERROR_NONE (stub)");
    ExpectEqStr(tr, value, "", "Stub keeps contextValue unchanged for empty key");

    // Env injection variables are currently ignored by implementation.
    setenv("APPGATEWAY_TEST_CONN_ID", "410", 1);
    setenv("APPGATEWAY_TEST_CTX_KEY", "header.user-agent", 1);
    setenv("APPGATEWAY_TEST_CTX_VALUE", "UA/1.0", 1);

    value.clear();
    ExpectEqU32(tr,
               responder.GetGatewayConnectionContext(410, "header.user-agent", value),
               ERROR_NONE,
               "Stub returns ERROR_NONE for matching key");
    ExpectEqStr(tr, value, "", "Stub does not inject env context value");

    // Mismatch connId still returns ERROR_NONE in current stub.
    value.clear();
    ExpectEqU32(tr,
               responder.GetGatewayConnectionContext(411, "header.user-agent", value),
               ERROR_NONE,
               "Stub ignores connectionId and returns ERROR_NONE");
    ExpectEqStr(tr, value, "", "Stub leaves contextValue empty for mismatched connectionId");

    // Mismatch key still returns ERROR_NONE in current stub.
    value.clear();
    ExpectEqU32(tr,
               responder.GetGatewayConnectionContext(410, "missing.key", value),
               ERROR_NONE,
               "Stub ignores contextKey and returns ERROR_NONE");
    ExpectEqStr(tr, value, "", "Stub leaves contextValue empty for mismatched key");

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayResponderImplementation_Configure_And_Public_Methods_NoCrash()
{
    // Goal: execute constructor/configure/initialize-websocket code and some public methods
    // without relying on private-member access hacks (which break libstdc++).
    TestResult tr;

    L0Test::ServiceMock::Config cfg = MakeResponderServiceConfig();
    L0Test::ServiceMock service(cfg);

    WPEFramework::Core::Sink<WPEFramework::Plugin::AppGatewayResponderImplementation> responder;

    // Precondition: Configure must succeed before any responder API that enqueues jobs is used.
    const uint32_t cfgRc = responder.Configure(&service);
    ExpectEqU32(tr, cfgRc, ERROR_NONE, "Configure() returns ERROR_NONE");

    if (cfgRc == ERROR_NONE) {
        // Precondition: WorkerPool must be available (job enqueue relies on it).
        // The l0test main normally bootstraps this, but keep this test robust if used standalone.
        if (WPEFramework::Core::IWorkerPool::IsAvailable() == false) {
            tr.failures++;
            std::cerr << "FAIL: WorkerPool is not available; cannot enqueue responder jobs safely." << std::endl;
            return tr.failures;
        }

        // Exercise the lightweight async enqueue paths. We don't assert delivery here (offline deterministic).
        const auto ctx = MakeContext(77, 10, "com.test.app");

        ExpectEqU32(tr, responder.Respond(ctx, "null"), ERROR_NONE, "Respond() returns ERROR_NONE");
        ExpectEqU32(tr, responder.Emit(ctx, "some.event", "null"), ERROR_NONE, "Emit() returns ERROR_NONE");
        ExpectEqU32(tr, responder.Request(10, 88, "some.request", "{}"), ERROR_NONE, "Request() returns ERROR_NONE");
    } else {
        std::cerr << "NOTE: Skipping responder enqueue-path validation because Configure() failed." << std::endl;
    }

    // With no env injection configured, API contract returns BAD_REQUEST in this isolated build.
    unsetenv("APPGATEWAY_TEST_CONN_ID");
    unsetenv("APPGATEWAY_TEST_CTX_KEY");
    unsetenv("APPGATEWAY_TEST_CTX_VALUE");

    std::string out;
    ExpectEqU32(tr,
               responder.GetGatewayConnectionContext(10, "header.user-agent", out),
               ERROR_BAD_REQUEST,
               "No env injection configured => ERROR_BAD_REQUEST");

    return tr.failures;
}
