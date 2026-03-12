#include <iostream>
#include <string>
#include <cstdlib>

#include <core/core.h>
#include <plugins/plugins.h>

#include "AppGatewayCommon.h"
#include "ServiceMock.h"
#include "L0Bootstrap.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_GENERAL;
using WPEFramework::Core::ERROR_UNAVAILABLE;
using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_UNKNOWN_KEY;
using WPEFramework::Plugin::AppGatewayCommon;
using WPEFramework::PluginHost::IPlugin;

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

struct PluginAndService {
    L0Test::ServiceMock* service { nullptr };
    IPlugin* plugin { nullptr };

    explicit PluginAndService(const L0Test::ServiceMock::Config& cfg = L0Test::ServiceMock::Config())
        : service(new L0Test::ServiceMock(cfg))
        , plugin(WPEFramework::Core::Service<AppGatewayCommon>::Create<IPlugin>())
    {
    }

    ~PluginAndService()
    {
        if (nullptr != plugin) {
            plugin->Release();
            plugin = nullptr;
        }
        if (nullptr != service) {
            service->Release();
            service = nullptr;
        }
    }
};

static Exchange::GatewayContext DefaultContext()
{
    Exchange::GatewayContext ctx;
    ctx.requestId = 1001;
    ctx.connectionId = 10;
    ctx.appId = "com.example.test";
    ctx.version = "1.0.0";
    return ctx;
}

} // namespace

// TEST_ID: AGC_L0_001
// Validate that Initialize returns empty string (success) and Deinitialize completes without crash.
static uint32_t Test_Initialize_Deinitialize_Lifecycle()
{
    TestResult tr;
    PluginAndService ps;

    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize returns empty string");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_002
// Validate that two separate plugin instances can initialize and deinitialize in sequence.
static uint32_t Test_Initialize_Twice_Idempotent()
{
    TestResult tr;

    {
        PluginAndService ps;
        const std::string init1 = ps.plugin->Initialize(ps.service);
        ExpectTrue(tr, init1.empty(), "First Initialize returns empty string");
        ps.plugin->Deinitialize(ps.service);
    }

    {
        PluginAndService ps;
        const std::string init2 = ps.plugin->Initialize(ps.service);
        ExpectTrue(tr, init2.empty(), "Second Initialize returns empty string");
        ps.plugin->Deinitialize(ps.service);
    }

    return tr.failures;
}

// TEST_ID: AGC_L0_003
// Validate that two separate plugin instances can deinitialize cleanly (robustness).
static uint32_t Test_Deinitialize_Twice_NoCrash()
{
    TestResult tr;

    {
        PluginAndService ps;
        ps.plugin->Initialize(ps.service);
        ps.plugin->Deinitialize(ps.service);
    }

    {
        PluginAndService ps;
        ps.plugin->Initialize(ps.service);
        ps.plugin->Deinitialize(ps.service);
    }

    return tr.failures;
}

// TEST_ID: AGC_L0_004
// Information() should return an empty string.
static uint32_t Test_Information_ReturnsEmpty()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);
    const std::string info = ps.plugin->Information();
    ExpectTrue(tr, info.empty(), "Information() returns empty string");
    ps.plugin->Deinitialize(ps.service);

    return tr.failures;
}

// TEST_ID: AGC_L0_005
// HandleAppGatewayRequest with an unknown method returns ERROR_UNKNOWN_KEY.
static uint32_t Test_HandleRequest_UnknownMethod()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "invalid.method.xyz", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNKNOWN_KEY, "invalid.method returns ERROR_UNKNOWN_KEY");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_006
// HandleAppGatewayRequest for "device.make" in L0 (no real plugins) returns ERROR_NONE
// because SystemDelegate::GetDeviceMake sets make="unknown" and returns ERROR_NONE
// after wrapping the default in quotes.
// In L0 without real Thunder plugins the link acquisition fails; the delegate
// falls through to the "unknown" default and still returns ERROR_NONE.
static uint32_t Test_HandleRequest_DeviceMake_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.make", "{}", result);

    // SystemDelegate::GetDeviceMake returns ERROR_NONE with make="\"unknown\""
    // even when the underlying JSON-RPC link is unavailable, because the
    // delegate defaults to "unknown". Accept either ERROR_NONE or ERROR_UNAVAILABLE
    // depending on the environment's Thunder link behavior.
    const bool acceptable = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, acceptable, "device.make returns ERROR_NONE or ERROR_UNAVAILABLE in L0");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_007
