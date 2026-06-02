#include "AppGatewayCommon_common_test.h"

// ============================================================================
// L0 tests for the five Firebolt Display module APIs implemented in
// AppGatewayCommon via IConnectionProperties / IDisplayProperties (ComRPC)
// and org.rdk.DisplaySettings (JSONRPC for videoResolutions).
//
// In L0 all real Thunder plugins are absent, so QueryInterfaceByCallsign
// returns nullptr for every ComRPC interface and AcquireLink returns nullptr
// for every JSONRPC callsign.  The contract for all five APIs in this case is:
//   - return Core::ERROR_NONE (graceful "no display" path)
//   - result contains the appropriate empty/zero default value
// ============================================================================

// ---- Display.edid ----------------------------------------------------------

// TEST_ID: AGC_L0_098
// Display.edid routes to GetDisplayEdid. With no IConnectionProperties
// available the method returns ERROR_NONE and result is the empty JSON string "".
uint32_t Test_HandleRequest_DisplayEdid_NoDisplay()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "display.edid", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "display.edid returns ERROR_NONE when no display present");
    ExpectEqStr(tr, result, "\"\"", "display.edid result is empty JSON string when no display present");
    return tr.failures;
}

// TEST_ID: AGC_L0_099
// Display.edid is case-insensitive: "Display.EDID" is lowered internally and
// resolves to the same handler.
uint32_t Test_HandleRequest_DisplayEdid_CaseInsensitive()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "Display.EDID", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "Display.EDID (mixed-case) returns ERROR_NONE");
    ExpectEqStr(tr, result, "\"\"", "Display.EDID (mixed-case) returns empty JSON string");
    return tr.failures;
}

// ---- Display.size ----------------------------------------------------------

// TEST_ID: AGC_L0_100
// Display.size returns {"width":0,"height":0} when IConnectionProperties is unavailable.
uint32_t Test_HandleRequest_DisplaySize_NoDisplay()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "display.size", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "display.size returns ERROR_NONE when no display present");
    // Result must contain width:0 and height:0
    ExpectTrue(tr, result.find("\"width\"") != std::string::npos, "display.size result contains 'width' field");
    ExpectTrue(tr, result.find("\"height\"") != std::string::npos, "display.size result contains 'height' field");
    ExpectTrue(tr, result.find(":0") != std::string::npos, "display.size result contains zero value");
    return tr.failures;
}

// TEST_ID: AGC_L0_101
// Display.size is case-insensitive.
uint32_t Test_HandleRequest_DisplaySize_CaseInsensitive()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "Display.Size", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "Display.Size (mixed-case) returns ERROR_NONE");
    ExpectTrue(tr, result.find("width") != std::string::npos, "Display.Size result contains width");
    return tr.failures;
}

// ---- Display.maxResolution -------------------------------------------------

// TEST_ID: AGC_L0_102
// Display.maxResolution returns {"width":0,"height":0} when IConnectionProperties
// is unavailable.
uint32_t Test_HandleRequest_DisplayMaxResolution_NoDisplay()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "display.maxresolution", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "display.maxresolution returns ERROR_NONE when no display present");
    ExpectTrue(tr, result.find("\"width\"") != std::string::npos, "display.maxresolution result contains 'width'");
    ExpectTrue(tr, result.find("\"height\"") != std::string::npos, "display.maxresolution result contains 'height'");
    ExpectTrue(tr, result.find(":0") != std::string::npos, "display.maxresolution result has zero dimensions");
    return tr.failures;
}

// TEST_ID: AGC_L0_103
// Display.maxResolution is case-insensitive.
uint32_t Test_HandleRequest_DisplayMaxResolution_CaseInsensitive()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "DISPLAY.MAXRESOLUTION", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "DISPLAY.MAXRESOLUTION (uppercase) returns ERROR_NONE");
    ExpectTrue(tr, result.find("width") != std::string::npos, "DISPLAY.MAXRESOLUTION result contains width");
    return tr.failures;
}

// ---- Display.colorimetry ---------------------------------------------------

// TEST_ID: AGC_L0_104
// Display.colorimetry returns [] when IDisplayProperties is unavailable.
uint32_t Test_HandleRequest_DisplayColorimetry_NoDisplay()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "display.colorimetry", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "display.colorimetry returns ERROR_NONE when no display present");
    ExpectEqStr(tr, result, "[]", "display.colorimetry result is empty array when no display present");
    return tr.failures;
}

// TEST_ID: AGC_L0_105
// Display.colorimetry is case-insensitive.
uint32_t Test_HandleRequest_DisplayColorimetry_CaseInsensitive()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "Display.Colorimetry", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "Display.Colorimetry (mixed-case) returns ERROR_NONE");
    ExpectEqStr(tr, result, "[]", "Display.Colorimetry (mixed-case) returns empty array");
    return tr.failures;
}

// ---- Display.videoResolutions ----------------------------------------------

// TEST_ID: AGC_L0_106
// Display.videoResolutions returns [] when DisplaySettings JSONRPC link is unavailable.
uint32_t Test_HandleRequest_DisplayVideoResolutions_NoDisplay()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "display.videoresolutions returns ERROR_NONE when no display present");
    ExpectEqStr(tr, result, "[]", "display.videoresolutions result is empty array when no display present");
    return tr.failures;
}

// TEST_ID: AGC_L0_107
// Display.videoResolutions is case-insensitive.
uint32_t Test_HandleRequest_DisplayVideoResolutions_CaseInsensitive()
{
    TestResult tr;
    PluginAndService& ps = SharedFixture::instance().ps();
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();

    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "Display.VideoResolutions", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "Display.VideoResolutions (mixed-case) returns ERROR_NONE");
    ExpectEqStr(tr, result, "[]", "Display.VideoResolutions (mixed-case) returns empty array");
    return tr.failures;
}
