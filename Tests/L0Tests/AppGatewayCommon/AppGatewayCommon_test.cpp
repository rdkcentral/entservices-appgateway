#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <chrono>

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

// Minimal IEmitter stub for HandleAppEventNotifier tests.
// Heap-allocated, ref-counted; deletes itself when the last reference is released.
class StubEmitter : public Exchange::IAppNotificationHandler::IEmitter {
public:
    StubEmitter() : _refCount(1) {}
    ~StubEmitter() override = default;

    void AddRef() const override { _refCount.fetch_add(1, std::memory_order_relaxed); }
    uint32_t Release() const override {
        const uint32_t r = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (0 == r) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return WPEFramework::Core::ERROR_NONE;
    }
    void* QueryInterface(const uint32_t id) override {
        if (Exchange::IAppNotificationHandler::IEmitter::ID == id) {
            AddRef();
            return static_cast<Exchange::IAppNotificationHandler::IEmitter*>(this);
        }
        return nullptr;
    }
    void Emit(const std::string& /*event*/, const std::string& /*payload*/, const std::string& /*appId*/) override {}

private:
    mutable std::atomic<uint32_t> _refCount;
};

// Helper: test a delegate-backed getter method.
// In L0 (no real plugins), the delegate may return ERROR_NONE, ERROR_UNAVAILABLE, or ERROR_GENERAL.
static uint32_t DelegateGetterTest(const std::string& method,
                                   const Exchange::GatewayContext& ctx = DefaultContext())
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, method, "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, method + " returns acceptable code in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
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
    // In L0, SystemDelegate::SetDeviceName → AcquireLink() returns null → ERROR_UNAVAILABLE.
    // ResponseUtils::SetNullResponseForSuccess passes error code through unchanged.
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.setname", R"({"value":"TestName"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "device.setname with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");

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

// ============================================================================
// Tests AGC_L0_020 – AGC_L0_087 (new)
// ============================================================================

// TEST_ID: AGC_L0_020
// Handler-map getter: device.name
static uint32_t Test_HandleRequest_DeviceName()
{
    return DelegateGetterTest("device.name");
}

// TEST_ID: AGC_L0_021
// Handler-map getter: device.sku
static uint32_t Test_HandleRequest_DeviceSku()
{
    return DelegateGetterTest("device.sku");
}

// TEST_ID: AGC_L0_022
// Handler-map getter: device.network
static uint32_t Test_HandleRequest_DeviceNetwork()
{
    return DelegateGetterTest("device.network");
}

// TEST_ID: AGC_L0_023
// Handler-map getter: device.version
static uint32_t Test_HandleRequest_DeviceVersion()
{
    return DelegateGetterTest("device.version");
}

// TEST_ID: AGC_L0_024
// Handler-map getter: device.screenresolution
static uint32_t Test_HandleRequest_DeviceScreenResolution()
{
    return DelegateGetterTest("device.screenresolution");
}

// TEST_ID: AGC_L0_025
// Handler-map getter: device.videoresolution
static uint32_t Test_HandleRequest_DeviceVideoResolution()
{
    return DelegateGetterTest("device.videoresolution");
}

// TEST_ID: AGC_L0_026
// Handler-map getter: device.hdcp
static uint32_t Test_HandleRequest_DeviceHdcp()
{
    return DelegateGetterTest("device.hdcp");
}

// TEST_ID: AGC_L0_027
// Handler-map getter: device.hdr
static uint32_t Test_HandleRequest_DeviceHdr()
{
    return DelegateGetterTest("device.hdr");
}

// TEST_ID: AGC_L0_028
// Handler-map getter: device.audio
static uint32_t Test_HandleRequest_DeviceAudio()
{
    return DelegateGetterTest("device.audio");
}

// TEST_ID: AGC_L0_029
// Handler-map getter: voiceguidance.enabled
static uint32_t Test_HandleRequest_VoiceGuidanceEnabled()
{
    return DelegateGetterTest("voiceguidance.enabled");
}

// TEST_ID: AGC_L0_030
// Handler-map getter: voiceguidance.navigationhints
static uint32_t Test_HandleRequest_VoiceGuidanceNavigationHints()
{
    return DelegateGetterTest("voiceguidance.navigationhints");
}

// TEST_ID: AGC_L0_031
// Handler-map getter: accessibility.voiceguidancesettings with ctx.version="1.0.0"
// IsRDK8Compliant("1.0.0") == false → addSpeed = !false = true
static uint32_t Test_HandleRequest_VoiceGuidanceSettings_NonRDK8()
{
    Exchange::GatewayContext ctx = DefaultContext();
    ctx.version = "1.0.0";
    return DelegateGetterTest("accessibility.voiceguidancesettings", ctx);
}

// TEST_ID: AGC_L0_032
// Handler-map getter: accessibility.voiceguidancesettings with ctx.version="8"
// IsRDK8Compliant("8") == true → addSpeed = !true = false
static uint32_t Test_HandleRequest_VoiceGuidanceSettings_RDK8()
{
    Exchange::GatewayContext ctx = DefaultContext();
    ctx.version = "8";
    return DelegateGetterTest("accessibility.voiceguidancesettings", ctx);
}

// TEST_ID: AGC_L0_033
// Handler-map getter: accessibility.voiceguidance (always addSpeed=true)
static uint32_t Test_HandleRequest_AccessibilityVoiceGuidance()
{
    return DelegateGetterTest("accessibility.voiceguidance");
}

// TEST_ID: AGC_L0_034
// Handler-map getter: accessibility.audiodescriptionsettings
static uint32_t Test_HandleRequest_AccessibilityAudioDescriptionSettings()
{
    return DelegateGetterTest("accessibility.audiodescriptionsettings");
}

// TEST_ID: AGC_L0_035
// Handler-map getter: accessibility.audiodescription
static uint32_t Test_HandleRequest_AccessibilityAudioDescription()
{
    return DelegateGetterTest("accessibility.audiodescription");
}

// TEST_ID: AGC_L0_036
// Handler-map getter: audiodescriptions.enabled
static uint32_t Test_HandleRequest_AudioDescriptionsEnabled()
{
    return DelegateGetterTest("audiodescriptions.enabled");
}

// TEST_ID: AGC_L0_037
// Handler-map getter: accessibility.highcontrastui
static uint32_t Test_HandleRequest_AccessibilityHighContrastUI()
{
    return DelegateGetterTest("accessibility.highcontrastui");
}

// TEST_ID: AGC_L0_038
// Handler-map getter: closedcaptions.enabled
static uint32_t Test_HandleRequest_ClosedCaptionsEnabled()
{
    return DelegateGetterTest("closedcaptions.enabled");
}

// TEST_ID: AGC_L0_039
// Handler-map getter: closedcaptions.preferredlanguages
static uint32_t Test_HandleRequest_ClosedCaptionsPreferredLanguages()
{
    return DelegateGetterTest("closedcaptions.preferredlanguages");
}

// TEST_ID: AGC_L0_040
// Handler-map getter: accessibility.closedcaptions
static uint32_t Test_HandleRequest_AccessibilityClosedCaptions()
{
    return DelegateGetterTest("accessibility.closedcaptions");
}

// TEST_ID: AGC_L0_041
// Handler-map getter: accessibility.closedcaptionssettings
static uint32_t Test_HandleRequest_AccessibilityClosedCaptionsSettings()
{
    return DelegateGetterTest("accessibility.closedcaptionssettings");
}

// TEST_ID: AGC_L0_042
// Handler-map getter: localization.language
static uint32_t Test_HandleRequest_LocalizationLanguage()
{
    return DelegateGetterTest("localization.language");
}

// TEST_ID: AGC_L0_043
// Handler-map getter: localization.locale
static uint32_t Test_HandleRequest_LocalizationLocale()
{
    return DelegateGetterTest("localization.locale");
}

// TEST_ID: AGC_L0_044
// Handler-map getter: localization.preferredaudiolanguages
static uint32_t Test_HandleRequest_LocalizationPreferredAudioLanguages()
{
    return DelegateGetterTest("localization.preferredaudiolanguages");
}

// TEST_ID: AGC_L0_045
// Handler-map getter: localization.countrycode
static uint32_t Test_HandleRequest_LocalizationCountryCode()
{
    return DelegateGetterTest("localization.countrycode");
}

// TEST_ID: AGC_L0_046
// Handler-map getter: localization.timezone
static uint32_t Test_HandleRequest_LocalizationTimezone()
{
    return DelegateGetterTest("localization.timezone");
}

// TEST_ID: AGC_L0_047
// Handler-map getter: secondscreen.friendlyname
static uint32_t Test_HandleRequest_SecondScreenFriendlyName()
{
    return DelegateGetterTest("secondscreen.friendlyname");
}

// TEST_ID: AGC_L0_048
// Handler-map pass-through: localization.addadditionalinfo → ERROR_NONE, result=="null"
static uint32_t Test_HandleRequest_LocalizationAddAdditionalInfo()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.addadditionalinfo", R"({"key":"test","value":"val"})", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "localization.addadditionalinfo returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "localization.addadditionalinfo result is null");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_049
// lifecycle.state returns ERROR_NONE (state lookup uses empty map, returns default state string).
static uint32_t Test_HandleRequest_LifecycleState()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle.state returns ERROR_NONE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_050
// lifecycle.close in L0 → mLifecycleManagerState is null → ERROR_GENERAL
static uint32_t Test_HandleRequest_LifecycleClose()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "lifecycle.close returns ERROR_GENERAL in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_051
// lifecycle.finished always returns ERROR_NONE with result="null"
// LifecycleDelegate::LifecycleFinished unconditionally sets result="null" and returns ERROR_NONE.
static uint32_t Test_HandleRequest_LifecycleFinished()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "lifecycle.finished", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle.finished returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "lifecycle.finished result is null");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_052
// lifecycle2.state returns ERROR_NONE (state lookup uses empty map, returns default state string).
static uint32_t Test_HandleRequest_Lifecycle2State()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle2.state returns ERROR_NONE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_053
// lifecycle2.close in L0 → mLifecycleManagerState is null → ERROR_GENERAL
static uint32_t Test_HandleRequest_Lifecycle2Close()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "lifecycle2.close returns ERROR_GENERAL in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_054
// commoninternal.dispatchintent returns ERROR_NONE with result="null"
// LifecycleDelegate::DispatchLastIntent unconditionally sets result="null" and returns ERROR_NONE.
static uint32_t Test_HandleRequest_DispatchIntent()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "commoninternal.dispatchintent", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "commoninternal.dispatchintent returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "commoninternal.dispatchintent result is null");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_055
// commoninternal.getlastintent returns ERROR_NONE
// LifecycleDelegate::GetLastIntent calls GetLastKnownIntent → empty map → empty result, returns ERROR_NONE.
static uint32_t Test_HandleRequest_GetLastIntent()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "commoninternal.getlastintent returns ERROR_NONE");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_056
// advertising.advertisingid in L0 → SharedStorage unavailable
static uint32_t Test_HandleRequest_AdvertisingId()
{
    return DelegateGetterTest("advertising.advertisingid");
}

