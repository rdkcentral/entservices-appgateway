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

class ResponderNotificationStub final : public Exchange::IAppGatewayResponder::INotification {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }
    void OnAppConnectionChanged(const string&, const uint32_t, const bool) override {}

    BEGIN_INTERFACE_MAP(ResponderNotificationStub)
        INTERFACE_ENTRY(Exchange::IAppGatewayResponder::INotification)
    END_INTERFACE_MAP
};

class CapturingResponderNotification final : public Exchange::IAppGatewayResponder::INotification {
public:
    enum EventState : uint32_t {
        Responder_StateInvalid = 0x00000000,
        Responder_OnAppConnectionChanged = 0x00000001
    };

    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }

    void OnAppConnectionChanged(const string& appId, const uint32_t connectionId, const bool connected) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        ++callCount;
        lastAppId = appId;
        lastConnectionId = connectionId;
        lastConnected = connected;
        eventSignalled |= Responder_OnAppConnectionChanged;
        conditionVariable.notify_one();
    }

    BEGIN_INTERFACE_MAP(CapturingResponderNotification)
        INTERFACE_ENTRY(Exchange::IAppGatewayResponder::INotification)
    END_INTERFACE_MAP

    uint32_t callCount {0};
    std::string lastAppId;
    uint32_t lastConnectionId {0};
    bool lastConnected {false};

    bool WaitForRequestStatus(uint32_t timeoutMs, EventState expectedState)
    {
        std::unique_lock<std::mutex> lock(mutex);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while ((eventSignalled & expectedState) == 0) {
            if (conditionVariable.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        return true;
    }

    void ResetEvents()
    {
        std::unique_lock<std::mutex> lock(mutex);
        eventSignalled = Responder_StateInvalid;
        callCount = 0;
        lastAppId.clear();
        lastConnectionId = 0;
        lastConnected = false;
    }

private:
    std::mutex mutex;
    std::condition_variable conditionVariable;
    uint32_t eventSignalled {Responder_StateInvalid};
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

    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(1).WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(service, COMLink()).Times(::testing::AnyNumber()).WillRepeatedly(Return(&comlink));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).Times(::testing::AnyNumber()).WillRepeatedly(Return(nullptr));

    const string result = plugin.Initialize(&service);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(std::string::npos, result.find("Could not retrieve the AppGateway interface"));

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

    EXPECT_TRUE(telemetry.ParseApiMetricName(
        "AppGw_PluginName_Badger_MethodName_getValue_Success_split",
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

    EXPECT_TRUE(telemetry.ParseServiceMetricName(
        "AppGw_PluginName_OttServices_ServiceName_ThorPermissionService_Error_split",
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

    EXPECT_TRUE(telemetry.ParseApiLatencyMetricName(
        "AppGw_PluginName_Badger_ApiName_getData_LatencyMs_split",
        pluginName,
        apiName));
    EXPECT_EQ("Badger", pluginName);
    EXPECT_EQ("getData", apiName);

    EXPECT_TRUE(telemetry.ParseServiceLatencyMetricName(
        "AppGw_PluginName_OttServices_ServiceName_ThorService_LatencyMs_split",
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
    telemetry.RecordTelemetryMetric(ctx, "AppGw_PluginName_Badger_ApiName_getData_LatencyMs_split", 12.5, "ms");
    telemetry.RecordTelemetryMetric(ctx, "AppGw_PluginName_OttServices_ServiceName_ThorService_LatencyMs_split", 20.1, "ms");
    telemetry.RecordTelemetryMetric(ctx, "AppGw_CustomMetric", 5.0, "count");

    ASSERT_FALSE(telemetry.mApiErrorCounts.empty());
    ASSERT_FALSE(telemetry.mExternalServiceErrorCounts.empty());
    ASSERT_FALSE(telemetry.mApiLatencyStats.empty());
    ASSERT_FALSE(telemetry.mServiceLatencyStats.empty());
    ASSERT_FALSE(telemetry.mMetricsCache.empty());

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
    TestAppGatewayImplementation impl;
    std::string resolution;

    EXPECT_EQ(Core::ERROR_GENERAL, impl.Resolve(MakeImplementationContext(), "AppGateway", "device.name", "{}", resolution));
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
    ResponderNotificationStub notification;

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

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_UnregisterUnknown_ReturnsGeneral)
{
    TestAppGatewayResponderImplementation responder;
    ResponderNotificationStub notification;

    EXPECT_EQ(Core::ERROR_GENERAL, responder.Unregister(&notification));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_DuplicateRegisterSingleUnregister)
{
    TestAppGatewayResponderImplementation responder;
    ResponderNotificationStub notification;

    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
    EXPECT_EQ(Core::ERROR_GENERAL, responder.Unregister(&notification));
}

TEST(AppGatewayPluginTest, AppGatewayResponderImplementation_OnConnectionStatusChanged_NotifiesRegisteredClients)
{
    TestAppGatewayResponderImplementation responder;
    CapturingResponderNotification notification;

    ASSERT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    notification.ResetEvents();

    responder.OnConnectionStatusChanged("test.app", 55, true);

    EXPECT_TRUE(notification.WaitForRequestStatus(1000, CapturingResponderNotification::Responder_OnAppConnectionChanged));
    EXPECT_EQ(1u, notification.callCount);
    EXPECT_EQ("test.app", notification.lastAppId);
    EXPECT_EQ(55u, notification.lastConnectionId);
    EXPECT_TRUE(notification.lastConnected);

    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
}
