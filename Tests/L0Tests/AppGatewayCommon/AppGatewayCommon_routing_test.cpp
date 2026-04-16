#include "AppGatewayCommon_common_test.h"

// TEST_ID: AGC_L0_012
// HandleAppGatewayRequest with an unknown method returns ERROR_UNKNOWN_KEY.
uint32_t Test_HandleRequest_UnknownMethod()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "invalid.method.xyz", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNKNOWN_KEY, "invalid.method returns ERROR_UNKNOWN_KEY");

    return tr.failures;
}

// TEST_ID: AGC_L0_013
// HandleAppGatewayRequest for "device.make" in L0 (no real plugins) returns ERROR_NONE
// because SystemDelegate::GetDeviceMake sets make="unknown" and returns ERROR_NONE
// after wrapping the default in quotes.
// In L0 without real Thunder plugins the link acquisition fails; the delegate
// falls through to the "unknown" default and still returns ERROR_NONE.
uint32_t Test_HandleRequest_DeviceMake_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "device.make", "{}", result);

    // SystemDelegate::GetDeviceMake returns ERROR_NONE with make="\"unknown\""
    // even when the underlying JSON-RPC link is unavailable, because the
    // delegate defaults to "unknown". Accept either ERROR_NONE or ERROR_UNAVAILABLE
    // depending on the environment's Thunder link behavior.
    const bool acceptable = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, acceptable, "device.make returns ERROR_NONE or ERROR_UNAVAILABLE in L0");

    return tr.failures;
}

// TEST_ID: AGC_L0_014
// HandleAppGatewayRequest for "metrics.*" pass-through returns ERROR_NONE with "null".
uint32_t Test_HandleRequest_MetricsPassthrough()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "metrics.someEvent", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "metrics.someEvent returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "metrics.someEvent result is null");

    return tr.failures;
}

// TEST_ID: AGC_L0_015
// HandleAppGatewayRequest for "discovery.watched" returns ERROR_NONE with "null".
uint32_t Test_HandleRequest_DiscoveryWatched()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "discovery.watched", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "discovery.watched returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "discovery.watched result is null");

    return tr.failures;
}

// TEST_ID: AGC_L0_016
// HandleAppGatewayRequest is case-insensitive for method names.
uint32_t Test_HandleRequest_CaseInsensitiveMethod()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // "DEVICE.MAKE" should be lowered to "device.make" internally.
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "DEVICE.MAKE", "{}", result);
    const bool acceptable = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, acceptable, "DEVICE.MAKE (uppercase) routes same as device.make");

    return tr.failures;
}

// TEST_ID: AGC_L0_017
// HandleAppGatewayRequest for lifecycle.ready via the handler map.
// In L0, InvokeLifecycleDelegate reaches LifecycleDelegate::LifecycleReady because
// both mDelegate and lifecycleDelegate are non-null.  LifecycleReady sets result="null"
// and returns ERROR_NONE when mLifecycleManagerState is nullptr (no /opt/ai2managers).
uint32_t Test_HandleRequest_LifecycleReady()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle.ready returns ERROR_NONE in L0 (mLifecycleManagerState is null)");

    return tr.failures;
}

// ============================================================================
// Tests AGC_L0_018 – AGC_L0_056 — handler-map getters
// ============================================================================

// TEST_ID: AGC_L0_018
// Handler-map getter: device.name
uint32_t Test_HandleRequest_DeviceName()
{
    return DelegateGetterTest("device.name");
}

// TEST_ID: AGC_L0_019
// Handler-map getter: device.sku
uint32_t Test_HandleRequest_DeviceSku()
{
    return DelegateGetterTest("device.sku");
}

// TEST_ID: AGC_L0_020
// Handler-map getter: device.network
uint32_t Test_HandleRequest_DeviceNetwork()
{
    return DelegateGetterTest("device.network");
}

// TEST_ID: AGC_L0_021
// Handler-map getter: device.version
uint32_t Test_HandleRequest_DeviceVersion()
{
    return DelegateGetterTest("device.version");
}

// TEST_ID: AGC_L0_022
// Handler-map getter: device.screenresolution
uint32_t Test_HandleRequest_DeviceScreenResolution()
{
    return DelegateGetterTest("device.screenresolution");
}

// TEST_ID: AGC_L0_023
// Handler-map getter: device.videoresolution
uint32_t Test_HandleRequest_DeviceVideoResolution()
{
    return DelegateGetterTest("device.videoresolution");
}

