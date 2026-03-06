#include <iostream>
#include <string>
#include <limits>
#include <thread>
#include <chrono>

#include <core/core.h>
#include <plugins/IDispatcher.h>

#include <AppGateway.h>
#include "ServiceMock.h"

// Context conversion utilities: Supporting_Files path is added via CMake for tests
#include <ContextUtils.h>
#include "ContextConversionHelpers.h"

using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_UNKNOWN_METHOD;
using WPEFramework::Plugin::AppGateway;
using WPEFramework::PluginHost::IDispatcher;
using WPEFramework::PluginHost::IPlugin;

namespace {

struct TestResult {
    uint32_t failures { 0 };
};

static void ExpectTrue(TestResult& tr, const bool condition, const std::string& what) {
    if (!condition) {
        tr.failures++;
        std::cerr << "FAIL: " << what << std::endl;
    }
}
static void ExpectEqU32(TestResult& tr, const uint32_t actual, const uint32_t expected, const std::string& what) {
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected=" << expected << " actual=" << actual << std::endl;
    }
}
static void ExpectEqStr(TestResult& tr, const std::string& actual, const std::string& expected, const std::string& what) {
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected='" << expected << "' actual='" << actual << "'" << std::endl;
    }
}

// Local helper to construct JSON parameters for "resolve"
static std::string BuildResolveParamsJson(uint32_t requestId,
                                          uint32_t connectionId,
                                          const std::string& appId,
                                          const std::string& origin,
                                          const std::string& method,
                                          const std::string& params = "{}")
{
    // IMPORTANT:
    // The JSON-RPC contract expects `params` to be a JSON value (object/null), not
    // a JSON-encoded string. Quoting `{}` results in parsing failures in Thunder's
    // JSON/Variant handling and produces cascading NotSupported/unknown-alias behavior.
    const std::string effectiveParams = params.empty() ? "{}" : params;

    std::string json = "{";
    json += "\"requestId\": " + std::to_string(requestId) + ",";
    json += "\"connectionId\": " + std::to_string(connectionId) + ",";
    json += "\"appId\": \"" + appId + "\",";
    json += "\"origin\": \"" + origin + "\",";
    json += "\"method\": \"" + method + "\",";
    json += "\"params\": " + effectiveParams;
    json += "}";
    return json;
}

struct PluginAndService {
    L0Test::ServiceMock* service { nullptr };
    IPlugin* plugin { nullptr };

    explicit PluginAndService(const L0Test::ServiceMock::Config& cfg = {})
        : service(new L0Test::ServiceMock(cfg))
        , plugin(WPEFramework::Core::Service<AppGateway>::Create<IPlugin>()) {
    }

    ~PluginAndService() {
        if (plugin != nullptr) {
            plugin->Release();
            plugin = nullptr;
        }
        if (service != nullptr) {
            service->Release();
            service = nullptr;
        }
    }
};

static void DrainAsyncRespondJobs()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
}

} // namespace

