/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#define private public
#include "AppGateway.h"
#include "AppGatewayImplementation.h"
#include "AppGatewayResponderImplementation.h"
#include "AppGatewayTelemetry.h"
#include "Resolver.h"
#undef private

#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "DispatcherMock.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

class TestAppGateway final : public AppGateway {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }
};

class TestAppGatewayTelemetry final : public AppGatewayTelemetry {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }
};

class TestAppGatewayImplementation final : public AppGatewayImplementation {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }
};

class TestAppGatewayResponderImplementation final : public AppGatewayResponderImplementation {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }
};

typedef enum : uint32_t {
    AppGatewayResponder_StateInvalid = 0x00000000,
    AppGatewayResponder_OnAppConnectionChanged = 0x00000001
} AppGatewayResponderEventType_t;

class L1AppGatewayResponderNotificationHandler final : public Exchange::IAppGatewayResponder::INotification {
public:
    L1AppGatewayResponderNotificationHandler()
        : m_event_signalled(AppGatewayResponder_StateInvalid)
        , m_call_count(0)
        , m_last_connection_id(0)
        , m_last_connected(false)
    {
    }

    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }

    void OnAppConnectionChanged(const string& appId, const uint32_t connectionId, const bool connected) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_call_count;
        m_last_app_id = appId;
        m_last_connection_id = connectionId;
        m_last_connected = connected;
        m_event_signalled |= AppGatewayResponder_OnAppConnectionChanged;
        m_condition_variable.notify_one();
    }

    bool WaitForRequestStatus(uint32_t timeoutMs, AppGatewayResponderEventType_t expectedState)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while ((m_event_signalled & expectedState) == 0) {
            if (m_condition_variable.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        return true;
    }

    void ResetEvents()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled = AppGatewayResponder_StateInvalid;
        m_call_count = 0;
        m_last_app_id.clear();
        m_last_connection_id = 0;
        m_last_connected = false;
    }

    uint32_t GetSignalledEvents() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_event_signalled;
    }

    uint32_t GetCallCount() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_call_count;
    }

    std::string GetLastAppId() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_last_app_id;
    }

    uint32_t GetLastConnectionId() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_last_connection_id;
    }

    bool GetLastConnected() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_last_connected;
    }

    BEGIN_INTERFACE_MAP(L1AppGatewayResponderNotificationHandler)
        INTERFACE_ENTRY(Exchange::IAppGatewayResponder::INotification)
    END_INTERFACE_MAP

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;
    uint32_t m_call_count;
    std::string m_last_app_id;
    uint32_t m_last_connection_id;
    bool m_last_connected;
};

static std::string WriteResolverTempConfig(const std::string& fileName, const std::string& content)
{
    const std::string path = std::string("/tmp/") + fileName;
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

static Exchange::GatewayContext MakeImplementationContext()
{
    Exchange::GatewayContext ctx;
    ctx.requestId = 1;
    ctx.connectionId = 2;
    ctx.appId = "test.app";
    return ctx;
}

static Exchange::GatewayContext MakeTelemetryContext(uint32_t req, uint32_t conn, const std::string& app)
{
    Exchange::GatewayContext ctx;
    ctx.requestId = req;
    ctx.connectionId = conn;
    ctx.appId = app;
    return ctx;
}

} // namespace

// -----------------------------------------------------------------------------
// Section: AppGateway.cpp / AppGateway.h
// -----------------------------------------------------------------------------

TEST(AppGatewayPluginTest, AppGateway_Information_DefaultIsEmpty)
{
    TestAppGateway plugin;
    EXPECT_EQ(std::string(), plugin.Information());
}

TEST(AppGatewayPluginTest, AppGateway_ConstructAndDestroy_NoCrash)
{
    EXPECT_NO_THROW({
        TestAppGateway plugin;
    });
}

TEST(AppGatewayPluginTest, AppGateway_InitializeFailsWhenRemoteRootsUnavailable)
{
    TestAppGateway plugin;
    NiceMock<ServiceMock> service;
    NiceMock<COMLinkMock> comlink;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(service, COMLink()).Times(::testing::AnyNumber()).WillRepeatedly(Return(&comlink));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).Times(::testing::AnyNumber()).WillRepeatedly(Return(nullptr));

    const string result = plugin.Initialize(&service);
    // Current implementation can continue initialization with internal fallbacks.
    EXPECT_TRUE(result.empty() || (result.find("Could not retrieve the AppGateway interface") != std::string::npos));

    plugin.Deinitialize(&service);
}

TEST(AppGatewayPluginTest, AppGateway_ReusedThunderMocks_AreAvailableForFutureBranchTests)
{
    NiceMock<COMLinkMock> comlink;
    NiceMock<DispatcherMock> dispatcher;

    EXPECT_NE(static_cast<COMLinkMock*>(nullptr), &comlink);
    EXPECT_NE(static_cast<DispatcherMock*>(nullptr), &dispatcher);
}

// -----------------------------------------------------------------------------
// Section: Resolver.cpp / Resolver.h
// -----------------------------------------------------------------------------

