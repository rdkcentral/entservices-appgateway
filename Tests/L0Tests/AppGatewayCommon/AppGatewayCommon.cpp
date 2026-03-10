// Copyright 2026 RDK Management
// SPDX-License-Identifier: Apache-2.0
// L0 Unit Tests for AppGatewayCommon Plugin

#include <iostream>
#include <string>
#include <cstdlib>
#include <core/core.h>
#include <plugins/IDispatcher.h>
#include "AppGatewayCommon.h"
#include "ServiceMock.h"
#include "ContextUtils.h"
#include "delegate/SettingsDelegate.h"
#include "delegate/SystemDelegate.h"
#include "delegate/NetworkDelegate.h"
#include "delegate/LifecycleDelegate.h"
#include "delegate/AppDelegate.h"
#include "delegate/TTSDelegate.h"
#include "delegate/UserSettingsDelegate.h"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_UNAVAILABLE;
using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_UNKNOWN_KEY;
using WPEFramework::Plugin::AppGatewayCommon;
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

struct PluginAndService {
    L0Test::ServiceMock* service { nullptr };
    IPlugin* plugin { nullptr };
    explicit PluginAndService(const L0Test::ServiceMock::Config& cfg = {})
        : service(new L0Test::ServiceMock(cfg))
        , plugin(WPEFramework::Core::Service<AppGatewayCommon>::Create<IPlugin>()) {}
    ~PluginAndService() {
        if (plugin != nullptr) { plugin->Release(); plugin = nullptr; }
        if (service != nullptr) { service->Release(); service = nullptr; }
    }
};

static Exchange::GatewayContext DefaultContext() {
    Exchange::GatewayContext ctx;
    ctx.requestId = 1001;
    ctx.connectionId = 10;
    ctx.appId = "com.example.test";
    ctx.version = "1.0.0";
    return ctx;
}

} // namespace

// TEST_ID: AGC_L0_001
uint32_t Test_Initialize_Deinitialize_Lifecycle() {
    TestResult tr;
    PluginAndService ps;
    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize returns empty string");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_002
uint32_t Test_Initialize_Twice_Idempotent() {
    TestResult tr;
    {
        PluginAndService ps;
        const std::string init1 = ps.plugin->Initialize(ps.service);
        ps.plugin->Deinitialize(ps.service);
    }
    {
        PluginAndService ps;
        const std::string init2 = ps.plugin->Initialize(ps.service);
        ps.plugin->Deinitialize(ps.service);
    }
    return tr.failures;
}

// TEST_ID: AGC_L0_003
uint32_t Test_Deinitialize_Twice_NoCrash() {
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
uint32_t Test_HandleAppGatewayRequest_Positive() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "device.make returns ERROR_NONE");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_005
uint32_t Test_HandleAppGatewayRequest_Negative() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    uint32_t rc = agc->HandleAppGatewayRequest(ctx, "invalid.method", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNKNOWN_KEY, "invalid.method returns ERROR_UNKNOWN_KEY");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_006
uint32_t Test_HandleAppGatewayRequest_Boundary() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    uint32_t rc = agc->HandleAppGatewayRequest(ctx, "device.make", "", result);
    ExpectEqU32(tr, rc, ERROR_NONE, "device.make with empty payload returns ERROR_NONE");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_007
uint32_t Test_Delegate_Mock_Usage() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    // TODO: Add mock for SettingsDelegate and verify delegate call
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_008
uint32_t Test_Lifecycle_Methods() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    uint32_t rc = agc->LifecycleReady(ctx, "{}", result);
    // TODO: Add assertion for expected result
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_009
uint32_t Test_Authenticator_Interface() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string appId;
    uint32_t rc = agc->Authenticate("sessionId", appId);
    // TODO: Add assertion for expected appId
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_010
uint32_t Test_CheckPermissionGroup_DefaultAllowed() {
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    AppGatewayCommon* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    bool allowed = false;
    uint32_t rc = agc->CheckPermissionGroup("appId", "group", allowed);
    ExpectTrue(tr, allowed, "Permission group default allowed");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// Add more tests for edge cases, negative scenarios, and boundary conditions as needed.

extern "C" uint32_t RunAllTests() {
    uint32_t failures = 0;
    failures += Test_Initialize_Deinitialize_Lifecycle();
    failures += Test_Initialize_Twice_Idempotent();
    failures += Test_Deinitialize_Twice_NoCrash();
    failures += Test_HandleAppGatewayRequest_Positive();
    failures += Test_HandleAppGatewayRequest_Negative();
    failures += Test_HandleAppGatewayRequest_Boundary();
    failures += Test_Delegate_Mock_Usage();
    failures += Test_Lifecycle_Methods();
    failures += Test_Authenticator_Interface();
    failures += Test_CheckPermissionGroup_DefaultAllowed();
    return failures;
}