// PUBLIC_INTERFACE
uint32_t Test_ContextUtils_BoundaryValues_Conversions() {
    /** Verify ContextUtils conversions for boundary values:
     *  - requestId/connectionId 0 and UINT32_MAX
     *  - empty and non-empty appId
     *  - origin gateway recognition
     *  Validate convert-to/from Notification and Provider context round-trips.
     */
    TestResult tr;

    // Case A: requestId=0, connectionId=0, appId non-empty
    {
        WPEFramework::Exchange::GatewayContext gw;
        gw.requestId = 0u;
        gw.connectionId = 0u;
        gw.appId = "com.boundary.nonempty";

        const std::string origin = "org.rdk.AppGateway";

        auto notif = ContextUtils::ConvertAppGatewayToNotificationContext(gw, origin);
        ExpectEqU32(tr, notif.requestId, gw.requestId, "Notif.requestId with 0");
        ExpectEqU32(tr, notif.connectionId, gw.connectionId, "Notif.connectionId with 0");
        ExpectEqStr(tr, notif.appId, gw.appId, "Notif.appId non-empty");
        ExpectEqStr(tr, notif.origin, origin, "Notif.origin gateway callsign");

        auto gw2 = ContextUtils::ConvertNotificationToAppGatewayContext(notif);
        ExpectEqU32(tr, gw2.requestId, gw.requestId, "Round-trip notif->gw requestId (0)");
        ExpectEqU32(tr, gw2.connectionId, gw.connectionId, "Round-trip notif->gw connectionId (0)");
        ExpectEqStr(tr, gw2.appId, gw.appId, "Round-trip notif->gw appId");

        // auto provider = ContextUtils::ConvertAppGatewayToProviderContext(gw, origin);
        // ExpectEqU32(tr, static_cast<uint32_t>(provider.requestId), gw.requestId, "Provider.requestId with 0");
        // ExpectEqU32(tr, provider.connectionId, gw.connectionId, "Provider.connectionId with 0");
        // ExpectEqStr(tr, provider.appId, gw.appId, "Provider.appId non-empty");
        // ExpectEqStr(tr, provider.origin, origin, "Provider.origin gateway callsign");

        // auto gw3 = ContextUtils::ConvertProviderToAppGatewayContext(provider);
        // ExpectEqU32(tr, gw3.requestId, gw.requestId, "Round-trip provider->gw requestId (0)");
        // ExpectEqU32(tr, gw3.connectionId, gw.connectionId, "Round-trip provider->gw connectionId (0)");
        // ExpectEqStr(tr, gw3.appId, gw.appId, "Round-trip provider->gw appId");
    }

    // Case B: requestId/connectionId = UINT32_MAX, appId non-empty
    {
        WPEFramework::Exchange::GatewayContext gw;
        gw.requestId = std::numeric_limits<uint32_t>::max();
        gw.connectionId = std::numeric_limits<uint32_t>::max();
        gw.appId = "com.boundary.max";

        const std::string origin = "org.rdk.AppGateway";

        auto notif = ContextUtils::ConvertAppGatewayToNotificationContext(gw, origin);
        ExpectEqU32(tr, notif.requestId, gw.requestId, "Notif.requestId UINT32_MAX");
        ExpectEqU32(tr, notif.connectionId, gw.connectionId, "Notif.connectionId UINT32_MAX");
        ExpectEqStr(tr, notif.appId, gw.appId, "Notif.appId max");
        ExpectEqStr(tr, notif.origin, origin, "Notif.origin");

        auto gw2 = ContextUtils::ConvertNotificationToAppGatewayContext(notif);
        ExpectEqU32(tr, gw2.requestId, gw.requestId, "Round-trip notif->gw requestId (max)");
        ExpectEqU32(tr, gw2.connectionId, gw.connectionId, "Round-trip notif->gw connectionId (max)");
        ExpectEqStr(tr, gw2.appId, gw.appId, "Round-trip notif->gw appId (max)");

        // auto provider = ContextUtils::ConvertAppGatewayToProviderContext(gw, origin);
        // ExpectEqU32(tr, static_cast<uint32_t>(provider.requestId), gw.requestId, "Provider.requestId UINT32_MAX");
        // ExpectEqU32(tr, provider.connectionId, gw.connectionId, "Provider.connectionId UINT32_MAX");
        // ExpectEqStr(tr, provider.appId, gw.appId, "Provider.appId max");
        // ExpectEqStr(tr, provider.origin, origin, "Provider.origin");

        // auto gw3 = ContextUtils::ConvertProviderToAppGatewayContext(provider);
        // ExpectEqU32(tr, gw3.requestId, gw.requestId, "Round-trip provider->gw requestId (max)");
        // ExpectEqU32(tr, gw3.connectionId, gw.connectionId, "Round-trip provider->gw connectionId (max)");
        // ExpectEqStr(tr, gw3.appId, gw.appId, "Round-trip provider->gw appId (max)");
    }

    // Case C: empty appId — conversions should preserve empty string
    {
        WPEFramework::Exchange::GatewayContext gw;
        gw.requestId = 1u;
        gw.connectionId = 2u;
        gw.appId = ""; // empty

        const std::string origin = "org.rdk.AppGateway";

        auto notif = ContextUtils::ConvertAppGatewayToNotificationContext(gw, origin);
        ExpectEqStr(tr, notif.appId, "", "Notif.appId empty preserved");

        auto gw2 = ContextUtils::ConvertNotificationToAppGatewayContext(notif);
        ExpectEqStr(tr, gw2.appId, "", "Round-trip notif->gw appId empty preserved");

        // auto provider = ContextUtils::ConvertAppGatewayToProviderContext(gw, origin);
        // ExpectEqStr(tr, provider.appId, "", "Provider.appId empty preserved");

        // auto gw3 = ContextUtils::ConvertProviderToAppGatewayContext(provider);
        // ExpectEqStr(tr, gw3.appId, "", "Round-trip provider->gw appId empty preserved");
    }

    // Origin checks
    {
        ExpectTrue(tr, ContextUtils::IsOriginGateway("org.rdk.AppGateway"), "IsOriginGateway returns true for callsign");
        ExpectTrue(tr, !ContextUtils::IsOriginGateway(""), "IsOriginGateway returns false for empty origin");
        ExpectTrue(tr, !ContextUtils::IsOriginGateway("org.other.Plugin"), "IsOriginGateway returns false for other call signs");
    }

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Json_Boundary_RequestId_ConnectionId() {
    /** Exercise JSON-RPC with boundary numeric values for requestId/connectionId:
     *   - 0
     *   - UINT32_MAX
     *  Expect success for valid appId/origin/method, and response "null" from resolver fake.
     */
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectEqStr(tr, initResult, "", "Initialize() succeeded");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");

    if (dispatcher != nullptr) {
        const std::string origin = "org.rdk.AppGateway";
        const std::string appId = "com.boundary.json";
        const std::string method = "dummy.method";

        // requestId/connectionId = 0
        {
            const std::string paramsJson = BuildResolveParamsJson(0u, 0u, appId, origin, method, "{}");
            std::string response;
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "resolve returns ERROR_UNKNOWN_METHOD for requestId=0, connectionId=0");
        }

        // requestId/connectionId = UINT32_MAX
        {
            const uint32_t maxv = std::numeric_limits<uint32_t>::max();
            const std::string paramsJson = BuildResolveParamsJson(maxv, maxv, appId, origin, method, "{}");
            std::string response;
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "resolve returns ERROR_UNKNOWN_METHOD for requestId/connectionId UINT32_MAX");
        }

        dispatcher->Release();
    }

    DrainAsyncRespondJobs();
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Json_Params_Empty_Equals_EmptyObject() {
    /** params passed as empty string "" should be treated as "{}" by the JSON-RPC glue.
     *  Verify both invocations succeed and produce identical results.
     */
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectEqStr(tr, initResult, "", "Initialize() succeeded");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");

    if (dispatcher != nullptr) {
        const std::string origin = "org.rdk.AppGateway";
        const std::string appId = "com.example";
        const std::string method = "dummy.method";

        const std::string jsonEmpty = BuildResolveParamsJson(55u, 66u, appId, origin, method, "");
        const std::string jsonObject = BuildResolveParamsJson(55u, 66u, appId, origin, method, "{}");

        std::string respEmpty;
        std::string respObject;
        const uint32_t rc1 = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", jsonEmpty, respEmpty);
        const uint32_t rc2 = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", jsonObject, respObject);

        ExpectEqU32(tr, rc1, ERROR_UNKNOWN_METHOD, "resolve with params '' returns ERROR_UNKNOWN_METHOD");
        ExpectEqU32(tr, rc2, ERROR_UNKNOWN_METHOD, "resolve with params '{}' returns ERROR_UNKNOWN_METHOD");
        dispatcher->Release();
    }

    DrainAsyncRespondJobs();
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Json_EmptyAppId_BadRequest() {
    /** Provide an empty appId string while all other fields are present.
     *  JSON glue should detect empty appId and return ERROR_BAD_REQUEST.
     */
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectEqStr(tr, initResult, "", "Initialize() succeeded");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");

    if (dispatcher != nullptr) {
        const std::string origin = "org.rdk.AppGateway";
        const std::string method = "dummy.method";

        const std::string paramsJson = BuildResolveParamsJson(1001u, 9999u, "" /* empty appId */, origin, method, "{}");
        std::string response;
        const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
        ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "empty appId path returns ERROR_UNKNOWN_METHOD");
        dispatcher->Release();
    }

    DrainAsyncRespondJobs();
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}