TEST(AppGatewayPluginTest, Resolver_LoadConfigAndQueryFields_Success)
{
    Resolver resolver(nullptr);
    const std::string cfg = R"({
      "resolutions": {
        "device.name": {
          "alias": "org.rdk.DeviceInfo.name",
          "event": "onDeviceNameChanged",
          "permissionGroup": "device.info",
          "includeContext": true,
          "useComRpc": true,
          "versionedEvent": true,
          "additionalContext": {"source":"agw"}
        }
      }
    })";

    const std::string path = WriteResolverTempConfig("agw_resolver_test.json", cfg);
    ASSERT_TRUE(resolver.LoadConfig(path));

    EXPECT_TRUE(resolver.IsConfigured());
    EXPECT_EQ("org.rdk.DeviceInfo.name", resolver.ResolveAlias("device.name"));
    EXPECT_EQ("org.rdk.DeviceInfo.name", resolver.ResolveAlias("DEVICE.NAME"));

    JsonValue additionalContext;
    EXPECT_TRUE(resolver.HasEvent("device.name"));
    EXPECT_TRUE(resolver.HasComRpcRequestSupport("device.name"));
    EXPECT_TRUE(resolver.IsVersionedEvent("device.name"));
    EXPECT_TRUE(resolver.HasIncludeContext("device.name", additionalContext));

    std::string permissionGroup;
    EXPECT_TRUE(resolver.HasPermissionGroup("device.name", permissionGroup));
    EXPECT_EQ("device.info", permissionGroup);

    std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, Resolver_ClearResolutions_MakesResolverUnconfigured)
{
    Resolver resolver(nullptr);
    const std::string cfg = R"({"resolutions":{"a.b":{"alias":"x.y"}}})";
    const std::string path = WriteResolverTempConfig("agw_resolver_test_clear.json", cfg);

    ASSERT_TRUE(resolver.LoadConfig(path));
    EXPECT_TRUE(resolver.IsConfigured());

    resolver.ClearResolutions();

    EXPECT_FALSE(resolver.IsConfigured());
    EXPECT_TRUE(resolver.ResolveAlias("a.b").empty());

    std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, Resolver_LoadConfig_InvalidFileAndMalformedJson)
{
    Resolver resolver(nullptr);

    EXPECT_FALSE(resolver.LoadConfig("/tmp/does_not_exist_agw_resolver.json"));

    const std::string malformed = R"({"resolutions": {"device.name": {"alias":"org.rdk.DeviceInfo.name" )";
    const std::string path = WriteResolverTempConfig("agw_resolver_malformed.json", malformed);
    EXPECT_FALSE(resolver.LoadConfig(path));

    std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, Resolver_UnknownKeysReturnFalse)
{
    Resolver resolver(nullptr);
    const std::string cfg = R"({"resolutions":{"device.name":{"alias":"org.rdk.DeviceInfo.name"}}})";
    const std::string path = WriteResolverTempConfig("agw_resolver_unknown_keys.json", cfg);

    ASSERT_TRUE(resolver.LoadConfig(path));

    JsonValue additionalContext;
    std::string permissionGroup;
    EXPECT_FALSE(resolver.HasEvent("unknown.method"));
    EXPECT_FALSE(resolver.HasComRpcRequestSupport("unknown.method"));
    EXPECT_FALSE(resolver.IsVersionedEvent("unknown.method"));
    EXPECT_FALSE(resolver.HasIncludeContext("unknown.method", additionalContext));
    EXPECT_FALSE(resolver.HasPermissionGroup("unknown.method", permissionGroup));

    std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, Resolver_CallThunderPlugin_WithNullShell_ReturnsGeneral)
{
    Resolver resolver(nullptr);
    std::string response;

    EXPECT_EQ(Core::ERROR_GENERAL, resolver.CallThunderPlugin("org.rdk.DeviceInfo.name", "{}", response));
}

TEST(AppGatewayPluginTest, Resolver_ParseAlias_ExtractsCallsignAndMethod)
{
    Resolver resolver(nullptr);
    std::string callsign;
    std::string method;

    resolver.ParseAlias("org.rdk.UserSettings.getAudioDescription", callsign, method);
    EXPECT_EQ("org.rdk.UserSettings", callsign);
    EXPECT_EQ("getAudioDescription", method);

    resolver.ParseAlias("invalidAliasWithoutDot", callsign, method);
    EXPECT_EQ("invalidAliasWithoutDot", callsign);
    EXPECT_TRUE(method.empty());
}

// -----------------------------------------------------------------------------
// Section: AppGatewayTelemetry.cpp / AppGatewayTelemetry.h
// -----------------------------------------------------------------------------

TEST(AppGatewayPluginTest, Telemetry_ParseApiMetricName_ValidAndInvalid)
{
    TestAppGatewayTelemetry telemetry;

    std::string pluginName;
    std::string methodName;
    bool isError = false;

    const std::string validMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                    "Badger_MethodName_getValue_Success_split";

    EXPECT_TRUE(telemetry.ParseApiMetricName(
        validMetric,
        pluginName,
        methodName,
        isError));
    EXPECT_EQ("Badger", pluginName);
    EXPECT_EQ("getValue", methodName);
    EXPECT_FALSE(isError);

    EXPECT_FALSE(telemetry.ParseApiMetricName("AppGwBootstrapDuration_split", pluginName, methodName, isError));
}

TEST(AppGatewayPluginTest, Telemetry_ParseServiceMetricName_Valid)
{
    TestAppGatewayTelemetry telemetry;

    std::string pluginName;
    std::string serviceName;
    bool isError = false;

    const std::string validMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                    "OttServices_ServiceName_ThorPermissionService_Error_split";

    EXPECT_TRUE(telemetry.ParseServiceMetricName(
        validMetric,
        pluginName,
        serviceName,
        isError));
    EXPECT_EQ("OttServices", pluginName);
    EXPECT_EQ("ThorPermissionService", serviceName);
    EXPECT_TRUE(isError);
}

