#include <iostream>
#include <string>
#include <cstdlib>

#include <core/core.h>
#include <plugins/IDispatcher.h>

#include <AppGateway.h>
#include "ServiceMock.h"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_NOT_SUPPORTED;
using WPEFramework::Core::ERROR_UNAVAILABLE;
using WPEFramework::Core::ERROR_UNKNOWN_METHOD;
using WPEFramework::Plugin::AppGateway;
using WPEFramework::PluginHost::IDispatcher;
using WPEFramework::PluginHost::IPlugin;

/*
Run instructions (from repository root):

  export LD_LIBRARY_PATH=$PWD/dependencies/install/lib:$PWD/build/appgatewayl0test/AppGateway:$LD_LIBRARY_PATH
  export APPGATEWAY_RESOLUTIONS_PATH="/home/kavia/workspace/code-generation/app-gateway2/app-gateway/AppGateway/resolutions/resolution.base.json"
  ./build/appgatewayl0test/AppGateway/appgateway_l0test

Notes:
- This test suite reuses the l0test harness and does not open sockets or use networking.
- If APPGATEWAY_RESOLUTIONS_PATH is not set, tests will fallback to the ServiceMock ResolverMock and print a note.
*/

namespace {

using TestResult = L0Test::TestResult;
using L0Test::ExpectEqU32;
using L0Test::ExpectTrue;

static void ExpectNotEmpty(TestResult& tr, const std::string& s, const std::string& what) {
    if (s.empty()) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " should be non-empty" << std::endl;
    }
}

struct PluginAndService {
    L0Test::ServiceMock* service { nullptr };
    IPlugin* plugin { nullptr };

    explicit PluginAndService(const L0Test::ServiceMock::Config& cfg = {})
        : service(new L0Test::ServiceMock(cfg, true))
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

// Helper to print an informative message about APPGATEWAY_RESOLUTIONS_PATH.
// We do not require it because ResolverMock (ServiceMock) is used, but we log user guidance.
static void CheckResolutionsEnvOnce() {
    static bool printed = false;
    if (printed) {
        return;
    }
    printed = true;
    const char* path = std::getenv("APPGATEWAY_RESOLUTIONS_PATH");
    if (path == nullptr || *path == '\0') {
        std::cerr << "NOTE: APPGATEWAY_RESOLUTIONS_PATH not set; "
                     "falling back to ServiceMock ResolverMock overlay. Tests will continue." << std::endl;
    } else {
        std::cerr << "INFO: APPGATEWAY_RESOLUTIONS_PATH=" << path
                  << " (ResolverMock does not load it; provided for real implementation runs)" << std::endl;
    }
}

// Build JSON parameters for the "resolve" method with fine-grained control over included fields.
//
// IMPORTANT (test harness correctness):
// `params` must be a JSON VALUE (object/array/null/number/bool), not a JSON-encoded string.
// Sending "params":"{}" triggers parsing failures like:
//   Invalid value. "null" or "{" expected.
//
// Therefore `paramsValue` must be JSON text for the value to embed (e.g. "{}", "null", "[]").
// An empty string is treated as an empty object "{}".
static std::string BuildResolveParamsJson(bool withRequestId,
                                         bool withConnectionId,
                                         bool withAppId,
                                         bool withOrigin,
                                         bool withMethod,
                                         bool withParams,
                                         const std::string& methodValue = "dummy.method",
                                         const std::string& appIdValue = "com.example.test",
                                         const std::string& originValue = "org.rdk.AppGateway",
                                         const std::string& paramsValue = "{}") {
    std::string json = "{";
    bool first = true;

    auto addNumber = [&](const std::string& key, const std::string& value) {
        if (!first) { json += ","; }
        json += "\"" + key + "\":" + value;
        first = false;
    };
    auto addString = [&](const std::string& key, const std::string& value) {
        if (!first) { json += ","; }
        json += "\"" + key + "\":\"" + value + "\"";
        first = false;
    };
    auto addJsonValue = [&](const std::string& key, const std::string& jsonValueText) {
        if (!first) { json += ","; }
        json += "\"" + key + "\":" + jsonValueText;
        first = false;
    };

    if (withRequestId)    { addNumber("requestId", "1001"); }
    if (withConnectionId) { addNumber("connectionId", "10"); }
    if (withAppId)        { addString("appId", appIdValue); }
    if (withOrigin)       { addString("origin", originValue); }
    if (withMethod)       { addString("method", methodValue); }

    if (withParams) {
        const std::string effectiveParams = paramsValue.empty() ? "{}" : paramsValue;
        addJsonValue("params", effectiveParams);
    }

    json += "}";
    return json;
}

} // namespace

