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
#include <string>

#include "Module.h"

#define private public
#include "AppGatewayCommon.h"
#undef private

#include "ServiceMock.h"
#include "MockJSONRPCDirectLink.h"
#include "MockEmitter.h"
#include "SystemServicesMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

class WorkerPoolGuard final {
public:
    WorkerPoolGuard(const WorkerPoolGuard&) = delete;
    WorkerPoolGuard& operator=(const WorkerPoolGuard&) = delete;

    WorkerPoolGuard()
        : mPool(2, 0, 64)
        , mAssigned(false)
    {
        if (Core::IWorkerPool::IsAvailable() == false) {
            Core::IWorkerPool::Assign(&mPool);
            mAssigned = true;
            mPool.Run();
        }
    }

    ~WorkerPoolGuard()
    {
        if (mAssigned) {
            mPool.Stop();
            Core::IWorkerPool::Assign(nullptr);
        }
    }

private:
    WorkerPoolImplementation mPool;
    bool mAssigned;
};

static WorkerPoolGuard gWorkerPool;

static Exchange::GatewayContext MakeContext()
{
    Exchange::GatewayContext ctx;
    ctx.appId = "test.app";
    ctx.connectionId = 100;
    ctx.requestId = 200;
    return ctx;
}

class SystemDelegateTest : public ::testing::Test {
protected:
    // Shared across all tests in this suite — initialized once in SetUpTestSuite.
    static Core::Sink<AppGatewayCommon>* sPlugin;
    static NiceMock<ServiceMock>* sService;
    static NiceMock<SystemServicesMock>* sSystemServices;
    static Core::Sink<MockJSONRPC::MockLocalDispatcher>* sSystemDisp;
    static Core::Sink<MockJSONRPC::MockLocalDispatcher>* sDisplayDisp;
    static Core::Sink<MockJSONRPC::MockLocalDispatcher>* sHdcpDisp;

    // Convenience references so existing TEST_F bodies compile unchanged.
    Core::Sink<AppGatewayCommon>& plugin = *sPlugin;
    NiceMock<ServiceMock>& service = *sService;
    NiceMock<SystemServicesMock>& systemServices = *sSystemServices;
    Core::Sink<MockJSONRPC::MockLocalDispatcher>& systemDispatcher = *sSystemDisp;
    Core::Sink<MockJSONRPC::MockLocalDispatcher>& displayDispatcher = *sDisplayDisp;
    Core::Sink<MockJSONRPC::MockLocalDispatcher>& hdcpDispatcher = *sHdcpDisp;