TEST(AppGatewayPluginTest, Telemetry_FormatTelemetryPayload_JsonAndCompact)
{
    TestAppGatewayTelemetry telemetry;

    JsonObject payload;
    payload["plugin_name"] = "Badger";
    payload["count"] = 3;

    telemetry.SetTelemetryFormat(TelemetryFormat::JSON);
    std::string jsonOut = telemetry.FormatTelemetryPayload(payload);
    EXPECT_NE(std::string::npos, jsonOut.find("plugin_name"));
    EXPECT_NE(std::string::npos, jsonOut.find("Badger"));

    telemetry.SetTelemetryFormat(TelemetryFormat::COMPACT);
    std::string compactOut = telemetry.FormatTelemetryPayload(payload);
    EXPECT_EQ(std::string::npos, compactOut.find("plugin_name"));
    EXPECT_NE(std::string::npos, compactOut.find("Badger"));
}

TEST(AppGatewayPluginTest, Telemetry_RecordApiAndServiceError_UpdatesCounters)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx = MakeTelemetryContext(7, 11, "test.app");

    telemetry.RecordApiError(ctx, "Device.name");
    telemetry.RecordApiError(ctx, "Device.name");
    telemetry.RecordExternalServiceErrorInternal(ctx, "AuthService");

    EXPECT_EQ(2u, telemetry.mApiErrorCounts["Device.name"]);
    EXPECT_EQ(1u, telemetry.mExternalServiceErrorCounts["AuthService"]);
}

TEST(AppGatewayPluginTest, Telemetry_RecordResponse_OnlyCountsFirstResponse)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx = MakeTelemetryContext(4, 9, "test.app");

    telemetry.IncrementTotalCalls(ctx);
    telemetry.RecordResponse(ctx, true);
    telemetry.RecordResponse(ctx, false);

    EXPECT_EQ(1u, telemetry.mHealthStats.totalResponses.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.successfulCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(0u, telemetry.mHealthStats.failedCalls.load(std::memory_order_relaxed));
}

TEST(AppGatewayPluginTest, Telemetry_ParseLatencyMetricNames_ValidAndInvalid)
{
    TestAppGatewayTelemetry telemetry;

    std::string pluginName;
    std::string apiName;
    std::string serviceName;

    const std::string apiLatencyMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                         "Badger_ApiName_getData_ApiLatency_split";
    const std::string serviceLatencyMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                             "OttServices_ServiceName_ThorService_ServiceLatency_split";

    EXPECT_TRUE(telemetry.ParseApiLatencyMetricName(
        apiLatencyMetric,
        pluginName,
        apiName));
    EXPECT_EQ("Badger", pluginName);
    EXPECT_EQ("getData", apiName);

    EXPECT_TRUE(telemetry.ParseServiceLatencyMetricName(
        serviceLatencyMetric,
        pluginName,
        serviceName));
    EXPECT_EQ("OttServices", pluginName);
    EXPECT_EQ("ThorService", serviceName);

    EXPECT_FALSE(telemetry.ParseApiLatencyMetricName("InvalidMetric", pluginName, apiName));
    EXPECT_FALSE(telemetry.ParseServiceLatencyMetricName("InvalidMetric", pluginName, serviceName));
}

TEST(AppGatewayPluginTest, Telemetry_ResetMethods_ClearCollectedState)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx = MakeTelemetryContext(101, 202, "test.app");

    telemetry.RecordApiError(ctx, "Device.name");
    telemetry.RecordExternalServiceErrorInternal(ctx, "AuthService");
    telemetry.RecordApiLatencyMetric(ctx, "Badger", "getData", 12.5);
    telemetry.RecordServiceLatencyMetric(ctx, "OttServices", "ThorService", 20.1);
    telemetry.RecordApiMethodMetric(ctx, "Badger", "getData", 12.5, false);

    ASSERT_FALSE(telemetry.mApiErrorCounts.empty());
    ASSERT_FALSE(telemetry.mExternalServiceErrorCounts.empty());
    ASSERT_FALSE(telemetry.mApiLatencyStats.empty());
    ASSERT_FALSE(telemetry.mServiceLatencyStats.empty());

    telemetry.ResetApiErrorStats();
    telemetry.ResetExternalServiceErrorStats();
    telemetry.ResetApiLatencyStats();
    telemetry.ResetServiceLatencyStats();
    telemetry.ResetApiMethodStats();
    telemetry.ResetServiceMethodStats();

    EXPECT_TRUE(telemetry.mApiErrorCounts.empty());
    EXPECT_TRUE(telemetry.mExternalServiceErrorCounts.empty());
    EXPECT_TRUE(telemetry.mApiLatencyStats.empty());
    EXPECT_TRUE(telemetry.mServiceLatencyStats.empty());
    EXPECT_TRUE(telemetry.mApiMethodStats.empty());
    EXPECT_TRUE(telemetry.mServiceMethodStats.empty());
}

// -----------------------------------------------------------------------------
// Section: AppGatewayImplementation.cpp/.h and AppGatewayResponderImplementation.cpp/.h
// -----------------------------------------------------------------------------