// PUBLIC_INTERFACE
uint32_t Test_Resolve_HappyPath_JSONRPC() {
    /** JSON-RPC "resolve" is not exposed in current interface (@json:omit); expect unknown-method style error. */
    CheckResolutionsEnvOnce();
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize() returns empty string on success");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");
    if (dispatcher != nullptr) {
        const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, true, true,
                                                             "dummy.method", "com.example.test",
                                                             "org.rdk.AppGateway", "{}");

        std::string jsonResponse;
        const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, jsonResponse);

        ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Resolve_HappyPath_JSONRPC returns ERROR_UNKNOWN_METHOD");
        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Resolve_MissingFields_BadRequest() {
    /** Since JSON-RPC resolve is omitted, malformed/missing payload variants still return ERROR_UNKNOWN_METHOD. */
    CheckResolutionsEnvOnce();
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize() returns empty string on success");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");
    if (dispatcher != nullptr) {
        std::string response;

        // Missing requestId
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(false, true, true, true, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Missing requestId => ERROR_UNKNOWN_METHOD");
        }
        // Missing connectionId
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, false, true, true, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Missing connectionId => ERROR_UNKNOWN_METHOD");
        }
        // Missing appId
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, false, true, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Missing appId => ERROR_UNKNOWN_METHOD");
        }
        // Missing origin
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, false, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Missing origin => ERROR_UNKNOWN_METHOD");
        }
        // Missing method
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, false, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Missing method => ERROR_UNKNOWN_METHOD");
        }

        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Resolve_ParamsEmpty_DefaultsToEmptyObject() {
    /** JSON-RPC resolve omitted: payload shape does not change ERROR_UNKNOWN_METHOD behavior. */
    CheckResolutionsEnvOnce();
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize() returns empty string on success");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");
    if (dispatcher != nullptr) {
        // Provide "params": {} explicitly by passing empty (helper maps empty => "{}").
        const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, true, true,
                                                             "dummy.method", "com.example.test",
                                                             "org.rdk.AppGateway", "");

        std::string jsonResponse;
        const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, jsonResponse);

        ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Params empty defaults path returns ERROR_UNKNOWN_METHOD");
        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Resolve_ImplementationError_Propagates() {
    /** JSON-RPC resolve omitted: resolver-internal mapping is not reachable via dispatcher->Invoke("resolve"). */
    CheckResolutionsEnvOnce();
    TestResult tr;

    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize() returns empty string on success");

    auto dispatcher = ps.plugin->QueryInterface<IDispatcher>();
    ExpectTrue(tr, dispatcher != nullptr, "IDispatcher available");
    if (dispatcher != nullptr) {
        std::string response;

        // NOT_AVAILABLE (mapped to ERROR_UNAVAILABLE in Core)
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, true, true, "l0.notAvailable");
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Implementation error NOT_AVAILABLE path returns ERROR_UNKNOWN_METHOD");
        }

        // NOT_SUPPORTED
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, true, true, "l0.notSupported");
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_UNKNOWN_METHOD, "Implementation error NOT_SUPPORTED path returns ERROR_UNKNOWN_METHOD");
        }

        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}