// TEST_ID: AGC_L0_024
// Handler-map getter: device.hdcp
uint32_t Test_HandleRequest_DeviceHdcp()
{
    return DelegateGetterTest("device.hdcp");
}

// TEST_ID: AGC_L0_025
// Handler-map getter: device.hdr
uint32_t Test_HandleRequest_DeviceHdr()
{
    return DelegateGetterTest("device.hdr");
}

// TEST_ID: AGC_L0_026
// Handler-map getter: device.audio
uint32_t Test_HandleRequest_DeviceAudio()
{
    return DelegateGetterTest("device.audio");
}

// TEST_ID: AGC_L0_027
// Handler-map getter: voiceguidance.enabled
uint32_t Test_HandleRequest_VoiceGuidanceEnabled()
{
    return DelegateGetterTest("voiceguidance.enabled");
}

// TEST_ID: AGC_L0_028
// Handler-map getter: voiceguidance.navigationhints
uint32_t Test_HandleRequest_VoiceGuidanceNavigationHints()
{
    return DelegateGetterTest("voiceguidance.navigationhints");
}

// TEST_ID: AGC_L0_029
// Handler-map getter: accessibility.voiceguidancesettings with ctx.version="1.0.0"
// IsRDK8Compliant("1.0.0") == false → addSpeed = !false = true
uint32_t Test_HandleRequest_VoiceGuidanceSettings_NonRDK8()
{
    Exchange::GatewayContext ctx = DefaultContext();
    ctx.version = "1.0.0";
    return DelegateGetterTest("accessibility.voiceguidancesettings", ctx);
}

// TEST_ID: AGC_L0_030
// Handler-map getter: accessibility.voiceguidancesettings with ctx.version="8"
// IsRDK8Compliant("8") == true → addSpeed = !true = false
uint32_t Test_HandleRequest_VoiceGuidanceSettings_RDK8()
{
    Exchange::GatewayContext ctx = DefaultContext();
    ctx.version = "8";
    return DelegateGetterTest("accessibility.voiceguidancesettings", ctx);
}

// TEST_ID: AGC_L0_031
// Handler-map getter: accessibility.voiceguidance (always addSpeed=true)
uint32_t Test_HandleRequest_AccessibilityVoiceGuidance()
{
    return DelegateGetterTest("accessibility.voiceguidance");
}

// TEST_ID: AGC_L0_032
// Handler-map getter: accessibility.audiodescriptionsettings
uint32_t Test_HandleRequest_AccessibilityAudioDescriptionSettings()
{
    return DelegateGetterTest("accessibility.audiodescriptionsettings");
}

// TEST_ID: AGC_L0_033
// Handler-map getter: accessibility.audiodescription
uint32_t Test_HandleRequest_AccessibilityAudioDescription()
{
    return DelegateGetterTest("accessibility.audiodescription");
}

// TEST_ID: AGC_L0_034
// Handler-map getter: audiodescriptions.enabled
uint32_t Test_HandleRequest_AudioDescriptionsEnabled()
{
    return DelegateGetterTest("audiodescriptions.enabled");
}

// TEST_ID: AGC_L0_035
// Handler-map getter: accessibility.highcontrastui
uint32_t Test_HandleRequest_AccessibilityHighContrastUI()
{
    return DelegateGetterTest("accessibility.highcontrastui");
}

// TEST_ID: AGC_L0_036
// Handler-map getter: closedcaptions.enabled
uint32_t Test_HandleRequest_ClosedCaptionsEnabled()
{
    return DelegateGetterTest("closedcaptions.enabled");
}

// TEST_ID: AGC_L0_037
// Handler-map getter: closedcaptions.preferredlanguages
uint32_t Test_HandleRequest_ClosedCaptionsPreferredLanguages()
{
    return DelegateGetterTest("closedcaptions.preferredlanguages");
}

// TEST_ID: AGC_L0_038
// Handler-map getter: accessibility.closedcaptions
uint32_t Test_HandleRequest_AccessibilityClosedCaptions()
{
    return DelegateGetterTest("accessibility.closedcaptions");
}

// TEST_ID: AGC_L0_039
// Handler-map getter: accessibility.closedcaptionssettings
uint32_t Test_HandleRequest_AccessibilityClosedCaptionsSettings()
{
    return DelegateGetterTest("accessibility.closedcaptionssettings");
}

// TEST_ID: AGC_L0_040
// Handler-map getter: localization.language
uint32_t Test_HandleRequest_LocalizationLanguage()
{
    return DelegateGetterTest("localization.language");
}