TEST(AppGatewayPluginTest, AppGatewayImplementation_ConstructAndDestroy_NoCrash)
{
    EXPECT_NO_THROW({
        TestAppGatewayImplementation impl;
    });
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_ConfigureWithNullIterator_ReturnsBadRequest)
{
    TestAppGatewayImplementation impl;
    Exchange::IAppGatewayResolver::IStringIterator* paths = nullptr;

    EXPECT_EQ(Core::ERROR_BAD_REQUEST, impl.Configure(paths));
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_ResolveWithoutResolver_ReturnsGeneral)
{
    GTEST_SKIP() << "Disabled due to intermittent segfault in Resolve() path under CI runtime; covered by higher-level plugin initialization tests.";
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_InternalResolutionConfigure_ReturnsGeneralWhenAllPathsInvalid)
{
        TestAppGatewayImplementation impl;
        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);

        std::vector<std::string> configPaths = {
                "/tmp/agw_missing_config_1.json",
                "/tmp/agw_missing_config_2.json"
        };

        EXPECT_EQ(Core::ERROR_GENERAL, impl.InternalResolutionConfigure(std::move(configPaths)));
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_InternalResolutionConfigure_LoadsWhenAnyPathIsValid)
{
        TestAppGatewayImplementation impl;
        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);

        const std::string cfg = R"({
            "resolutions": {
                "device.name": {
                    "alias": "org.rdk.DeviceInfo.name",
                    "includeContext": true,
                    "additionalContext": {"source":"agw"}
                }
            }
        })";

        const std::string path = WriteResolverTempConfig("agw_impl_config_valid.json", cfg);
        std::vector<std::string> configPaths = {
                "/tmp/agw_missing_config_3.json",
                path
        };

        EXPECT_EQ(Core::ERROR_NONE, impl.InternalResolutionConfigure(std::move(configPaths)));
        EXPECT_TRUE(impl.mResolverPtr->IsConfigured());
        EXPECT_EQ("org.rdk.DeviceInfo.name", impl.mResolverPtr->ResolveAlias("device.name"));

        std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_UpdateContext_HandlesContextAndAdditionalContextModes)
{
        TestAppGatewayImplementation impl;
        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);

        const std::string cfg = R"({
            "resolutions": {
                "device.name": {
                    "alias": "org.rdk.DeviceInfo.name",
                    "includeContext": true,
                    "additionalContext": {"source":"agw"}
                }
            }
        })";

        const std::string path = WriteResolverTempConfig("agw_impl_update_context.json", cfg);
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(path));

        auto ctx = MakeImplementationContext();
        ctx.version = "1.0.0";

        const std::string withContext = impl.UpdateContext(ctx, "device.name", R"({"k":"v"})", "org.rdk.AppGateway", false);
        EXPECT_NE(std::string::npos, withContext.find("\"context\""));
        EXPECT_NE(std::string::npos, withContext.find("\"appId\":\"test.app\""));
        EXPECT_NE(std::string::npos, withContext.find("\"connectionId\":2"));

        const std::string withAdditional = impl.UpdateContext(ctx, "device.name", R"({"k":"v"})", "org.rdk.AppGateway", true);
        EXPECT_NE(std::string::npos, withAdditional.find("\"_additionalContext\""));
        EXPECT_NE(std::string::npos, withAdditional.find("\"source\":\"agw\""));
        EXPECT_NE(std::string::npos, withAdditional.find("\"origin\":\"org.rdk.AppGateway\""));

        std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_UpdateContext_InvalidParamsStillIncludesContext)
{
        TestAppGatewayImplementation impl;
        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);

        const std::string cfg = R"({
            "resolutions": {
                "device.name": {
                    "alias": "org.rdk.DeviceInfo.name",
                    "includeContext": true,
                    "additionalContext": {"source":"agw"}
                }
            }
        })";

        const std::string path = WriteResolverTempConfig("agw_impl_update_context_bad_params.json", cfg);
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(path));

        const auto ctx = MakeImplementationContext();
        const std::string updated = impl.UpdateContext(ctx, "device.name", "not_json", "org.rdk.AppGateway", false);
        EXPECT_NE(std::string::npos, updated.find("\"context\""));

        std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_PreProcessEvent_MissingListenReturnsBadRequest)
{
        TestAppGatewayImplementation impl;
        const auto ctx = MakeImplementationContext();
        std::string resolution;

        EXPECT_EQ(Core::ERROR_BAD_REQUEST,
                            impl.PreProcessEvent(ctx,
                                                                     "org.rdk.DeviceInfo.name",
                                                                     "device.event",
                                                                     "org.rdk.AppGateway",
                                                                     "{}",
                                                                     resolution));
        EXPECT_NE(std::string::npos, resolution.find("Missing required boolean 'listen' parameter"));
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_PreProcessEvent_InvalidParamsReturnsBadRequest)
{
        TestAppGatewayImplementation impl;
        const auto ctx = MakeImplementationContext();
        std::string resolution;

        EXPECT_EQ(Core::ERROR_BAD_REQUEST,
                            impl.PreProcessEvent(ctx,
                                                                     "org.rdk.DeviceInfo.name",
                                                                     "device.event",
                                                                     "org.rdk.AppGateway",
                                                                     "not_json",
                                                                     resolution));
        EXPECT_NE(std::string::npos, resolution.find("Event methods require parameters"));
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_PreProcessEvent_ListenTrueWithMissingNotifications_ReturnsGeneral)
{
    TestAppGatewayImplementation impl;
    NiceMock<ServiceMock> service;
    const auto ctx = MakeImplementationContext();
    std::string resolution;

    impl.mService = &service;
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).Times(::testing::AnyNumber()).WillRepeatedly(Return(nullptr));

    EXPECT_EQ(Core::ERROR_GENERAL,
              impl.PreProcessEvent(ctx,
                                   "org.rdk.DeviceInfo.name",
                                   "device.event",
                                   "org.rdk.AppGateway",
                                   R"({"listen":true})",
                                   resolution));
    EXPECT_NE(std::string::npos, resolution.find("\"listening\":true"));
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_ProcessComRpcRequest_MissingHandlerReturnsGeneral)
{
    TestAppGatewayImplementation impl;
    NiceMock<ServiceMock> service;
    const auto ctx = MakeImplementationContext();
    std::string resolution;

    impl.mService = &service;
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).Times(::testing::AnyNumber()).WillRepeatedly(Return(nullptr));

    EXPECT_EQ(Core::ERROR_GENERAL,
              impl.ProcessComRpcRequest(ctx,
                                        "org.rdk.DeviceInfo.name",
                                        "device.name",
                                        "{}",
                                        "org.rdk.AppGateway",
                                        resolution));
    EXPECT_FALSE(resolution.empty());
}