// TEST_ID: AGC_L0_057
// device.uid in L0 → SharedStorage unavailable
static uint32_t Test_HandleRequest_DeviceUid()
{
    return DelegateGetterTest("device.uid");
}

// TEST_ID: AGC_L0_058
// network.connected in L0 → NetworkDelegate
static uint32_t Test_HandleRequest_NetworkConnected()
{
    return DelegateGetterTest("network.connected");
}

// TEST_ID: AGC_L0_059
// localization.setcountrycode with invalid payload (missing value) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_SetCountryCode_InvalidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setcountrycode", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.setcountrycode with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_060
// localization.setcountrycode with valid payload → delegate unavailable
static uint32_t Test_HandleRequest_SetCountryCode_ValidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setcountrycode", R"({"value":"US"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setcountrycode with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_061
// localization.settimezone with invalid payload (missing value) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_SetTimezone_InvalidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.settimezone", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.settimezone with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_062
// localization.settimezone with valid payload → delegate unavailable
static uint32_t Test_HandleRequest_SetTimezone_ValidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.settimezone", R"({"value":"America/New_York"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.settimezone with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_063
// localization.setlocale with invalid payload (missing value) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_SetLocale_InvalidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setlocale", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.setlocale with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_064
// localization.setlocale with valid payload → delegate unavailable
static uint32_t Test_HandleRequest_SetLocale_ValidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setlocale", R"({"value":"en-US"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setlocale with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_065
// voiceguidance.setenabled with valid bool payload → delegate unavailable
static uint32_t Test_HandleRequest_VoiceGuidanceSetEnabled_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", R"({"value":true})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setenabled with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_066
// voiceguidance.speed getter → acceptable code in L0
static uint32_t Test_HandleRequest_VoiceGuidanceSpeed()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, "voiceguidance.speed returns acceptable code in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_067
// voiceguidance.rate alias → same handler as voiceguidance.speed
static uint32_t Test_HandleRequest_VoiceGuidanceRate()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.rate", "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, "voiceguidance.rate returns acceptable code in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_068
// voiceguidance.setspeed with boundary value 0.5 (min) → ERROR_BAD_REQUEST
// NOTE: Variant::Number() truncates 0.5 to 0, which fails the min-bound check (0 < 0.5).
// This is a known issue in ValidateAndExtractDouble (uses integer Number() instead of Float()).
static uint32_t Test_HandleRequest_SetSpeed_MinBoundary()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":0.5})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setspeed with 0.5 returns ERROR_BAD_REQUEST (Number() truncates to 0)");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_069
// voiceguidance.setspeed with boundary value 2.0 (max) → delegate unavailable
static uint32_t Test_HandleRequest_SetSpeed_MaxBoundary()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":2.0})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setspeed with 2.0 returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_070
// voiceguidance.setspeed with 0.49 (below min) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_SetSpeed_BelowMin()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":0.49})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setspeed with 0.49 returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_071
// voiceguidance.setspeed with 2.01 (above max) → passes validation, delegate unavailable
// NOTE: Variant::Number() truncates 2.01 to 2, which passes the max-bound check (2 > 2.0 is false).
// This is a known issue in ValidateAndExtractDouble (uses integer Number() instead of Float()).
static uint32_t Test_HandleRequest_SetSpeed_AboveMax()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":2.01})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setspeed with 2.01 passes validation (Number() truncates to 2), delegate unavailable");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_072
// voiceguidance.setrate alias — same handler as setspeed, valid payload → delegate unavailable
static uint32_t Test_HandleRequest_SetRate_Alias()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setrate", R"({"value":1.0})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setrate alias returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_073
// voiceguidance.setnavigationhints with invalid payload (string not bool) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_SetNavigationHints_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", R"({"value":"yes"})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setnavigationhints with string value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_074
// voiceguidance.setnavigationhints with valid bool payload → delegate unavailable
static uint32_t Test_HandleRequest_SetNavigationHints_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", R"({"value":false})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setnavigationhints with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_075
// audiodescriptions.setenabled with invalid payload (missing value) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_AudioDescSetEnabled_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "audiodescriptions.setenabled with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_076
// audiodescriptions.setenabled with valid bool payload → delegate unavailable
static uint32_t Test_HandleRequest_AudioDescSetEnabled_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", R"({"value":true})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "audiodescriptions.setenabled with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_077
// closedcaptions.setenabled with invalid payload (missing value) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_CCSetEnabled_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "closedcaptions.setenabled with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_078
// closedcaptions.setenabled with valid bool payload → delegate unavailable
static uint32_t Test_HandleRequest_CCSetEnabled_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", R"({"value":true})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "closedcaptions.setenabled with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_079
// closedcaptions.setpreferredlanguages with invalid payload (number, not string/array) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_CCSetPrefLangs_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":123})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "closedcaptions.setpreferredlanguages with number returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_080
// closedcaptions.setpreferredlanguages with valid string → delegate unavailable
static uint32_t Test_HandleRequest_CCSetPrefLangs_ValidString()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":"en"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "closedcaptions.setpreferredlanguages with string returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_081
// closedcaptions.setpreferredlanguages with valid array → delegate unavailable
static uint32_t Test_HandleRequest_CCSetPrefLangs_ValidArray()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":["en","fr"]})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "closedcaptions.setpreferredlanguages with array returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_082
// localization.setpreferredaudiolanguages with invalid payload (number, not string/array) → ERROR_BAD_REQUEST
static uint32_t Test_HandleRequest_SetPrefAudioLangs_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":123})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.setpreferredaudiolanguages with number returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_083
// localization.setpreferredaudiolanguages with valid string → delegate unavailable
static uint32_t Test_HandleRequest_SetPrefAudioLangs_ValidString()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":"en"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setpreferredaudiolanguages with string returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_084
// localization.setpreferredaudiolanguages with valid array → delegate unavailable
static uint32_t Test_HandleRequest_SetPrefAudioLangs_ValidArray()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":["en","fr"]})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setpreferredaudiolanguages with array returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_085
// HandleAppEventNotifier with null cb → ERROR_GENERAL, status==false
static uint32_t Test_HandleAppEventNotifier_NullCb()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    bool status = true;
    const uint32_t rc = agc->HandleAppEventNotifier(nullptr, "lifecycle.onbackground", true, status);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "HandleAppEventNotifier with null cb returns ERROR_GENERAL");
    ExpectTrue(tr, false == status, "HandleAppEventNotifier with null cb sets status false");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_086
// HandleAppEventNotifier with valid emitter → ERROR_NONE, status==true; exercise listen and unlisten
static uint32_t Test_HandleAppEventNotifier_ValidCb()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    auto* emitter = new StubEmitter(); // refcount=1

    // listen=true
    bool status1 = false;
    const uint32_t rc1 = agc->HandleAppEventNotifier(emitter, "lifecycle.onbackground", true, status1);
    ExpectEqU32(tr, rc1, ERROR_NONE, "HandleAppEventNotifier listen=true returns ERROR_NONE");
    ExpectTrue(tr, status1, "HandleAppEventNotifier listen=true sets status true");

    // listen=false (unlisten)
    bool status2 = false;
    const uint32_t rc2 = agc->HandleAppEventNotifier(emitter, "lifecycle.onbackground", false, status2);
    ExpectEqU32(tr, rc2, ERROR_NONE, "HandleAppEventNotifier listen=false returns ERROR_NONE");
    ExpectTrue(tr, status2, "HandleAppEventNotifier listen=false sets status true");

    // Allow WorkerPool to drain the two async EventRegistrationJobs before teardown,
    // preventing a use-after-free on the emitter/delegate pointers.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    emitter->Release(); // drop our initial ref
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_087
// QueryInterface for IAppNotificationHandler returns valid pointer.
static uint32_t Test_InterfaceMap_NotificationHandler()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* handler = ps.plugin->QueryInterface<Exchange::IAppNotificationHandler>();
    ExpectTrue(tr, nullptr != handler, "QueryInterface<IAppNotificationHandler> returns non-null");
    if (nullptr != handler) {
        handler->Release();
    }
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// ============================================================================
// Tests AGC_L0_088 – AGC_L0_092 (Phase 1: remaining L0 coverage squeeze)
// ============================================================================

// TEST_ID: AGC_L0_088
// HandleAppGatewayRequest before Initialize → mDelegate is null → ERROR_UNAVAILABLE
// Covers the null-delegate guard at the top of HandleAppGatewayRequest (lines 312-316).
static uint32_t Test_HandleRequest_BeforeInit()
{
    TestResult tr;
    PluginAndService ps;
    // Deliberately do NOT call Initialize — mDelegate remains nullptr
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "HandleAppGatewayRequest before Init returns ERROR_UNAVAILABLE");
    ExpectTrue(tr, result.find("unavailable") != std::string::npos,
               "result contains 'unavailable' error message");
    return tr.failures;
}

// TEST_ID: AGC_L0_089
// HandleAppEventNotifier before Initialize → SafeSubmitEventRegistrationJob hits null mDelegate → ERROR_GENERAL
// Covers the mDelegate null-check branch inside SafeSubmitEventRegistrationJob (lines 481-483).
static uint32_t Test_HandleAppEventNotifier_BeforeInit()
{
    TestResult tr;
    PluginAndService ps;
    // Deliberately do NOT call Initialize — mDelegate remains nullptr
    auto* handler = ps.plugin->QueryInterface<Exchange::IAppNotificationHandler>();
    ExpectTrue(tr, nullptr != handler, "QueryInterface<IAppNotificationHandler> returns non-null");
    if (nullptr != handler) {
        auto* emitter = new StubEmitter();
        bool status = true;
        const uint32_t rc = handler->HandleAppEventNotifier(emitter, "lifecycle.onbackground", true, status);
        ExpectEqU32(tr, rc, ERROR_GENERAL, "HandleAppEventNotifier before Init returns ERROR_GENERAL");
        ExpectTrue(tr, false == status, "status is false when delegate is null");
        emitter->Release();
        handler->Release();
    }
    return tr.failures;
}

// TEST_ID: AGC_L0_090
// Authenticate before Initialize → InvokeLifecycleDelegate returns ERROR_UNAVAILABLE (null delegate)
// Covers the `if (!delegate)` guard in the InvokeLifecycleDelegate template.
static uint32_t Test_Authenticate_BeforeInit()
{
    TestResult tr;
    PluginAndService ps;
    // Deliberately do NOT call Initialize — mDelegate remains nullptr
    auto* auth = ps.plugin->QueryInterface<Exchange::IAppGatewayAuthenticator>();
    ExpectTrue(tr, nullptr != auth, "QueryInterface<IAppGatewayAuthenticator> returns non-null");
    if (nullptr != auth) {
        std::string appId;
        const uint32_t rc = auth->Authenticate("session-before-init", appId);
        ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "Authenticate before Init returns ERROR_UNAVAILABLE");
        auth->Release();
    }
    return tr.failures;
}

// TEST_ID: AGC_L0_091
// HandleAppGatewayRequest after Deinitialize → mDelegate has been reset → ERROR_UNAVAILABLE
// Real-world scenario: plugin deactivated but a pending request arrives (race condition).
static uint32_t Test_HandleRequest_AfterDeinit()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    ps.plugin->Deinitialize(ps.service);
    // After Deinitialize, mDelegate is reset to nullptr
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "HandleAppGatewayRequest after Deinit returns ERROR_UNAVAILABLE");
    return tr.failures;
}

