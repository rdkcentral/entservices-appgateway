#include <iostream>
#include <string>
#include <cstdlib>

#include <core/core.h>
#include <plugins/IDispatcher.h>

#include <AppGateway.h>
#include "ServiceMock.h"

using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_NOT_SUPPORTED;
using WPEFramework::Core::ERROR_UNAVAILABLE;
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
- If APPGATEWAY_RESOLUTIONS_PATH is not set, tests will fallback to the ServiceMock ResolverFake and print a note.
*/

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

// Helper to print an informative message about APPGATEWAY_RESOLUTIONS_PATH.
// We do not require it because ResolverFake (ServiceMock) is used, but we log user guidance.
static void CheckResolutionsEnvOnce() {
    static bool printed = false;
    if (printed) {
        return;
    }
    printed = true;
    const char* path = std::getenv("APPGATEWAY_RESOLUTIONS_PATH");
    if (path == nullptr || *path == '\0') {
        std::cerr << "NOTE: APPGATEWAY_RESOLUTIONS_PATH not set; "
                     "falling back to ServiceMock ResolverFake overlay. Tests will continue." << std::endl;
    } else {
        std::cerr << "INFO: APPGATEWAY_RESOLUTIONS_PATH=" << path
                  << " (ResolverFake does not load it; provided for real implementation runs)" << std::endl;
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
    /** Exercise JSON-RPC "resolve" with full context and params; expect success and non-empty result. */
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

        ExpectEqU32(tr, rc, ERROR_NONE, "Resolve_HappyPath_JSONRPC returns ERROR_NONE");
        ExpectNotEmpty(tr, jsonResponse, "Resolve_HappyPath_JSONRPC JSON response");
        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Resolve_MissingFields_BadRequest() {
    /** Omit required fields one-at-a-time: requestId, connectionId, appId, origin, method => ERROR_BAD_REQUEST. */
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
            ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "Missing requestId => ERROR_BAD_REQUEST");
        }
        // Missing connectionId
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, false, true, true, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "Missing connectionId => ERROR_BAD_REQUEST");
        }
        // Missing appId
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, false, true, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "Missing appId => ERROR_BAD_REQUEST");
        }
        // Missing origin
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, false, true, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "Missing origin => ERROR_BAD_REQUEST");
        }
        // Missing method
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, false, true);
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_BAD_REQUEST, "Missing method => ERROR_BAD_REQUEST");
        }

        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Resolve_ParamsEmpty_DefaultsToEmptyObject() {
    /** params as empty string should be treated as {} and return success if method exists. */
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

        ExpectEqU32(tr, rc, ERROR_NONE, "Params empty defaults to {} and succeeds");
        ExpectNotEmpty(tr, jsonResponse, "Response non-empty when method exists");
        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_Resolve_ImplementationError_Propagates() {
    /** Force resolver to return NOT_AVAILABLE and NOT_SUPPORTED; JSON-RPC must surface same codes. */
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
            ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "Implementation error NOT_AVAILABLE propagates as ERROR_UNAVAILABLE");
        }

        // NOT_SUPPORTED
        response.clear();
        {
            const std::string paramsJson = BuildResolveParamsJson(true, true, true, true, true, true, "l0.notSupported");
            const uint32_t rc = dispatcher->Invoke(nullptr, 0, 0, "", "resolve", paramsJson, response);
            ExpectEqU32(tr, rc, ERROR_NOT_SUPPORTED, "Implementation error NOT_SUPPORTED propagates as ERROR_NOT_SUPPORTED");
        }

        dispatcher->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}