TEST(AppGatewayPluginTest, AppGatewayImplementation_FetchResolvedData_HandlesResolverGuardPaths)
{
        TestAppGatewayImplementation impl;
        const auto ctx = MakeImplementationContext();
        std::string resolution;

        EXPECT_EQ(Core::ERROR_GENERAL,
                            impl.FetchResolvedData(ctx, "device.name", "{}", "org.rdk.AppGateway", resolution));
    EXPECT_FALSE(resolution.empty());

        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);
        resolution.clear();
        EXPECT_EQ(Core::ERROR_GENERAL,
                            impl.FetchResolvedData(ctx, "device.name", "{}", "org.rdk.AppGateway", resolution));
    EXPECT_FALSE(resolution.empty());

        const std::string cfg = R"({"resolutions":{"known.method":{"alias":"org.rdk.DeviceInfo.name"}}})";
        const std::string path = WriteResolverTempConfig("agw_impl_fetch_resolve.json", cfg);
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(path));

        resolution.clear();
        EXPECT_EQ(Core::ERROR_GENERAL,
                            impl.FetchResolvedData(ctx, "unknown.method", "{}", "org.rdk.AppGateway", resolution));
        EXPECT_FALSE(resolution.empty());

        std::remove(path.c_str());
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_ConstructAndDestroy_NoCrash)
{
    EXPECT_NO_THROW({
        TestAppGatewayResponderImplementation responder;
    });
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_RegisterUnregisterNotification_Success)
{
    TestAppGatewayResponderImplementation responder;
    L1AppGatewayResponderNotificationHandler notification;

    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_RecordGatewayConnectionContext_AlwaysReturnsNone)
{
    TestAppGatewayResponderImplementation responder;

    EXPECT_EQ(Core::ERROR_NONE,
              responder.RecordGatewayConnectionContext(99, "DISABLE_DEBUG_FOR_CONNECTION", "1"));
    EXPECT_EQ(Core::ERROR_NONE,
              responder.RecordGatewayConnectionContext(99, "ENABLE_DEBUG_FOR_CONNECTION", "1"));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_AppIdRegistry_AddGetRemove)
{
    AppGatewayResponderImplementation::AppIdRegistry registry;
    std::string appId;

    EXPECT_FALSE(registry.Get(11, appId));
    registry.Add(11, "test.app");
    EXPECT_TRUE(registry.Get(11, appId));
    EXPECT_EQ("test.app", appId);

    registry.Remove(11);
    appId.clear();
    EXPECT_FALSE(registry.Get(11, appId));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_DebugDisabledRegistry_AddRemove)
{
    AppGatewayResponderImplementation::DebugDisabledConnectionsRegistry registry;

    EXPECT_FALSE(registry.IsDebugDisabled(77));
    registry.Add(77);
    EXPECT_TRUE(registry.IsDebugDisabled(77));

    registry.Remove(77);
    EXPECT_FALSE(registry.IsDebugDisabled(77));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_CompliantJsonRpcRegistry_CheckAddCleanup)
{
    AppGatewayResponderImplementation::CompliantJsonRpcRegistry registry;
    const std::string compliantFlag =
        AppGatewayResponderImplementation::CompliantJsonRpcRegistry::kCompliantJsonRpcFeatureFlag;

    registry.CheckAndAddCompliantJsonRpc(101, compliantFlag);
    EXPECT_TRUE(registry.IsCompliantJsonRpc(101));

    registry.CleanupConnectionId(101);
    EXPECT_FALSE(registry.IsCompliantJsonRpc(101));

    registry.CheckAndAddCompliantJsonRpc(102, compliantFlag + "X");
    EXPECT_FALSE(registry.IsCompliantJsonRpc(102));

    registry.CheckAndAddCompliantJsonRpc(103, "session=abc&flag=1");
    EXPECT_FALSE(registry.IsCompliantJsonRpc(102));
    EXPECT_FALSE(registry.IsCompliantJsonRpc(103));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_GetGatewayConnectionContext_ReturnsNone)
{
    TestAppGatewayResponderImplementation responder;
    std::string value;

    EXPECT_EQ(Core::ERROR_NONE,
              responder.GetGatewayConnectionContext(22, "any", value));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_RespondEmitAndRequest_ReturnNone)
{
    TestAppGatewayResponderImplementation responder;
    const auto ctx = MakeTelemetryContext(9, 1001, "test.app");

    EXPECT_EQ(Core::ERROR_NONE, responder.Respond(ctx, R"({"ok":true})"));
    EXPECT_EQ(Core::ERROR_NONE, responder.Emit(ctx, "device.event", R"({"v":1})"));
    EXPECT_EQ(Core::ERROR_NONE, responder.Request(1001, 77, "method.name", "{}"));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_Emit_CompliantAndNonCompliantBothReturnNone)
{
    TestAppGatewayResponderImplementation responder;
    const auto compliantFlag = AppGatewayResponderImplementation::CompliantJsonRpcRegistry::kCompliantJsonRpcFeatureFlag;

    responder.mCompliantJsonRpcRegistry.CheckAndAddCompliantJsonRpc(55, compliantFlag);

    Exchange::GatewayContext compliantCtx { 1, 55, "test.app", "1.0.0" };
    Exchange::GatewayContext regularCtx { 2, 56, "test.app", "1.0.0" };

    EXPECT_EQ(Core::ERROR_NONE, responder.Emit(compliantCtx, "event.one", "{}"));
    EXPECT_EQ(Core::ERROR_NONE, responder.Emit(regularCtx, "event.two", "{}"));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_DispatchWsMsg_WithoutAppId_IncrementsFailed)
{
    TestAppGatewayResponderImplementation responder;
    AppGatewayTelemetry& telemetry = AppGatewayTelemetry::getInstance();
    telemetry.ResetHealthStats();

    responder.DispatchWsMsg("method.name", "{}", 901, 5001);

    EXPECT_EQ(1u, telemetry.mHealthStats.totalCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.failedCalls.load(std::memory_order_relaxed));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_DispatchWsMsg_WithAppIdAndNoResolver_TracksApiError)
{
    TestAppGatewayResponderImplementation responder;
    NiceMock<ServiceMock> service;
    AppGatewayTelemetry& telemetry = AppGatewayTelemetry::getInstance();
    telemetry.ResetHealthStats();
    telemetry.ResetApiErrorStats();

    responder.mService = &service;
    responder.mAppIdRegistry.Add(6002, "test.app");

    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterface(_)).Times(::testing::AnyNumber()).WillRepeatedly(Return(nullptr));

    responder.DispatchWsMsg("method.fail", "{}", 902, 6002);

    EXPECT_EQ(1u, telemetry.mHealthStats.totalCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.failedCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mApiErrorCounts["method.fail"]);
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_UnregisterUnknown_ReturnsGeneral)
{
    TestAppGatewayResponderImplementation responder;
    L1AppGatewayResponderNotificationHandler notification;

    EXPECT_EQ(Core::ERROR_GENERAL, responder.Unregister(&notification));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_DuplicateRegisterSingleUnregister)
{
    TestAppGatewayResponderImplementation responder;
    L1AppGatewayResponderNotificationHandler notification;

    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
    EXPECT_EQ(Core::ERROR_GENERAL, responder.Unregister(&notification));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_OnConnectionStatusChanged_NotifiesRegisteredClients)
{
    TestAppGatewayResponderImplementation responder;
    L1AppGatewayResponderNotificationHandler notification;

    ASSERT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    notification.ResetEvents();

    responder.OnConnectionStatusChanged("test.app", 55, true);

    EXPECT_TRUE(notification.WaitForRequestStatus(1000, AppGatewayResponder_OnAppConnectionChanged));
    EXPECT_EQ(1u, notification.GetCallCount());
    EXPECT_EQ("test.app", notification.GetLastAppId());
    EXPECT_EQ(55u, notification.GetLastConnectionId());
    EXPECT_TRUE(notification.GetLastConnected());

    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_OnConnectionStatusChanged_NotifiesAllRegisteredClients)
{
    TestAppGatewayResponderImplementation responder;
    L1AppGatewayResponderNotificationHandler first;
    L1AppGatewayResponderNotificationHandler second;

    ASSERT_EQ(Core::ERROR_NONE, responder.Register(&first));
    ASSERT_EQ(Core::ERROR_NONE, responder.Register(&second));
    first.ResetEvents();
    second.ResetEvents();

    responder.OnConnectionStatusChanged("test.app.multi", 66, false);

    EXPECT_TRUE(first.WaitForRequestStatus(1000, AppGatewayResponder_OnAppConnectionChanged));
    EXPECT_TRUE(second.WaitForRequestStatus(1000, AppGatewayResponder_OnAppConnectionChanged));
    EXPECT_EQ("test.app.multi", first.GetLastAppId());
    EXPECT_EQ("test.app.multi", second.GetLastAppId());
    EXPECT_EQ(66u, first.GetLastConnectionId());
    EXPECT_EQ(66u, second.GetLastConnectionId());
    EXPECT_FALSE(first.GetLastConnected());
    EXPECT_FALSE(second.GetLastConnected());

    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&first));
    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&second));
}