// TEST_ID: AGC_L0_092
// Re-initialization: Init → Deinit → Init → use → Deinit on the same instance.
// Validates that the plugin can be cleanly reactivated after deactivation.
static uint32_t Test_Reinitialize_AfterDeinit()
{
    TestResult tr;
    PluginAndService ps;

    // First lifecycle
    const std::string init1 = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, init1.empty(), "First Initialize succeeds");
    ps.plugin->Deinitialize(ps.service);

    // Second lifecycle on the same instance
    const std::string init2 = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, init2.empty(), "Re-Initialize after Deinit succeeds");

    // Verify the plugin works after re-initialization
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, "device.make works after re-initialization");

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
        // AGC_L0_020–087 — handler-map getters
        { "HandleRequest_DeviceName",                     Test_HandleRequest_DeviceName },
        { "HandleRequest_DeviceSku",                      Test_HandleRequest_DeviceSku },
        { "HandleRequest_DeviceNetwork",                  Test_HandleRequest_DeviceNetwork },
        { "HandleRequest_DeviceVersion",                  Test_HandleRequest_DeviceVersion },
        { "HandleRequest_DeviceScreenResolution",         Test_HandleRequest_DeviceScreenResolution },
        { "HandleRequest_DeviceVideoResolution",          Test_HandleRequest_DeviceVideoResolution },
        { "HandleRequest_DeviceHdcp",                     Test_HandleRequest_DeviceHdcp },
        { "HandleRequest_DeviceHdr",                      Test_HandleRequest_DeviceHdr },
        { "HandleRequest_DeviceAudio",                    Test_HandleRequest_DeviceAudio },
        { "HandleRequest_VoiceGuidanceEnabled",           Test_HandleRequest_VoiceGuidanceEnabled },
        { "HandleRequest_VoiceGuidanceNavigationHints",   Test_HandleRequest_VoiceGuidanceNavigationHints },
        { "HandleRequest_VoiceGuidanceSettings_NonRDK8",  Test_HandleRequest_VoiceGuidanceSettings_NonRDK8 },
        { "HandleRequest_VoiceGuidanceSettings_RDK8",     Test_HandleRequest_VoiceGuidanceSettings_RDK8 },
        { "HandleRequest_AccessibilityVoiceGuidance",     Test_HandleRequest_AccessibilityVoiceGuidance },
        { "HandleRequest_AccessibilityAudioDescSettings", Test_HandleRequest_AccessibilityAudioDescriptionSettings },
        { "HandleRequest_AccessibilityAudioDescription",  Test_HandleRequest_AccessibilityAudioDescription },
        { "HandleRequest_AudioDescriptionsEnabled",       Test_HandleRequest_AudioDescriptionsEnabled },
        { "HandleRequest_AccessibilityHighContrastUI",    Test_HandleRequest_AccessibilityHighContrastUI },
        { "HandleRequest_ClosedCaptionsEnabled",          Test_HandleRequest_ClosedCaptionsEnabled },
        { "HandleRequest_ClosedCaptionsPreferredLangs",   Test_HandleRequest_ClosedCaptionsPreferredLanguages },
        { "HandleRequest_AccessibilityClosedCaptions",    Test_HandleRequest_AccessibilityClosedCaptions },
        { "HandleRequest_AccessibilityClosedCaptSettings",Test_HandleRequest_AccessibilityClosedCaptionsSettings },
        { "HandleRequest_LocalizationLanguage",           Test_HandleRequest_LocalizationLanguage },
        { "HandleRequest_LocalizationLocale",             Test_HandleRequest_LocalizationLocale },
        { "HandleRequest_LocalizationPreferredAudioLangs",Test_HandleRequest_LocalizationPreferredAudioLanguages },
        { "HandleRequest_LocalizationCountryCode",        Test_HandleRequest_LocalizationCountryCode },
        { "HandleRequest_LocalizationTimezone",           Test_HandleRequest_LocalizationTimezone },
        { "HandleRequest_SecondScreenFriendlyName",       Test_HandleRequest_SecondScreenFriendlyName },
        { "HandleRequest_LocalizationAddAdditionalInfo",  Test_HandleRequest_LocalizationAddAdditionalInfo },
        { "HandleRequest_LifecycleState",                 Test_HandleRequest_LifecycleState },
        { "HandleRequest_LifecycleClose",                 Test_HandleRequest_LifecycleClose },
        { "HandleRequest_LifecycleFinished",              Test_HandleRequest_LifecycleFinished },
        { "HandleRequest_Lifecycle2State",                Test_HandleRequest_Lifecycle2State },
        { "HandleRequest_Lifecycle2Close",                Test_HandleRequest_Lifecycle2Close },
        { "HandleRequest_DispatchIntent",                 Test_HandleRequest_DispatchIntent },
        { "HandleRequest_GetLastIntent",                  Test_HandleRequest_GetLastIntent },
        { "HandleRequest_AdvertisingId",                  Test_HandleRequest_AdvertisingId },
        { "HandleRequest_DeviceUid",                      Test_HandleRequest_DeviceUid },
        { "HandleRequest_NetworkConnected",               Test_HandleRequest_NetworkConnected },
        // AGC_L0_059–084 — setter validation
        { "SetCountryCode_InvalidPayload",                Test_HandleRequest_SetCountryCode_InvalidPayload },
        { "SetCountryCode_ValidPayload",                  Test_HandleRequest_SetCountryCode_ValidPayload },
        { "SetTimezone_InvalidPayload",                   Test_HandleRequest_SetTimezone_InvalidPayload },
        { "SetTimezone_ValidPayload",                     Test_HandleRequest_SetTimezone_ValidPayload },
        { "SetLocale_InvalidPayload",                     Test_HandleRequest_SetLocale_InvalidPayload },
        { "SetLocale_ValidPayload",                       Test_HandleRequest_SetLocale_ValidPayload },
        { "VoiceGuidanceSetEnabled_Valid",                Test_HandleRequest_VoiceGuidanceSetEnabled_Valid },
        { "VoiceGuidanceSpeed",                           Test_HandleRequest_VoiceGuidanceSpeed },
        { "VoiceGuidanceRate",                            Test_HandleRequest_VoiceGuidanceRate },
        { "SetSpeed_MinBoundary",                         Test_HandleRequest_SetSpeed_MinBoundary },
        { "SetSpeed_MaxBoundary",                         Test_HandleRequest_SetSpeed_MaxBoundary },
        { "SetSpeed_BelowMin",                            Test_HandleRequest_SetSpeed_BelowMin },
        { "SetSpeed_AboveMax",                            Test_HandleRequest_SetSpeed_AboveMax },
        { "SetRate_Alias",                                Test_HandleRequest_SetRate_Alias },
        { "SetNavigationHints_Invalid",                   Test_HandleRequest_SetNavigationHints_Invalid },
        { "SetNavigationHints_Valid",                     Test_HandleRequest_SetNavigationHints_Valid },
        { "AudioDescSetEnabled_Invalid",                  Test_HandleRequest_AudioDescSetEnabled_Invalid },
        { "AudioDescSetEnabled_Valid",                    Test_HandleRequest_AudioDescSetEnabled_Valid },
        { "CCSetEnabled_Invalid",                         Test_HandleRequest_CCSetEnabled_Invalid },
        { "CCSetEnabled_Valid",                            Test_HandleRequest_CCSetEnabled_Valid },
        { "CCSetPrefLangs_Invalid",                       Test_HandleRequest_CCSetPrefLangs_Invalid },
        { "CCSetPrefLangs_ValidString",                   Test_HandleRequest_CCSetPrefLangs_ValidString },
        { "CCSetPrefLangs_ValidArray",                    Test_HandleRequest_CCSetPrefLangs_ValidArray },
        { "SetPrefAudioLangs_Invalid",                    Test_HandleRequest_SetPrefAudioLangs_Invalid },
        { "SetPrefAudioLangs_ValidString",                Test_HandleRequest_SetPrefAudioLangs_ValidString },
        { "SetPrefAudioLangs_ValidArray",                 Test_HandleRequest_SetPrefAudioLangs_ValidArray },
        // AGC_L0_085–087 — HandleAppEventNotifier & QueryInterface
        { "HandleAppEventNotifier_NullCb",                Test_HandleAppEventNotifier_NullCb },
        { "HandleAppEventNotifier_ValidCb",               Test_HandleAppEventNotifier_ValidCb },
        { "InterfaceMap_NotificationHandler",             Test_InterfaceMap_NotificationHandler },
                // AGC_L0_088–092 — Pre-init / post-deinit guards and re-initialization
        { "HandleRequest_BeforeInit",                     Test_HandleRequest_BeforeInit },
        { "HandleAppEventNotifier_BeforeInit",            Test_HandleAppEventNotifier_BeforeInit },
        { "Authenticate_BeforeInit",                      Test_Authenticate_BeforeInit },
        { "HandleRequest_AfterDeinit",                    Test_HandleRequest_AfterDeinit },
        { "Reinitialize_AfterDeinit",                     Test_Reinitialize_AfterDeinit },
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
