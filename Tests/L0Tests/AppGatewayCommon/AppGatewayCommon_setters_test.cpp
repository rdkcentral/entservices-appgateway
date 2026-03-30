#include "AppGatewayCommon_common_test.h"

// TEST_ID: AGC_L0_057
// Setter with invalid payload returns ERROR_BAD_REQUEST.
uint32_t Test_HandleRequest_SetterInvalidPayload()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // device.setname expects a JSON with "value" field; empty object should fail validation.
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "device.setname", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "device.setname with missing 'value' returns ERROR_BAD_REQUEST");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_058
// Setter with valid payload but no backing plugin returns ERROR_GENERAL (delegate unavailable).
uint32_t Test_HandleRequest_SetterValidPayload_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // device.setname with valid payload; delegate calls SystemDelegate::SetDeviceName which fails in L0.
    // In L0, SystemDelegate::SetDeviceName → AcquireLink() returns null → ERROR_UNAVAILABLE.
    // ResponseUtils::SetNullResponseForSuccess passes error code through unchanged.
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "device.setname", R"({"value":"TestName"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "device.setname with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_059
// HandleAppGatewayRequest for "voiceguidance.setenabled" with invalid non-bool payload
// returns ERROR_BAD_REQUEST.
uint32_t Test_HandleRequest_BoolSetterInvalidPayload()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    // Expects {"value": true/false}, providing a string instead.
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", R"({"value":"notabool"})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setenabled with string value returns ERROR_BAD_REQUEST");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_060
// localization.setcountrycode with invalid payload (missing value) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_SetCountryCode_InvalidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setcountrycode", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.setcountrycode with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_061
// localization.setcountrycode with valid payload → delegate unavailable
uint32_t Test_HandleRequest_SetCountryCode_ValidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setcountrycode", R"({"value":"US"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setcountrycode with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_062
// localization.settimezone with invalid payload (missing value) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_SetTimezone_InvalidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.settimezone", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.settimezone with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_063
// localization.settimezone with valid payload → delegate unavailable
uint32_t Test_HandleRequest_SetTimezone_ValidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.settimezone", R"({"value":"America/New_York"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.settimezone with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_064
// localization.setlocale with invalid payload (missing value) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_SetLocale_InvalidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setlocale", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.setlocale with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_065
// localization.setlocale with valid payload → delegate unavailable
uint32_t Test_HandleRequest_SetLocale_ValidPayload()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setlocale", R"({"value":"en-US"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setlocale with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_066
// voiceguidance.setenabled with valid bool payload → delegate unavailable
uint32_t Test_HandleRequest_VoiceGuidanceSetEnabled_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", R"({"value":true})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setenabled with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_067
// voiceguidance.speed getter → acceptable code in L0
uint32_t Test_HandleRequest_VoiceGuidanceSpeed()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, "voiceguidance.speed returns acceptable code in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_068
// voiceguidance.rate alias → same handler as voiceguidance.speed
uint32_t Test_HandleRequest_VoiceGuidanceRate()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.rate", "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, "voiceguidance.rate returns acceptable code in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_069
// voiceguidance.setspeed with boundary value 0.5 (FLOAT type) → rejected by ValidateAndExtractDouble
uint32_t Test_HandleRequest_SetSpeed_MinBoundary()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":0.5})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setspeed with 0.5 (FLOAT) returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_070
// voiceguidance.setspeed with 2.0 — Number() truncates to 2, passes validation, delegate unavailable
uint32_t Test_HandleRequest_SetSpeed_MaxBoundary()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":2.0})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setspeed with 2.0 returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_071
// voiceguidance.setspeed with 0.49 (below min) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_SetSpeed_BelowMin()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":0.49})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setspeed with 0.49 returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_072
// voiceguidance.setspeed with 2.01 — Number() truncates to 2, passes validation, delegate unavailable
uint32_t Test_HandleRequest_SetSpeed_AboveMax()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":2.01})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setspeed with 2.01 — Number() truncates to 2, delegate unavailable in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_073
// voiceguidance.setrate alias — same handler as setspeed, 1.0 Number() truncates to 1, delegate unavailable
uint32_t Test_HandleRequest_SetRate_Alias()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setrate", R"({"value":1.0})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setrate with 1.0 returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_074
// voiceguidance.setnavigationhints with invalid payload (string not bool) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_SetNavigationHints_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", R"({"value":"yes"})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "voiceguidance.setnavigationhints with string value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_075
// voiceguidance.setnavigationhints with valid bool payload → delegate unavailable
uint32_t Test_HandleRequest_SetNavigationHints_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", R"({"value":false})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "voiceguidance.setnavigationhints with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_076
// audiodescriptions.setenabled with invalid payload (missing value) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_AudioDescSetEnabled_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "audiodescriptions.setenabled with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_077
// audiodescriptions.setenabled with valid bool payload → delegate unavailable
uint32_t Test_HandleRequest_AudioDescSetEnabled_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", R"({"value":true})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "audiodescriptions.setenabled with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_078
// closedcaptions.setenabled with invalid payload (missing value) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_CCSetEnabled_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{}", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "closedcaptions.setenabled with missing value returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_079
// closedcaptions.setenabled with valid bool payload → delegate unavailable
uint32_t Test_HandleRequest_CCSetEnabled_Valid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", R"({"value":true})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "closedcaptions.setenabled with valid payload returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_080
// closedcaptions.setpreferredlanguages with invalid payload (number, not string/array) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_CCSetPrefLangs_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":123})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "closedcaptions.setpreferredlanguages with number returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_081
// closedcaptions.setpreferredlanguages with valid string → delegate unavailable
uint32_t Test_HandleRequest_CCSetPrefLangs_ValidString()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":"en"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "closedcaptions.setpreferredlanguages with string returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_082
// closedcaptions.setpreferredlanguages with valid array → delegate unavailable
uint32_t Test_HandleRequest_CCSetPrefLangs_ValidArray()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":["en","fr"]})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "closedcaptions.setpreferredlanguages with array returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_083
// localization.setpreferredaudiolanguages with invalid payload (number, not string/array) → ERROR_BAD_REQUEST
uint32_t Test_HandleRequest_SetPrefAudioLangs_Invalid()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":123})", result);
    ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "localization.setpreferredaudiolanguages with number returns ERROR_BAD_REQUEST");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_084
// localization.setpreferredaudiolanguages with valid string → delegate unavailable
uint32_t Test_HandleRequest_SetPrefAudioLangs_ValidString()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":"en"})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setpreferredaudiolanguages with string returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_085
// localization.setpreferredaudiolanguages with valid array → delegate unavailable
uint32_t Test_HandleRequest_SetPrefAudioLangs_ValidArray()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":["en","fr"]})", result);
    const bool ok = (rc == ERROR_GENERAL || rc == ERROR_UNAVAILABLE);
    ExpectTrue(tr, ok, "localization.setpreferredaudiolanguages with array returns ERROR_GENERAL|ERROR_UNAVAILABLE in L0");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}