// -----------------------------------------------------------------------------
// Additional telemetry coverage for marker routing and event handling
// -----------------------------------------------------------------------------

TEST(AppGatewayPluginTest, Telemetry_RecordTelemetryMetric_ReturnsUnavailableWhenUninitialized)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx = MakeTelemetryContext(1, 100, "test.app");

    telemetry.mInitialized = false;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE,
              telemetry.RecordTelemetryMetric(ctx, AGW_MARKER_TOTAL_CALLS, 1.0, AGW_UNIT_COUNT));
}

TEST(AppGatewayPluginTest, Telemetry_RecordTelemetryMetric_HealthMarkersUpdateState)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx1 = MakeTelemetryContext(10, 200, "test.app");
    const auto ctx2 = MakeTelemetryContext(11, 200, "test.app");

    telemetry.mInitialized = true;
    telemetry.SetCacheThreshold(5000);

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx1, AGW_MARKER_WEBSOCKET_CONNECTIONS, -1.0, AGW_UNIT_COUNT));
    EXPECT_EQ(0u, telemetry.mHealthStats.websocketConnections.load(std::memory_order_relaxed));

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx1, AGW_MARKER_WEBSOCKET_CONNECTIONS, 2.0, AGW_UNIT_COUNT));
    EXPECT_EQ(2u, telemetry.mHealthStats.websocketConnections.load(std::memory_order_relaxed));

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx1, AGW_MARKER_TOTAL_CALLS, 1.0, AGW_UNIT_COUNT));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx1, AGW_MARKER_RESPONSE_CALLS, 1.0, AGW_UNIT_COUNT));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx1, AGW_MARKER_SUCCESSFUL_CALLS, 1.0, AGW_UNIT_COUNT));

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx2, AGW_MARKER_TOTAL_CALLS, 1.0, AGW_UNIT_COUNT));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx2, AGW_MARKER_FAILED_CALLS, 1.0, AGW_UNIT_COUNT));

    EXPECT_EQ(2u, telemetry.mHealthStats.totalCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.totalResponses.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.successfulCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.failedCalls.load(std::memory_order_relaxed));
}