    static void SetUpTestSuite()
    {
        sService = new NiceMock<ServiceMock>();
        sSystemServices = new NiceMock<SystemServicesMock>();
        sSystemDisp = new Core::Sink<MockJSONRPC::MockLocalDispatcher>();
        sDisplayDisp = new Core::Sink<MockJSONRPC::MockLocalDispatcher>();
        sHdcpDisp = new Core::Sink<MockJSONRPC::MockLocalDispatcher>();
        sPlugin = new Core::Sink<AppGatewayCommon>();

        ON_CALL(*sService, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(*sService, QueryInterfaceByCallsign(Exchange::ISystemServices::ID, ::testing::StrEq("org.rdk.System")))
            .WillByDefault(::testing::Invoke([](uint32_t, const string&) -> void* {
                return static_cast<Exchange::ISystemServices*>(sSystemServices);
            }));

        ON_CALL(*sService, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.System")))
            .WillByDefault(::testing::Invoke([](uint32_t, const string&) -> void* {
                sSystemDisp->AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&*sSystemDisp);
            }));

        ON_CALL(*sService, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.DisplaySettings")))
            .WillByDefault(::testing::Invoke([](uint32_t, const string&) -> void* {
                sDisplayDisp->AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&*sDisplayDisp);
            }));

        ON_CALL(*sService, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.HdcpProfile")))
            .WillByDefault(::testing::Invoke([](uint32_t, const string&) -> void* {
                sHdcpDisp->AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&*sHdcpDisp);
            }));

        ON_CALL(*sService, QueryInterfaceByCallsign(PluginHost::IAuthenticate::ID, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(*sSystemServices, AddRef()).WillByDefault(Return());
        ON_CALL(*sSystemServices, Release()).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(*sSystemServices, Register(_)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(*sSystemServices, Unregister(_)).WillByDefault(Return(Core::ERROR_NONE));

        EXPECT_CALL(*sService, AddRef()).Times(AnyNumber());
        EXPECT_CALL(*sService, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = sPlugin->Initialize(sService);
        ASSERT_TRUE(response.empty());
    }

    static void TearDownTestSuite()
    {
        if (sPlugin && sService) {
            sPlugin->Deinitialize(sService);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        delete sPlugin;      sPlugin = nullptr;
        delete sHdcpDisp;    sHdcpDisp = nullptr;
        delete sDisplayDisp; sDisplayDisp = nullptr;
        delete sSystemDisp;  sSystemDisp = nullptr;
        delete sSystemServices; sSystemServices = nullptr;
        delete sService;     sService = nullptr;
    }
};

Core::Sink<AppGatewayCommon>* SystemDelegateTest::sPlugin = nullptr;
NiceMock<ServiceMock>* SystemDelegateTest::sService = nullptr;
NiceMock<SystemServicesMock>* SystemDelegateTest::sSystemServices = nullptr;
Core::Sink<MockJSONRPC::MockLocalDispatcher>* SystemDelegateTest::sSystemDisp = nullptr;
Core::Sink<MockJSONRPC::MockLocalDispatcher>* SystemDelegateTest::sDisplayDisp = nullptr;
Core::Sink<MockJSONRPC::MockLocalDispatcher>* SystemDelegateTest::sHdcpDisp = nullptr;

/* ---------- GetDeviceMake ---------- */

TEST_F(SystemDelegateTest, AGC_L1_090_GetDeviceMake_Success)
{
    ON_CALL(*sSystemServices, GetDeviceInfo(_, _))
        .WillByDefault(::testing::Invoke([](Exchange::ISystemServices::IStringIterator* const&, Exchange::ISystemServices::DeviceInfo& info) {
            info.make = "Arris";
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.make", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("Arris"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_091_GetDeviceMake_MissingField_ReturnsUnknown)
{
    ON_CALL(*sSystemServices, GetDeviceInfo(_, _))
        .WillByDefault(::testing::Invoke([](Exchange::ISystemServices::IStringIterator* const&, Exchange::ISystemServices::DeviceInfo& info) {
            info.make = "";
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.make", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("unknown"), std::string::npos);
}

/* ---------- GetDeviceName ---------- */

TEST_F(SystemDelegateTest, AGC_L1_092_GetDeviceName_Success)
{
    ON_CALL(*sSystemServices, GetFriendlyName(_, _))
        .WillByDefault(::testing::Invoke([](string& friendlyName, bool& success) {
            friendlyName = "Bedroom TV";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.name", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("Bedroom TV"), std::string::npos);
}

/* ---------- GetDeviceSku ---------- */

TEST_F(SystemDelegateTest, AGC_L1_093_GetDeviceSku_Success)
{
    ON_CALL(*sSystemServices, GetSystemVersions(_))
        .WillByDefault(::testing::Invoke([](Exchange::ISystemServices::SystemVersionsInfo& info) {
            info.stbVersion = "SKXI11ADS_VBN_23Q4_sprint_20240101123456";
            info.receiverVersion = "99.99.15.07";
            info.success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.sku", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("SKXI11ADS"), std::string::npos);
}

/* ---------- GetFirmwareVersion ---------- */

// Separate fixture: firmware-version tests need a fresh plugin instance because
// the firmware-version cache persists for the plugin's lifetime.
class SystemDelegateCacheTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<SystemServicesMock> systemServices;
    Core::Sink<MockJSONRPC::MockLocalDispatcher> systemDispatcher;
    Core::Sink<MockJSONRPC::MockLocalDispatcher> displayDispatcher;
    Core::Sink<MockJSONRPC::MockLocalDispatcher> hdcpDispatcher;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));
        ON_CALL(service, QueryInterfaceByCallsign(Exchange::ISystemServices::ID, ::testing::StrEq("org.rdk.System")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                return static_cast<Exchange::ISystemServices*>(&systemServices);
            }));
        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.System")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                systemDispatcher.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&systemDispatcher);
            }));
        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.DisplaySettings")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                displayDispatcher.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&displayDispatcher);
            }));
        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.HdcpProfile")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                hdcpDispatcher.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&hdcpDispatcher);
            }));
        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::IAuthenticate::ID, _))
            .WillByDefault(Return(nullptr));
        ON_CALL(systemServices, AddRef()).WillByDefault(Return());
        ON_CALL(systemServices, Release()).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(systemServices, Register(_)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(systemServices, Unregister(_)).WillByDefault(Return(Core::ERROR_NONE));
        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = plugin.Initialize(&service);
        ASSERT_TRUE(response.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

TEST_F(SystemDelegateCacheTest, AGC_L1_094_GetFirmwareVersion_Success)
{
    ON_CALL(systemServices, GetSystemVersions(_))
        .WillByDefault(::testing::Invoke([](Exchange::ISystemServices::SystemVersionsInfo& info) {
            info.receiverVersion = "99.99.15.07";
            info.stbVersion = "SKXI11ADS_VBN_23Q4";
            info.success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("firmware"), std::string::npos);
    EXPECT_NE(result.find("99"), std::string::npos);
}

/* ---------- Country Code (territory mapping) ---------- */

TEST_F(SystemDelegateTest, AGC_L1_095_GetCountryCode_USA_MapsToUS)
{
    ON_CALL(*sSystemServices, GetTerritory(_, _, _))
        .WillByDefault(::testing::Invoke([](string& territory, string& region, bool& success) {
            territory = "USA";
            region = "";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.countrycode", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("US"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_096_GetCountryCode_GBR_MapsToGB)
{
    ON_CALL(*sSystemServices, GetTerritory(_, _, _))
        .WillByDefault(::testing::Invoke([](string& territory, string& region, bool& success) {
            territory = "GBR";
            region = "";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.countrycode", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("GB"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_097_SetCountryCode_US_SetsUSA)
{
    ON_CALL(*sSystemServices, SetTerritory(_, _, _, _))
        .WillByDefault(::testing::Invoke([](const string& territory, const string& region, Exchange::ISystemServices::SystemError& error, bool& success) {
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setcountrycode", R"({"value":"US"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- Timezone ---------- */

TEST_F(SystemDelegateTest, AGC_L1_098_GetTimeZone_Success)
{
    ON_CALL(*sSystemServices, GetTimeZoneDST(_, _, _))
        .WillByDefault(::testing::Invoke([](string& timeZone, string& accuracy, bool& success) {
            timeZone = "America/New_York";
            accuracy = "INITIAL";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.timezone", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("America/New_York"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_099_SetTimeZone_Success)
{
    ON_CALL(*sSystemServices, SetTimeZoneDST(_, _, _, _, _))
        .WillByDefault(::testing::Invoke([](const string& timeZone, const string& accuracy, uint32_t& SysSrv_Status, string& errorMessage, bool& success) {
            success = true;
            SysSrv_Status = 0;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.settimezone", R"({"value":"America/Chicago"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- Screen Resolution ---------- */

TEST_F(SystemDelegateTest, AGC_L1_100_GetScreenResolution_Success)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"w":3840,"h":2160})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.screenresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[3840,2160]", result);
}

/* ---------- Video Resolution ---------- */

TEST_F(SystemDelegateTest, AGC_L1_101_GetVideoResolution_UHD)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"w":3840,"h":2160})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.videoresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[3840,2160]", result);
}

TEST_F(SystemDelegateTest, AGC_L1_102_GetVideoResolution_FHD)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"w":1920,"h":1080})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.videoresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[1920,1080]", result);
}

/* ---------- HDCP ---------- */

TEST_F(SystemDelegateTest, AGC_L1_103_GetHdcp_HDCP14Connected)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"HDCPStatus":{"hdcpReason":2,"currentHDCPVersion":"1.4"}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdcp1.4\":true,\"hdcp2.2\":false}", result);
}

TEST_F(SystemDelegateTest, AGC_L1_104_GetHdcp_HDCP22Connected)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"HDCPStatus":{"hdcpReason":2,"currentHDCPVersion":"2.2"}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":true}", result);
}

/* ---------- HDR ---------- */

TEST_F(SystemDelegateTest, AGC_L1_105_GetHdr_AllCapabilities)
{
    displayDispatcher.SetHandler("getTVHDRCapabilities", [](const std::string&, const std::string&, std::string& resp) {
        // 0x01 | 0x02 | 0x04 | 0x10 = 0x17 = 23
        resp = R"({"capabilities":23,"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdr", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdr10\":true,\"dolbyVision\":true,\"hlg\":true,\"hdr10Plus\":true}", result);
}

TEST_F(SystemDelegateTest, AGC_L1_106_GetHdr_NoCapabilities)
{
    displayDispatcher.SetHandler("getTVHDRCapabilities", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"capabilities":0,"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdr", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}", result);
}

/* ---------- Audio ---------- */

TEST_F(SystemDelegateTest, AGC_L1_107_GetAudio_MultipleFlagsSet)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedAudioFormat":["PCM","EAC3","ATMOS"],"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("\"stereo\":true"), std::string::npos);
    EXPECT_NE(result.find("\"dolbyAtmos\":true"), std::string::npos);
    EXPECT_NE(result.find("\"dolbyDigital5.1+\":true"), std::string::npos);
}

/* ---------- RPC failure paths ---------- */

TEST_F(SystemDelegateTest, AGC_L1_108_GetDeviceMake_RPCFailure)
{
    systemDispatcher.SetHandler("getDeviceInfo", [](const std::string&, const std::string&, std::string& resp) {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.make", "{}", result);

    // On RPC failure the delegate falls back to "unknown"
    EXPECT_NE(result.find("unknown"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_109_GetScreenResolution_RPCFailure)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.screenresolution", "{}", result);

    // Falls back to [1920,1080]
    EXPECT_EQ("[1920,1080]", result);
}

TEST_F(SystemDelegateTest, AGC_L1_110_GetVideoResolution_RPCFailure)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.videoresolution", "{}", result);

    EXPECT_EQ("[1920,1080]", result);
}

TEST_F(SystemDelegateTest, AGC_L1_111_GetHdcp_RPCFailure)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    // Falls back to all-false
    EXPECT_NE(result.find("false"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_112_GetHdr_RPCFailure)
{
    displayDispatcher.SetHandler("getTVHDRCapabilities", [](const std::string&, const std::string&, std::string& resp) {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdr", "{}", result);

    EXPECT_NE(result.find("false"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_113_GetAudio_RPCFailure)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    // Falls back to stereo-off default
    EXPECT_NE(result.find("\"stereo\":false"), std::string::npos);
}

TEST_F(SystemDelegateCacheTest, AGC_L1_114_GetFirmwareVersion_Success)
{
     ON_CALL(systemServices, GetSystemVersions(_))
        .WillByDefault(::testing::Invoke([](Exchange::ISystemServices::SystemVersionsInfo& info) {
            info.receiverVersion = "4.4.1";
            info.stbVersion = "SCXI11BEI_VBN_24Q2_sprint_20240611103551sdy_FG_EDGE_R4.4.1";
            info.success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Should extract firmware version from receiverVersion
    EXPECT_FALSE(result.empty());
}

TEST_F(SystemDelegateTest, AGC_L1_115_GetDeviceSku_MissingField)
{
    ON_CALL(*sSystemServices, GetSystemVersions(_))
        .WillByDefault(::testing::Invoke([](Exchange::ISystemServices::SystemVersionsInfo& info) {
            info.stbVersion = "";
            info.receiverVersion = "";
            info.success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.sku", "{}", result);

    // When stbVersion is missing, skuOut stays empty and ERROR_UNAVAILABLE is returned
    EXPECT_TRUE(result.empty());
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

/* ---------- secondscreen.friendlyname (alias for device.name) ---------- */

TEST_F(SystemDelegateTest, AGC_L1_116_SecondScreenFriendlyName_Success)
{
    ON_CALL(*sSystemServices, GetFriendlyName(_, _))
        .WillByDefault(::testing::Invoke([](string& friendlyName, bool& success) {
            friendlyName = "Kitchen TV";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "secondscreen.friendlyname", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("Kitchen TV"), std::string::npos);
}

/* ---------- device.setname success via JSON-RPC mock ---------- */

TEST_F(SystemDelegateTest, AGC_L1_117_SetDeviceName_Success)
{
    ON_CALL(*sSystemServices, SetFriendlyName(_, _))
        .WillByDefault(::testing::Invoke([](const string& friendlyName, Exchange::ISystemServices::SystemResult& result) {
            result.success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.setname", R"({"value":"Bedroom"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- device.setname failure via JSON-RPC mock ---------- */

TEST_F(SystemDelegateTest, AGC_L1_118_SetDeviceName_Failure)
{
    ON_CALL(*sSystemServices, SetFriendlyName(_, _))
        .WillByDefault(::testing::Invoke([](const string& friendlyName, Exchange::ISystemServices::SystemResult& result) {
            result.success = false;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.setname", R"({"value":"Bedroom"})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ---------- localization.setcountrycode success ---------- */

TEST_F(SystemDelegateTest, AGC_L1_119_SetCountryCode_Success)
{
    ON_CALL(*sSystemServices, SetTerritory(_, _, _, _))
        .WillByDefault(::testing::Invoke([](const string& territory, const string& region, Exchange::ISystemServices::SystemError& error, bool& success) {
            success = true;
            return Core::ERROR_NONE;
        }));
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setcountrycode", R"({"value":"GB"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- localization.settimezone success ---------- */

TEST_F(SystemDelegateTest, AGC_L1_120_SetTimezone_Success)
{
    ON_CALL(*sSystemServices, SetTimeZoneDST(_, _, _, _, _))
        .WillByDefault(::testing::Invoke([](const string& timeZone, const string& accuracy, uint32_t& SysSrv_Status, string& errorMessage, bool& success) {
            success = true;
            SysSrv_Status = 0;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.settimezone", R"({"value":"America/Chicago"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- GetFirmwareVersion with receiverVersion parsing failure ---------- */

TEST_F(SystemDelegateCacheTest, AGC_L1_121_GetFirmwareVersion_BadReceiverVersion)
{
    systemDispatcher.SetHandler("getSystemVersions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"receiverVersion":"not-a-version","stbVersion":"NOTVALID","success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result);

    // receiverVersion doesn't parse as major.minor.patch → ERROR_UNAVAILABLE
    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ========================================================================
 * Category A Coverage: SystemDelegate uncovered branches
 * ======================================================================== */

/* ---------- GetScreenResolution: nested result.w/h parsing ---------- */

TEST_F(SystemDelegateTest, AGC_L1_122_GetScreenResolution_NestedResultWH)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"result":{"w":3840,"h":2160}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.screenresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[3840,2160]", result);
}

TEST_F(SystemDelegateTest, AGC_L1_123_GetScreenResolution_NestedResultWidthHeight)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"result":{"width":1280,"height":720}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.screenresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[1280,720]", result);
}

/* ---------- GetVideoResolution: UHD threshold from screen resolution ---------- */

TEST_F(SystemDelegateTest, AGC_L1_124_GetVideoResolution_UHDFrom4KScreen)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"w":3840,"h":2160})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.videoresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[3840,2160]", result);
}

/* ---------- GetHdcp: nested result path ---------- */

TEST_F(SystemDelegateTest, AGC_L1_125_GetHdcp_NestedResult_Hdcp22)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"result":{"success":true,"HDCPStatus":{"hdcpReason":2,"currentHDCPVersion":"2.2"}}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":true}", result);
}

TEST_F(SystemDelegateTest, AGC_L1_126_GetHdcp_NestedResult_Hdcp14)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"result":{"success":true,"HDCPStatus":{"hdcpReason":2,"currentHDCPVersion":"1.4"}}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdcp1.4\":true,\"hdcp2.2\":false}", result);
}

/* ---------- GetHdcp: reason != 2 → both false ---------- */

TEST_F(SystemDelegateTest, AGC_L1_127_GetHdcp_ReasonNotConnected_BothFalse)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"HDCPStatus":{"hdcpReason":0,"currentHDCPVersion":"1.4"}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":false}", result);
}

/* ---------- GetAudio: AC3 vs EAC3 disambiguation ---------- */

TEST_F(SystemDelegateTest, AGC_L1_128_GetAudio_EAC3NotMatchedAsAC3)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedAudioFormat":["EAC3"]})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // EAC3 should set dolbyDigital5.1+ but NOT dolbyDigital5.1
    EXPECT_NE(result.find("\"dolbyDigital5.1+\":true"), std::string::npos);
    EXPECT_NE(result.find("\"dolbyDigital5.1\":false"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_129_GetAudio_StandaloneAC3MatchesDolbyDigital)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedAudioFormat":["AC3"]})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Standalone AC3 (not preceded by E) → dolbyDigital5.1 = true
    EXPECT_NE(result.find("\"dolbyDigital5.1\":true"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_130_GetAudio_AtmosDetection)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedAudioFormat":["DOLBY ATMOS","PCM"]})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("\"dolbyAtmos\":true"), std::string::npos);
    EXPECT_NE(result.find("\"stereo\":true"), std::string::npos);
}

TEST_F(SystemDelegateTest, AGC_L1_131_GetAudio_NestedResultSupportedAudioFormat)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"result":{"supportedAudioFormat":["PCM","AC4"]}})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("\"stereo\":true"), std::string::npos);
    EXPECT_NE(result.find("\"dolbyDigital5.1+\":true"), std::string::npos);
}

/* ---------- GetDeviceName: missing friendlyName -> falls back ---------- */

TEST_F(SystemDelegateTest, AGC_L1_132_GetDeviceName_MissingFriendlyName)
{
    ON_CALL(*sSystemServices, GetFriendlyName(_, _))
        .WillByDefault(::testing::Invoke([](string& friendlyName, bool& success) {
            friendlyName = "";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.name", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Should return some default value when friendlyName is missing
    EXPECT_FALSE(result.empty());
}

/* ---------- GetFirmwareVersion: cached response second call ---------- */

TEST_F(SystemDelegateCacheTest, AGC_L1_133_GetFirmwareVersion_CacheHitOnSecondCall)
{
    int callCount = 0;
    EXPECT_CALL(systemServices, GetSystemVersions(_))
        .WillRepeatedly(::testing::Invoke([&callCount](Exchange::ISystemServices::SystemVersionsInfo& info) {
            ++callCount;
            info.receiverVersion = "99.88.77.66";
            info.stbVersion = "PLATFORM_DEV_20250101_TEST";
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();

    // First call populates cache
    string result1;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result1));
    EXPECT_NE(result1.find("\"firmware\""), std::string::npos);
    EXPECT_EQ(1, callCount);

    // Second call should use cached response (callCount should not increase)
    string result2;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result2));
    EXPECT_EQ(result1, result2);
    EXPECT_EQ(1, callCount);
}

/* ---------- GetHdr: single HLG-only capability ---------- */

TEST_F(SystemDelegateTest, AGC_L1_134_GetHdr_HlgOnly)
{
    displayDispatcher.SetHandler("getTVHDRCapabilities", [](const std::string&, const std::string&, std::string& resp) {
        // capabilities=2 → only HDRSTANDARD_HLG bit set
        resp = R"({"capabilities":2,"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdr", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":true,\"hdr10Plus\":false}", result);
}

/* ---------- GetCountryCode: non-US/GB territory mapping ---------- */

TEST_F(SystemDelegateTest, AGC_L1_135_GetCountryCode_ItalyMapping)
{
    ON_CALL(*sSystemServices, GetTerritory(_, _, _))
        .WillByDefault(::testing::Invoke([](string& territory, string& region, bool& success) {
            territory = "ITA";
            region = "";
            success = true;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.countrycode", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("IT"), std::string::npos);
}

/* ---------- GetFirmwareVersion: missing stbVersion ---------- */

TEST_F(SystemDelegateCacheTest, AGC_L1_136_GetFirmwareVersion_MissingStbVersion)
{
    systemDispatcher.SetHandler("getSystemVersions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"receiverVersion":"99.88.77.66","stbVersion":"","success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ---------- SetCountryCode: RPC failure ---------- */

TEST_F(SystemDelegateTest, AGC_L1_137_SetCountryCode_Failure)
{
    ON_CALL(*sSystemServices, SetTerritory(_, _, _, _))
        .WillByDefault(::testing::Invoke([](const string& territory, const string& region, Exchange::ISystemServices::SystemError& error, bool& success) {
            success = false;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setcountrycode", R"({"value":"US"})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ---------- SetTimeZone: RPC failure ---------- */

TEST_F(SystemDelegateTest, AGC_L1_138_SetTimeZone_Failure)
{
    ON_CALL(*sSystemServices, SetTimeZoneDST(_, _, _, _, _))
        .WillByDefault(::testing::Invoke([](const string& timeZone, const string& accuracy, uint32_t& SysSrv_Status, string& errorMessage, bool& success) {
            success = false;
            SysSrv_Status = 1;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.settimezone", R"({"value":"America/New_York"})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ---------- GetTimeZone: success=true but missing timeZone field ---------- */

TEST_F(SystemDelegateTest, AGC_L1_139_GetTimeZone_MissingTimeZoneField)
{
    ON_CALL(*sSystemServices, GetTimeZoneDST(_, _, _))
        .WillByDefault(::testing::Invoke([](string& timeZone, string& accuracy, bool& success) {
            timeZone = "";
            accuracy = "";
            success = false;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.timezone", "{}", result);

    // success=false should return error
    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ================================================================
 * Gap C – SystemDelegate Emit notification dispatch tests
 *
 * These tests verify that when a Device.* event fires, the
 * SystemDelegate re-queries the underlying JSON-RPC plugin,
 * constructs the payload, and dispatches to registered emitters.
 * ================================================================ */

class SystemDelegateEmitTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<SystemServicesMock> systemServices;
    Core::Sink<MockJSONRPC::MockLocalDispatcher> systemDispatcher;
    Core::Sink<MockJSONRPC::MockLocalDispatcher> displayDispatcher;
    Core::Sink<MockJSONRPC::MockLocalDispatcher> hdcpDispatcher;
    std::vector<MockEmitter*> heapEmitters;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(service, QueryInterfaceByCallsign(Exchange::ISystemServices::ID, ::testing::StrEq("org.rdk.System")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                return static_cast<Exchange::ISystemServices*>(&systemServices);
            }));
        
        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.System")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                systemDispatcher.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&systemDispatcher);
            }));

        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.DisplaySettings")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                displayDispatcher.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&displayDispatcher);
            }));

        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, ::testing::StrEq("org.rdk.HdcpProfile")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                hdcpDispatcher.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&hdcpDispatcher);
            }));

        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::IAuthenticate::ID, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(systemServices, AddRef()).WillByDefault(Return());
        ON_CALL(systemServices, Release()).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(systemServices, Register(_)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(systemServices, Unregister(_)).WillByDefault(Return(Core::ERROR_NONE));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = plugin.Initialize(&service);
        ASSERT_TRUE(response.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        // Wait for any pending async dispatch jobs to complete before
        // destroying emitters to prevent use-after-free.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (auto* e : heapEmitters) {
            testing::Mock::VerifyAndClearExpectations(e);
            delete e;
        }
        heapEmitters.clear();
    }

    // Register a heap-allocated MockEmitter directly on the SystemDelegate's
    // notification map via AddNotification.  This avoids the async
    // HandleAppEventNotifier → EventRegistrationJob path which triggers
    // four slow SetupXxx subscription retries (~2 s timeout each ≈ 8 s),
    // saturating the 2-thread worker pool and preventing the subsequent
    // EventDelegateDispatchJob from executing within the test window.
    MockEmitter* SubscribeEmitter(const std::string& event)
    {
        MockEmitter* emitter = new MockEmitter();
        heapEmitters.push_back(emitter);
        emitter->AddRef();

        auto systemDelegate = plugin.mDelegate->getSystemDelegate();
        EXPECT_NE(systemDelegate, nullptr);
        systemDelegate->AddNotification(event, emitter);

        return emitter;
    }
};

TEST_F(SystemDelegateEmitTest, AGC_L1_140_EmitOnScreenResolutionChanged_DispatchesToEmitter)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"w":3840,"h":2160})";
        return Core::ERROR_NONE;
    });

    MockEmitter* emitter = SubscribeEmitter("Device.onScreenResolutionChanged");

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Device.onScreenResolutionChanged"), ::testing::Eq("[3840,2160]"), _))
        .Times(::testing::AtLeast(1));

    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    systemDelegate->EmitOnScreenResolutionChanged();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(SystemDelegateEmitTest, AGC_L1_141_EmitOnVideoResolutionChanged_DispatchesToEmitter)
{
    displayDispatcher.SetHandler("getCurrentResolution", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"w":3840,"h":2160})";
        return Core::ERROR_NONE;
    });

    MockEmitter* emitter = SubscribeEmitter("Device.onVideoResolutionChanged");

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Device.onVideoResolutionChanged"), ::testing::Eq("[3840,2160]"), _))
        .Times(::testing::AtLeast(1));

    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    systemDelegate->EmitOnVideoResolutionChanged();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(SystemDelegateEmitTest, AGC_L1_142_EmitOnHdcpChanged_DispatchesToEmitter)
{
    hdcpDispatcher.SetHandler("getHDCPStatus", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"HDCPStatus":{"isConnected":true,"isHDCPCompliant":true,"isHDCPEnabled":true,"hdcpReason":1,"supportedHDCPVersion":"2.2","receiverHDCPVersion":"1.4","currentHDCPVersion":"1.4"}})";
        return Core::ERROR_NONE;
    });

    MockEmitter* emitter = SubscribeEmitter("Device.onHdcpChanged");

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Device.onHdcpChanged"), _, _))
        .Times(::testing::AtLeast(1));

    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    systemDelegate->EmitOnHdcpChanged();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(SystemDelegateEmitTest, AGC_L1_143_EmitOnHdrChanged_DispatchesToEmitter)
{
    displayDispatcher.SetHandler("getTVHDRCapabilities", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"capabilities":23,"success":true})";
        return Core::ERROR_NONE;
    });

    MockEmitter* emitter = SubscribeEmitter("Device.onHdrChanged");

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Device.onHdrChanged"), _, _))
        .Times(::testing::AtLeast(1));

    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    systemDelegate->EmitOnHdrChanged();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(SystemDelegateEmitTest, AGC_L1_144_EmitOnAudioChanged_DispatchesToEmitter)
{
    displayDispatcher.SetHandler("getAudioFormat", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedAudioFormat":["PCM","DOLBY DIGITAL 5.1"]})";
        return Core::ERROR_NONE;
    });

    MockEmitter* emitter = SubscribeEmitter("Device.onAudioChanged");

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Device.onAudioChanged"), _, _))
        .Times(::testing::AtLeast(1));

    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    systemDelegate->EmitOnAudioChanged();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(SystemDelegateEmitTest, AGC_L1_145_EmitOnNameChanged_DispatchesToEmitter)
{
    ON_CALL(systemServices, GetFriendlyName(_, _))
        .WillByDefault(::testing::Invoke([](string& friendlyName, bool& success) {
            friendlyName = "Living Room TV";
            success = true;
            return Core::ERROR_NONE;
        }));

    MockEmitter* emitter = SubscribeEmitter("Device.onDeviceNameChanged");

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Device.onDeviceNameChanged"), ::testing::HasSubstr("Living Room TV"), _))
        .Times(::testing::AtLeast(1));

    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    systemDelegate->EmitOnNameChanged();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

/* ================================================================
 * Gap 4 – SystemDelegate::HandleEvent unsubscribe path
 *
 * Verifies that HandleEvent(listen=false) correctly removes an
 * emitter from the notification map via RemoveNotification.
 * ================================================================ */

TEST_F(SystemDelegateEmitTest, AGC_L1_146_HandleEvent_Unsubscribe_RemovesEmitter)
{
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);

    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();

    // Subscribe directly via AddNotification
    systemDelegate->AddNotification("Device.onHdrChanged", emitter);
    EXPECT_TRUE(systemDelegate->IsNotificationRegistered("Device.onHdrChanged"));

    // Now use HandleEvent to unsubscribe
    bool registrationError = true;
    const bool handled = systemDelegate->HandleEvent(emitter, "Device.onHdrChanged", false, registrationError);

    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    // After removal the notification should no longer be registered
    EXPECT_FALSE(systemDelegate->IsNotificationRegistered("Device.onHdrChanged"));
}

} // namespace