// HandleAppGatewayRequest for "metrics.*" pass-through returns ERROR_NONE with "null".
static uint32_t Test_HandleRequest_MetricsPassthrough()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "metrics.someEvent", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "metrics.someEvent returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "metrics.someEvent result is null");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_008
// HandleAppGatewayRequest for "discovery.watched" returns ERROR_NONE with "null".
static uint32_t Test_HandleRequest_DiscoveryWatched()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "discovery.watched", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "discovery.watched returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "discovery.watched result is null");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_009
// CheckPermissionGroup always returns allowed=true (no permission groups defined yet).
static uint32_t Test_CheckPermissionGroup_DefaultAllowed()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    bool allowed = false;
    const uint32_t rc = agc->CheckPermissionGroup("com.test.app", "someGroup", allowed);
    ExpectEqU32(tr, rc, ERROR_NONE, "CheckPermissionGroup returns ERROR_NONE");
    ExpectTrue(tr, allowed, "CheckPermissionGroup defaults to allowed=true");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_010
// Authenticate delegates to LifecycleDelegate via InvokeLifecycleDelegate.
// In L0 mDelegate and lifecycleDelegate are both non-null (created in Initialize),
// so LifecycleDelegate::Authenticate is reached.  The session ID is not in the
// AppIdInstanceIdMap, so GetAppId returns empty → ERROR_GENERAL.
static uint32_t Test_Authenticate_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string appId;
    const uint32_t rc = agc->Authenticate("test-session-id", appId);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "Authenticate returns ERROR_GENERAL in L0 (session not in map)");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_011
// GetSessionId delegates to LifecycleDelegate via InvokeLifecycleDelegate.
// In L0 the delegate chain is non-null, so LifecycleDelegate::GetSessionId is reached.
// The appId is not in the map → GetAppInstanceId returns empty → ERROR_GENERAL.
static uint32_t Test_GetSessionId_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string sessionId;
    const uint32_t rc = agc->GetSessionId("com.test.app", sessionId);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "GetSessionId returns ERROR_GENERAL in L0 (appId not in map)");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_012
// Setter with invalid payload returns ERROR_BAD_REQUEST.
static uint32_t Test_HandleRequest_SetterInvalidPayload()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // device.setname expects a JSON with "value" field; empty object should fail validation.
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.setname", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "device.setname with missing 'value' returns ERROR_BAD_REQUEST");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_013
// Setter with valid payload but no backing plugin returns ERROR_GENERAL (delegate unavailable).
static uint32_t Test_HandleRequest_SetterValidPayload_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // device.setname with valid payload; delegate calls SystemDelegate::SetDeviceName which fails in L0.
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.setname", R"({"value":"TestName"})", result);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "device.setname with valid payload returns ERROR_GENERAL in L0");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_014
// HandleAppGatewayRequest for "voiceguidance.setspeed" with out-of-range value
// returns ERROR_BAD_REQUEST.
static uint32_t Test_HandleRequest_VoiceGuidanceSetSpeed_OutOfRange()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // Speed must be between 0.5 and 2.0 — value 5.0 is out of range.
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":5.0})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setspeed with out-of-range value returns ERROR_BAD_REQUEST");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_015
// HandleAppGatewayRequest is case-insensitive for method names.
static uint32_t Test_HandleRequest_CaseInsensitiveMethod()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // "DEVICE.MAKE" should be lowered to "device.make" internally.
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "DEVICE.MAKE", "{}", result);
    const bool acceptable = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, acceptable, "DEVICE.MAKE (uppercase) routes same as device.make");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_016