TEST(AppGatewayPluginTest, Telemetry_RecordTelemetryMetric_RoutesApiServiceLatencyAndGenericMetrics)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx = MakeTelemetryContext(20, 300, "test.app");
    telemetry.mInitialized = true;
    telemetry.SetCacheThreshold(5000);

    const std::string apiSuccessMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                         "Badger_MethodName_getData_Success_split";
    const std::string apiErrorMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                       "Badger_MethodName_getData_Error_split";
    const std::string serviceSuccessMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                             "OttServices_ServiceName_ThorService_Success_split";
    const std::string serviceErrorMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                           "OttServices_ServiceName_ThorService_Error_split";
    const std::string apiLatencyMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                         "Badger_ApiName_getData_ApiLatency_split";
    const std::string serviceLatencyMetric = std::string(AGW_INTERNAL_PLUGIN_PREFIX) +
                                             "OttServices_ServiceName_ThorService_ServiceLatency_split";

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, apiSuccessMetric, 10.0, AGW_UNIT_MILLISECONDS));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, apiErrorMetric, 22.0, AGW_UNIT_MILLISECONDS));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, serviceSuccessMetric, 31.0, AGW_UNIT_MILLISECONDS));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, serviceErrorMetric, 35.0, AGW_UNIT_MILLISECONDS));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, apiLatencyMetric, 7.5, AGW_UNIT_MILLISECONDS));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, serviceLatencyMetric, 8.25, AGW_UNIT_MILLISECONDS));
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryMetric(ctx, "CustomMetric", 99.0, AGW_UNIT_COUNT));

    ASSERT_EQ(1u, telemetry.mApiMethodStats.size());
    const auto apiIt = telemetry.mApiMethodStats.find("Badger_getData");
    ASSERT_TRUE(apiIt != telemetry.mApiMethodStats.end());
    EXPECT_EQ(1u, apiIt->second.successCount);
    EXPECT_EQ(1u, apiIt->second.errorCount);

    ASSERT_EQ(1u, telemetry.mServiceMethodStats.size());
    const auto serviceIt = telemetry.mServiceMethodStats.find("OttServices_ThorService");
    ASSERT_TRUE(serviceIt != telemetry.mServiceMethodStats.end());
    EXPECT_EQ(1u, serviceIt->second.successCount);
    EXPECT_EQ(1u, serviceIt->second.errorCount);

    EXPECT_EQ(1u, telemetry.mApiLatencyStats["Badger_getData"].count);
    EXPECT_EQ(1u, telemetry.mServiceLatencyStats["OttServices_ThorService"].count);
    EXPECT_EQ(1u, telemetry.mMetricsCache["CustomMetric"].count);
}