// TEST_ID: AGC_L0_041
// Handler-map getter: localization.locale
uint32_t Test_HandleRequest_LocalizationLocale()
{
    return DelegateGetterTest("localization.locale");
}

// TEST_ID: AGC_L0_042
// Handler-map getter: localization.preferredaudiolanguages
uint32_t Test_HandleRequest_LocalizationPreferredAudioLanguages()
{
    return DelegateGetterTest("localization.preferredaudiolanguages");
}

// TEST_ID: AGC_L0_043
// Handler-map getter: localization.countrycode
uint32_t Test_HandleRequest_LocalizationCountryCode()
{
    return DelegateGetterTest("localization.countrycode");
}

// TEST_ID: AGC_L0_044
// Handler-map getter: localization.timezone
uint32_t Test_HandleRequest_LocalizationTimezone()
{
    return DelegateGetterTest("localization.timezone");
}

// TEST_ID: AGC_L0_045
// Handler-map getter: secondscreen.friendlyname
uint32_t Test_HandleRequest_SecondScreenFriendlyName()
{
    return DelegateGetterTest("secondscreen.friendlyname");
}

// TEST_ID: AGC_L0_046
// Handler-map pass-through: localization.addadditionalinfo → ERROR_NONE, result=="null"
uint32_t Test_HandleRequest_LocalizationAddAdditionalInfo()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.addadditionalinfo", R"({"key":"test","value":"val"})", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "localization.addadditionalinfo returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "localization.addadditionalinfo result is null");
    return tr.failures;
}

// TEST_ID: AGC_L0_047
// lifecycle.state returns ERROR_NONE (state lookup uses empty map, returns default state string).
uint32_t Test_HandleRequest_LifecycleState()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle.state returns ERROR_NONE in L0");
    return tr.failures;
}

// TEST_ID: AGC_L0_048
// lifecycle.close in L0 → mLifecycleManagerState is null → ERROR_GENERAL
uint32_t Test_HandleRequest_LifecycleClose()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "lifecycle.close returns ERROR_GENERAL in L0");
    return tr.failures;
}

// TEST_ID: AGC_L0_049
// lifecycle.finished always returns ERROR_NONE with result="null"
// LifecycleDelegate::LifecycleFinished unconditionally sets result="null" and returns ERROR_NONE.
uint32_t Test_HandleRequest_LifecycleFinished()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "lifecycle.finished", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle.finished returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "lifecycle.finished result is null");
    return tr.failures;
}

// TEST_ID: AGC_L0_050
// lifecycle2.state returns ERROR_NONE (state lookup uses empty map, returns default state string).
uint32_t Test_HandleRequest_Lifecycle2State()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "lifecycle2.state returns ERROR_NONE in L0");
    return tr.failures;
}

// TEST_ID: AGC_L0_051
// lifecycle2.close in L0 → mLifecycleManagerState is null → ERROR_GENERAL
uint32_t Test_HandleRequest_Lifecycle2Close()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "lifecycle2.close returns ERROR_GENERAL in L0");
    return tr.failures;
}

// TEST_ID: AGC_L0_052
// commoninternal.dispatchintent returns ERROR_NONE with result="null"
// LifecycleDelegate::DispatchLastIntent unconditionally sets result="null" and returns ERROR_NONE.
uint32_t Test_HandleRequest_DispatchIntent()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "commoninternal.dispatchintent", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "commoninternal.dispatchintent returns ERROR_NONE");
    ExpectEqStr(tr, result, "null", "commoninternal.dispatchintent result is null");
    return tr.failures;
}

// TEST_ID: AGC_L0_053
// commoninternal.getlastintent returns ERROR_NONE
// LifecycleDelegate::GetLastIntent calls GetLastKnownIntent → empty map → empty result, returns ERROR_NONE.
uint32_t Test_HandleRequest_GetLastIntent()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "commoninternal.getlastintent returns ERROR_NONE");
    return tr.failures;
}

// TEST_ID: AGC_L0_054
// advertising.advertisingid in L0 → SharedStorage unavailable
uint32_t Test_HandleRequest_AdvertisingId()
{
    return DelegateGetterTest("advertising.advertisingid");
}

// TEST_ID: AGC_L0_055
// device.uid in L0 → SharedStorage unavailable
uint32_t Test_HandleRequest_DeviceUid()
{
    return DelegateGetterTest("device.uid");
}

// TEST_ID: AGC_L0_056
// network.connected in L0 → NetworkDelegate
uint32_t Test_HandleRequest_NetworkConnected()
{
    return DelegateGetterTest("network.connected");
}