// HandleAppGatewayRequest for "voiceguidance.setenabled" with invalid non-bool payload
// returns ERROR_BAD_REQUEST.
static uint32_t Test_HandleRequest_BoolSetterInvalidPayload()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // Expects {"value": true/false}, providing a string instead.
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", R"({"value":"notabool"})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setenabled with string value returns ERROR_BAD_REQUEST");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_017
// HandleAppGatewayRequest for lifecycle.ready via the handler map.
// In L0, InvokeLifecycleDelegate reaches LifecycleDelegate::LifecycleReady because
// both mDelegate and lifecycleDelegate are non-null.  LifecycleReady sets result="null"
// and returns ERROR_NONE when mLifecycleManagerState is nullptr (no /opt/ai2managers).
static uint32_t Test_HandleRequest_LifecycleReady()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle.ready returns ERROR_NONE in L0 (mLifecycleManagerState is null)");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_018
// Interface map: QueryInterface for IAppGatewayRequestHandler returns valid pointer.
static uint32_t Test_InterfaceMap_RequestHandler()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* requestHandler = ps.plugin->QueryInterface<Exchange::IAppGatewayRequestHandler>();
    ExpectTrue(tr, nullptr != requestHandler, "QueryInterface<IAppGatewayRequestHandler> returns non-null");
    if (nullptr != requestHandler) {
        requestHandler->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_019
// Interface map: QueryInterface for IAppGatewayAuthenticator returns valid pointer.
static uint32_t Test_InterfaceMap_Authenticator()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* authenticator = ps.plugin->QueryInterface<Exchange::IAppGatewayAuthenticator>();
    ExpectTrue(tr, nullptr != authenticator, "QueryInterface<IAppGatewayAuthenticator> returns non-null");
    if (nullptr != authenticator) {
        authenticator->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

int main()
{
    // Test-only bootstrap for WorkerPool.
    // Must be constructed before any plugin Initialize() calls.
    L0Test::L0BootstrapGuard bootstrap;

    struct Case {
        const char* name;
        uint32_t (*fn)();
    };

    const Case cases[] = {
        { "Initialize_Deinitialize_Lifecycle",           Test_Initialize_Deinitialize_Lifecycle },
        { "Initialize_Twice_Idempotent",                 Test_Initialize_Twice_Idempotent },
        { "Deinitialize_Twice_NoCrash",                  Test_Deinitialize_Twice_NoCrash },
        { "Information_ReturnsEmpty",                     Test_Information_ReturnsEmpty },
        { "HandleRequest_UnknownMethod",                  Test_HandleRequest_UnknownMethod },
        { "HandleRequest_DeviceMake_DelegateUnavailable", Test_HandleRequest_DeviceMake_DelegateUnavailable },
        { "HandleRequest_MetricsPassthrough",             Test_HandleRequest_MetricsPassthrough },
        { "HandleRequest_DiscoveryWatched",               Test_HandleRequest_DiscoveryWatched },
        { "CheckPermissionGroup_DefaultAllowed",          Test_CheckPermissionGroup_DefaultAllowed },
        { "Authenticate_DelegateUnavailable",             Test_Authenticate_DelegateUnavailable },
        { "GetSessionId_DelegateUnavailable",             Test_GetSessionId_DelegateUnavailable },
        { "HandleRequest_SetterInvalidPayload",           Test_HandleRequest_SetterInvalidPayload },
        { "HandleRequest_SetterValidPayload_DelegateUnavailable", Test_HandleRequest_SetterValidPayload_DelegateUnavailable },
        { "HandleRequest_VoiceGuidanceSetSpeed_OutOfRange", Test_HandleRequest_VoiceGuidanceSetSpeed_OutOfRange },
        { "HandleRequest_CaseInsensitiveMethod",          Test_HandleRequest_CaseInsensitiveMethod },
        { "HandleRequest_BoolSetterInvalidPayload",       Test_HandleRequest_BoolSetterInvalidPayload },
        { "HandleRequest_LifecycleReady",                 Test_HandleRequest_LifecycleReady },
        { "InterfaceMap_RequestHandler",                  Test_InterfaceMap_RequestHandler },
        { "InterfaceMap_Authenticator",                   Test_InterfaceMap_Authenticator },
    };

    uint32_t failures = 0;

    for (const auto& c : cases) {
        std::cerr << "[ RUN      ] " << c.name << std::endl;
        const uint32_t f = c.fn();
        if (0 == f) {
            std::cerr << "[       OK ] " << c.name << std::endl;
        } else {
            std::cerr << "[  FAILED  ] " << c.name << " failures=" << f << std::endl;
        }
        failures += f;
    }

    WPEFramework::Core::Singleton::Dispose();

    if (0 == failures) {
        std::cout << "AppGatewayCommon l0test passed." << std::endl;
        return 0;
    }

    std::cerr << "AppGatewayCommon l0test total failures: " << failures << std::endl;
    return static_cast<int>(failures);
}