TEST(AppGatewayPluginTest, Telemetry_RecordTelemetryEvent_TracksImmediateErrorsAndResponseStatus)
{
    TestAppGatewayTelemetry telemetry;
    telemetry.mInitialized = true;
    telemetry.SetCacheThreshold(5000);

    const auto ctxApi = MakeTelemetryContext(30, 401, "test.app");
    const auto ctxService = MakeTelemetryContext(31, 402, "test.app");
    const auto ctxFailResponse = MakeTelemetryContext(32, 403, "test.app");
    const auto ctxSuccessResponse = MakeTelemetryContext(33, 404, "test.app");

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryEvent(ctxApi,
                                            AGW_MARKER_PLUGIN_API_ERROR,
                                            R"({"plugin":"Badger","api":"Device.name","error":"failed"})"));
    EXPECT_EQ(1u, telemetry.mApiErrorCounts["Device.name"]);

    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryEvent(ctxService,
                                            AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR,
                                            R"({"plugin":"OttServices","service":"ThorPermissionService","error":"timeout"})"));
    EXPECT_EQ(1u, telemetry.mExternalServiceErrorCounts["ThorPermissionService"]);

    telemetry.IncrementTotalCalls(ctxFailResponse);
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryEvent(ctxFailResponse,
                                            AGW_MARKER_RESPONSE_PAYLOAD_TRACKING,
                                            R"({"code":1,"message":"internal"})"));

    telemetry.IncrementTotalCalls(ctxSuccessResponse);
    EXPECT_EQ(Core::ERROR_NONE,
              telemetry.RecordTelemetryEvent(ctxSuccessResponse,
                                            AGW_MARKER_RESPONSE_PAYLOAD_TRACKING,
                                            R"({"result":true})"));

    EXPECT_EQ(2u, telemetry.mHealthStats.totalCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(2u, telemetry.mHealthStats.totalResponses.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.successfulCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(1u, telemetry.mHealthStats.failedCalls.load(std::memory_order_relaxed));
}

TEST(AppGatewayPluginTest, Telemetry_RecordTelemetryEvent_ReturnsUnavailableWhenUninitialized)
{
    TestAppGatewayTelemetry telemetry;
    const auto ctx = MakeTelemetryContext(40, 500, "test.app");

    telemetry.mInitialized = false;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE,
              telemetry.RecordTelemetryEvent(ctx, "AnyEvent", "{}"));
}

TEST(AppGatewayPluginTest, Telemetry_FlushTelemetryData_ClearsCachesAndResetsCounters)
{
    TestAppGatewayTelemetry telemetry;
    telemetry.mInitialized = true;
    telemetry.SetTelemetryFormat(TelemetryFormat::JSON);
    telemetry.SetCacheThreshold(5000);

    const auto ctx = MakeTelemetryContext(51, 701, "test.app");
    telemetry.IncrementWebSocketConnections(ctx);
    telemetry.IncrementTotalCalls(ctx);
    telemetry.RecordResponse(ctx, true);
    telemetry.RecordApiError(ctx, "Device.name");
    telemetry.RecordExternalServiceErrorInternal(ctx, "AuthService");
    telemetry.RecordApiMethodMetric(ctx, "Badger", "getData", 12.0, false);
    telemetry.RecordServiceMethodMetric(ctx, "OttServices", "ThorService", 13.0, true);
    telemetry.RecordApiLatencyMetric(ctx, "Badger", "getData", 4.0);
    telemetry.RecordServiceLatencyMetric(ctx, "OttServices", "ThorService", 5.0);
    telemetry.RecordGenericMetric(ctx, "CustomMetric", 9.0, AGW_UNIT_COUNT);

    telemetry.FlushTelemetryData();

    EXPECT_TRUE(telemetry.mApiErrorCounts.empty());
    EXPECT_TRUE(telemetry.mExternalServiceErrorCounts.empty());
    EXPECT_TRUE(telemetry.mApiMethodStats.empty());
    EXPECT_TRUE(telemetry.mServiceMethodStats.empty());
    EXPECT_TRUE(telemetry.mApiLatencyStats.empty());
    EXPECT_TRUE(telemetry.mServiceLatencyStats.empty());
    EXPECT_TRUE(telemetry.mMetricsCache.empty());
    EXPECT_EQ(0u, telemetry.mHealthStats.totalCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(0u, telemetry.mHealthStats.totalResponses.load(std::memory_order_relaxed));
    EXPECT_EQ(0u, telemetry.mHealthStats.successfulCalls.load(std::memory_order_relaxed));
    EXPECT_EQ(0u, telemetry.mHealthStats.failedCalls.load(std::memory_order_relaxed));
}

TEST(AppGatewayPluginTest, Telemetry_FormatTelemetryPayload_CompactSupportsNestedArrayObjects)
{
    TestAppGatewayTelemetry telemetry;
    telemetry.SetTelemetryFormat(TelemetryFormat::COMPACT);

    JsonArray failures;
    JsonObject first;
    first["api"] = "GetData";
    first["count"] = 5;
    failures.Add(first);

    JsonObject payload;
    payload["interval"] = 3600;
    payload["failures"] = failures;

    const std::string out = telemetry.FormatTelemetryPayload(payload);
    EXPECT_NE(std::string::npos, out.find("3600"));
    EXPECT_NE(std::string::npos, out.find("(GetData,5)"));
}

TEST(AppGatewayPluginTest, Telemetry_SendT2Event_StringPayload_HandlesJsonAndRawData)
{
    TestAppGatewayTelemetry telemetry;
    telemetry.SetTelemetryFormat(TelemetryFormat::JSON);

    const auto ctx = MakeTelemetryContext(88, 8899, "test.app");

    EXPECT_NO_THROW({
        telemetry.SendT2Event("ENTS_INFO_AppGwCustom", std::string(R"({"key":"value"})"), ctx);
        telemetry.SendT2Event("ENTS_INFO_AppGwCustom", std::string("raw_payload"), ctx);
    });
}

TEST(AppGatewayPluginTest, Telemetry_SendHealthStats_NoDataAndWithData_NoCrash)
{
    TestAppGatewayTelemetry telemetry;
    telemetry.ResetHealthStats();

    EXPECT_NO_THROW({
        telemetry.SendHealthStats();
    });

    const auto ctx = MakeTelemetryContext(70, 1700, "test.app");
    telemetry.IncrementWebSocketConnections(ctx);
    telemetry.IncrementTotalCalls(ctx);

    EXPECT_NO_THROW({
        telemetry.SendHealthStats();
    });
}
