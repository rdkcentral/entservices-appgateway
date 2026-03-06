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
#include <string>
#include <thread>

#include "Module.h"

#define private public
#include "AppGatewayCommon.h"
#include "delegate/UserSettingsDelegate.h"
#include "delegate/LifecycleDelegate.h"
#include "delegate/SystemDelegate.h"
#undef private

#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "IUserSettingsMock.h"
#include "INetworkManagerMock.h"
#include "ISharedStorageMock.h"
#include "ITextToSpeechMock.h"
#include "ILifecycleManagerStateMock.h"

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
        }
        mPool.Run();
    }

    ~WorkerPoolGuard()
    {
        mPool.Stop();
        if (mAssigned) {
            Core::IWorkerPool::Assign(nullptr);
        }
    }

private:
    WorkerPoolImplementation mPool;
    bool mAssigned;
};

static WorkerPoolGuard gWorkerPool;

class WorkerPoolDrainGuard final {
public:
    ~WorkerPoolDrainGuard()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

static WorkerPoolDrainGuard gWorkerPoolDrain;

static Exchange::GatewayContext MakeContext()
{
    Exchange::GatewayContext ctx;
    ctx.appId = "test.app";
    ctx.connectionId = 100;
    ctx.requestId = 200;
    return ctx;
}

class TestEmitter : public Exchange::IAppNotificationHandler::IEmitter {
public:
    void Emit(const string& event, const string& payload, const string& appId) override
    {
        (void)event;
        (void)payload;
        (void)appId;
    }

    BEGIN_INTERFACE_MAP(TestEmitter)
    INTERFACE_ENTRY(Exchange::IAppNotificationHandler::IEmitter)
    END_INTERFACE_MAP
};

class AppGatewayCommonTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;

    void TearDown() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
};

TEST_F(AppGatewayCommonTest, AGC_L1_001_002_InitializeAndDeinitialize_Success)
{
    NiceMock<ServiceMock> service;

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(1).WillOnce(Return(Core::ERROR_NONE));

    const string response = plugin.Initialize(&service);
    EXPECT_TRUE(response.empty());

    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, DISABLED_AGC_L1_003_HandleAppEventNotifier_SubmitsJob)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(1).WillOnce(Return(Core::ERROR_NONE));

    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());

    Core::Sink<TestEmitter> emitter;

    bool status = false;
    const auto rc = plugin.HandleAppEventNotifier(&emitter, "Device.onHdrChanged", true, status);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_004_UnknownMethod_ReturnsUnknownKey)
{
    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "foo.bar", "{}", result);

    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, rc);
    EXPECT_FALSE(result.empty());
}

TEST_F(AppGatewayCommonTest, AGC_L1_009_DeviceSetName_InvalidPayload)
{
    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.setname", "{invalid", result);

    EXPECT_EQ(Core::ERROR_BAD_REQUEST, rc);
    EXPECT_EQ("{\"error\":\"Invalid payload\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_011_013_016_018_020_022_026_027_InvalidPayloads)
{
    const auto ctx = MakeContext();

    const std::vector<std::string> methods = {
        "localization.setcountrycode",
        "localization.settimezone",
        "voiceguidance.setenabled",
        "audiodescriptions.setenabled",
        "closedcaptions.setenabled",
        "localization.setlocale",
        "localization.setpreferredaudiolanguages",
        "closedcaptions.setpreferredlanguages",
        "voiceguidance.setspeed",
        "voiceguidance.setnavigationhints"
    };

    for (const auto& method : methods) {
        string result;
        const auto rc = plugin.HandleAppGatewayRequest(ctx, method, "{invalid", result);
        EXPECT_EQ(Core::ERROR_BAD_REQUEST, rc) << "method=" << method;
        EXPECT_EQ("{\"error\":\"Invalid payload\"}", result) << "method=" << method;
    }
}

TEST_F(AppGatewayCommonTest, AGC_L1_005_006_007_010_012_014_015_017_019_021_023_024_025_NullDelegate)
{
    plugin.mDelegate.reset();
    const auto ctx = MakeContext();

    const std::vector<std::string> methods = {
        "device.make",
        "device.name",
        "device.sku",
        "localization.countrycode",
        "localization.timezone",
        "device.network",
        "voiceguidance.enabled",
        "audiodescriptions.enabled",
        "closedcaptions.enabled",
        "localization.locale",
        "localization.language",
        "localization.preferredaudiolanguages",
        "closedcaptions.preferredlanguages"
    };

    for (const auto& method : methods) {
        string result;
        const auto rc = plugin.HandleAppGatewayRequest(ctx, method, "{}", result);
        EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc) << "method=" << method;
    }
}

TEST_F(AppGatewayCommonTest, AGC_L1_042_DefaultCapabilityFallbacks_WhenDelegateUnavailable)
{
    plugin.mDelegate.reset();

    string result;

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetScreenResolution(result));
    EXPECT_EQ("[1920,1080]", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetVideoResolution(result));
    EXPECT_EQ("[1920,1080]", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetHdcp(result));
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":false}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetHdr(result));
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetAudio(result));
    EXPECT_EQ("{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_038_039_VoiceGuidanceSettings_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetVoiceGuidanceSettings(true, result));
    EXPECT_EQ("{\"error\":\"couldn't get voice guidance settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_040_041_ClosedCaptionsSettings_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetClosedCaptionsSettings(result));
    EXPECT_EQ("{\"error\":\"couldn't get closed captions settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_028_032_GetSpeed_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    double speed = 0.0;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetSpeed(speed));
}

TEST_F(AppGatewayCommonTest, AGC_L1_033_037_SetSpeed_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(2.0));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.67));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.33));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.0));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(0.8));
}

TEST_F(AppGatewayCommonTest, AGC_L1_044_045_LifecycleAndAuth_NullDelegate_ReturnUnavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    string out;

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.Authenticate("session-1", out));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetSessionId("test.app", out));

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.LifecycleReady(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.LifecycleClose(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.LifecycleState(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.Lifecycle2State(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.Lifecycle2Close(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.LifecycleFinished(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.DispatchLastIntent(ctx, "{}", result));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetLastIntent(ctx, "{}", result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_046_CheckPermissionGroup_DefaultAllowed)
{
    bool allowed = false;
    const auto rc = plugin.CheckPermissionGroup("test.app", "any.group", allowed);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(allowed);
}

TEST_F(AppGatewayCommonTest, AGC_L1_043_FirmwareVersion_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetFirmwareVersion(result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_008_DeviceSetName_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.setname", "{\"value\":\"Bedroom\"}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_Extra_AddAdditionalInfo_RouteSuccess)
{
    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.addadditionalinfo", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_CaseInsensitiveRouting_UnknownAndKnown)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "DEVICE.NETWORK", "{}", result));

    result.clear();
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, plugin.HandleAppGatewayRequest(ctx, "Not.A.Method", "{}", result));
}

// ============================================================================
// POSITIVE TEST CASES - Valid payloads with null delegate
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_047_SetCountryCode_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setcountrycode", "{\"value\":\"US\"}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_048_SetTimeZone_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.settimezone", "{\"value\":\"America/New_York\"}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_049_SetVoiceGuidanceEnabled_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":true}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_050_SetAudioDescriptionsEnabled_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", "{\"value\":true}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_051_SetClosedCaptionsEnabled_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{\"value\":true}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_052_SetLocale_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setlocale", "{\"value\":\"en-US\"}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_053_SetPreferredAudioLanguages_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", "{\"value\":\"eng,spa\"}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_054_SetClosedCaptionsPreferredLanguages_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", "{\"value\":\"eng\"}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_055_SetVoiceGuidanceSpeed_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":1.5}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_056_SetVoiceGuidanceNavigationHints_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", "{\"value\":true}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

// ============================================================================
// VOICEGUIDANCE SPEED/RATE ROUTES
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_057_VoiceGuidanceSpeed_Route_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_058_VoiceGuidanceRate_Route_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.rate", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_059_SetVoiceGuidanceRate_ValidPayload_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setrate", "{\"value\":1.33}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_060_SetVoiceGuidanceRate_InvalidPayload)
{
    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setrate", "{invalid", result);

    EXPECT_EQ(Core::ERROR_BAD_REQUEST, rc);
    EXPECT_EQ("{\"error\":\"Invalid payload\"}", result);
}

// ============================================================================
// METRICS PREFIX ROUTING
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_061_Metrics_Prefix_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "metrics.action", "{\"type\":\"app_loaded\"}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_062_Metrics_AnySubMethod_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;

    // Test various metrics sub-methods
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "metrics.startContent", "{}", result));
    EXPECT_EQ("null", result);

    result.clear();
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "metrics.stopContent", "{}", result));
    EXPECT_EQ("null", result);

    result.clear();
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "Metrics.Ready", "{}", result));
    EXPECT_EQ("null", result);
}

// ============================================================================
// DISCOVERY.WATCHED ROUTING
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_063_DiscoveryWatched_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "discovery.watched", "{\"entityId\":\"123\"}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

// ============================================================================
// ACCESSIBILITY ROUTES
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_064_AccessibilityVoiceGuidanceSettings_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldn't get voice guidance settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_065_AccessibilityVoiceGuidance_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidance", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldn't get voice guidance settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_066_AccessibilityAudioDescriptionSettings_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescriptionsettings", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldnt get audio description settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_067_AccessibilityAudioDescription_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescription", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldnt get audio descriptions enabled\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_068_AccessibilityHighContrastUI_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldnt get high contrast state\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_069_AccessibilityClosedCaptions_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptions", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldn't get closed captions settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_070_AccessibilityClosedCaptionsSettings_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptionssettings", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldn't get closed captions settings\"}", result);
}

// ============================================================================
// HANDLE APP DELEGATE REQUEST (advertising/device.uid)
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_071_AdvertisingId_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_072_DeviceUid_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

// ============================================================================
// NETWORK.CONNECTED ROUTE
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_073_NetworkConnected_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldn't get network connected status\"}", result);
}

// ============================================================================
// DEVICE VERSION/FIRMWARE
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_074_DeviceVersion_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

// ============================================================================
// DEVICE SCREEN/VIDEO RESOLUTION, HDCP, HDR, AUDIO
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_075_DeviceScreenResolution_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.screenresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("[1920,1080]", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_076_DeviceVideoResolution_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.videoresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("[1920,1080]", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_077_DeviceHdcp_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":false}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_078_DeviceHdr_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.hdr", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_079_DeviceAudio_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", result);
}

// ============================================================================
// VOICEGUIDANCE NAVIGATION HINTS
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_080_VoiceGuidanceNavigationHints_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.navigationhints", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("{\"error\":\"couldnt get navigationHints\"}", result);
}

// ============================================================================
// SECONDSCREEN FRIENDLY NAME
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_081_SecondScreenFriendlyName_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "secondscreen.friendlyname", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

// ============================================================================
// LIFECYCLE METHODS VIA HANDLER MAP
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_082_LifecycleReady_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_083_LifecycleClose_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_084_LifecycleState_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_085_Lifecycle2State_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_086_Lifecycle2Close_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_087_LifecycleFinished_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.finished", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_088_CommonInternalDispatchIntent_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.dispatchintent", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

TEST_F(AppGatewayCommonTest, AGC_L1_089_CommonInternalGetLastIntent_ViaHandlerMap_NoDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

// ============================================================================
// BOUNDARY TEST CASES - Speed transformation
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_090_SetSpeed_BoundaryValues_NoDelegate)
{
    plugin.mDelegate.reset();

    // Test all boundary values for speed transformation
    // speed == 2.0 -> 10.0
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(2.0));

    // speed >= 1.67 -> 1.38
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.67));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.99));

    // speed >= 1.33 -> 1.19
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.33));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.66));

    // speed >= 1.0 -> 1.0
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.0));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(1.32));

    // speed < 1.0 -> 0.1
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(0.5));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(0.0));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetSpeed(-1.0));
}

// ============================================================================
// DIRECT METHOD CALLS - Null Delegate Coverage
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_091_GetVoiceGuidance_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetVoiceGuidance(result));
    EXPECT_EQ("{\"error\":\"couldnt get voiceguidance state\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_092_GetAudioDescription_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetAudioDescription(result));
    EXPECT_EQ("{\"error\":\"couldnt get audio description settings\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_093_GetAudioDescriptionsEnabled_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetAudioDescriptionsEnabled(result));
    EXPECT_EQ("{\"error\":\"couldnt get audio descriptions enabled\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_094_GetHighContrast_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetHighContrast(result));
    EXPECT_EQ("{\"error\":\"couldnt get high contrast state\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_095_GetCaptions_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetCaptions(result));
    EXPECT_EQ("{\"error\":\"couldnt get captions state\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_096_GetPresentationLanguage_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetPresentationLanguage(result));
    EXPECT_EQ("{\"error\":\"couldn't get language\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_097_GetLocale_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetLocale(result));
    EXPECT_EQ("{\"error\":\"couldn't get locale\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_098_SetLocale_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetLocale("en-US"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_099_GetPreferredAudioLanguages_NullDelegate_ReturnsEmpty)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetPreferredAudioLanguages(result));
    EXPECT_EQ("[]", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_100_GetPreferredCaptionsLanguages_NullDelegate_ReturnsDefault)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetPreferredCaptionsLanguages(result));
    EXPECT_EQ("[\"eng\"]", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_101_SetPreferredAudioLanguages_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetPreferredAudioLanguages("eng,spa"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_102_SetPreferredCaptionsLanguages_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetPreferredCaptionsLanguages("eng"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_103_SetVoiceGuidance_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetVoiceGuidance(true));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetVoiceGuidance(false));
}

TEST_F(AppGatewayCommonTest, AGC_L1_104_SetAudioDescriptionsEnabled_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetAudioDescriptionsEnabled(true));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetAudioDescriptionsEnabled(false));
}

TEST_F(AppGatewayCommonTest, AGC_L1_105_SetCaptions_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetCaptions(true));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetCaptions(false));
}

TEST_F(AppGatewayCommonTest, AGC_L1_106_GetVoiceGuidanceHints_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetVoiceGuidanceHints(result));
    EXPECT_EQ("{\"error\":\"couldnt get navigationHints\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_107_SetVoiceGuidanceHints_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetVoiceGuidanceHints(true));
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetVoiceGuidanceHints(false));
}

TEST_F(AppGatewayCommonTest, AGC_L1_108_GetInternetConnectionStatus_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetInternetConnectionStatus(result));
    EXPECT_EQ("{\"error\":\"couldn't get internet connection status\"}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_109_GetNetworkConnected_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetNetworkConnected(result));
    EXPECT_EQ("{\"error\":\"couldn't get network connected status\"}", result);
}

// ============================================================================
// SYSTEM DELEGATE METHODS - Null Delegate Coverage
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_110_GetDeviceMake_NullDelegate)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetDeviceMake(result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_111_GetDeviceName_NullDelegate)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetDeviceName(result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_112_SetDeviceName_NullDelegate)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetDeviceName("TestDevice"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_113_GetDeviceSku_NullDelegate)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetDeviceSku(result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_114_GetCountryCode_NullDelegate)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetCountryCode(result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_115_SetCountryCode_NullDelegate)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetCountryCode("US"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_116_GetTimeZone_NullDelegate)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetTimeZone(result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_117_SetTimeZone_NullDelegate)
{
    plugin.mDelegate.reset();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.SetTimeZone("America/New_York"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_118_GetSecondScreenFriendlyName_NullDelegate)
{
    plugin.mDelegate.reset();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.GetSecondScreenFriendlyName(result));
}

// ============================================================================
// SetName AND AddAdditionalInfo STATIC METHODS
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_119_SetName_ReturnsNull)
{
    string result;
    const auto rc = plugin.SetName("TestValue", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_120_AddAdditionalInfo_ReturnsNull)
{
    string result;
    const auto rc = plugin.AddAdditionalInfo("{\"key\":\"value\"}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

// ============================================================================
// HANDLE APP DELEGATE REQUEST - Error Scenarios
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_121_HandleAppDelegateRequest_NullSettingsDelegate)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;

    const auto rc = plugin.HandleAppDelegateRequest(ctx, "advertising.advertisingid", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
}

// ============================================================================
// USERSETTINGSDELEGATE STATIC HELPER FUNCTIONS
// ============================================================================

TEST(UserSettingsHelperTest, AGC_L1_122_FontFamilyToString_AllValues)
{
    // Test all FontFamily enum values
    EXPECT_STREQ("null", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CONTENT_DEFAULT));
    EXPECT_STREQ("monospaced_serif", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::MONOSPACED_SERIF));
    EXPECT_STREQ("proportional_serif", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::PROPORTIONAL_SERIF));
    EXPECT_STREQ("monospaced_sanserif", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::MONOSPACE_SANS_SERIF));
    EXPECT_STREQ("proportional_sanserif", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::PROPORTIONAL_SANS_SERIF));
    EXPECT_STREQ("casual", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CASUAL));
    EXPECT_STREQ("cursive", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CURSIVE));
    EXPECT_STREQ("smallcaps", FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily::SMALL_CAPITAL));
    
    // Test invalid/unknown value falls through to default
    EXPECT_STREQ("null", FontFamilyToString(static_cast<Exchange::ITextTrackClosedCaptionsStyle::FontFamily>(999)));
}

TEST(UserSettingsHelperTest, AGC_L1_123_FontSizeToNumber_AllValues)
{
    // Test all FontSize enum values
    EXPECT_EQ(-1, FontSizeToNumber(Exchange::ITextTrackClosedCaptionsStyle::FontSize::CONTENT_DEFAULT));
    EXPECT_EQ(0, FontSizeToNumber(Exchange::ITextTrackClosedCaptionsStyle::FontSize::SMALL));
    EXPECT_EQ(1, FontSizeToNumber(Exchange::ITextTrackClosedCaptionsStyle::FontSize::REGULAR));
    EXPECT_EQ(2, FontSizeToNumber(Exchange::ITextTrackClosedCaptionsStyle::FontSize::LARGE));
    EXPECT_EQ(3, FontSizeToNumber(Exchange::ITextTrackClosedCaptionsStyle::FontSize::EXTRA_LARGE));
    
    // Test invalid/unknown value falls through to default
    EXPECT_EQ(-1, FontSizeToNumber(static_cast<Exchange::ITextTrackClosedCaptionsStyle::FontSize>(999)));
}

TEST(UserSettingsHelperTest, AGC_L1_124_FontEdgeToString_AllValues)
{
    // Test all FontEdge enum values
    EXPECT_STREQ("null", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::CONTENT_DEFAULT));
    EXPECT_STREQ("none", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::NONE));
    EXPECT_STREQ("raised", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RAISED));
    EXPECT_STREQ("depressed", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::DEPRESSED));
    EXPECT_STREQ("uniform", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::UNIFORM));
    EXPECT_STREQ("drop_shadow_left", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::LEFT_DROP_SHADOW));
    EXPECT_STREQ("drop_shadow_right", FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RIGHT_DROP_SHADOW));
    
    // Test invalid/unknown value falls through to default
    EXPECT_STREQ("null", FontEdgeToString(static_cast<Exchange::ITextTrackClosedCaptionsStyle::FontEdge>(999)));
}

TEST(UserSettingsHelperTest, AGC_L1_125_ParseCommaSeparatedLanguages_ValidInput)
{
    JsonArray result;
    ParseCommaSeparatedLanguages("eng,fra,spa", result);
    
    EXPECT_EQ(3u, result.Length());
    
    JsonArray::Iterator it = result.Elements();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("eng", it.Current().String());
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("fra", it.Current().String());
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("spa", it.Current().String());
    EXPECT_FALSE(it.Next());
}

TEST(UserSettingsHelperTest, AGC_L1_126_ParseCommaSeparatedLanguages_WithWhitespace)
{
    JsonArray result;
    ParseCommaSeparatedLanguages("eng , fra , spa", result);
    
    EXPECT_EQ(3u, result.Length());
    
    JsonArray::Iterator it = result.Elements();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("eng", it.Current().String());
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("fra", it.Current().String());
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("spa", it.Current().String());
}

TEST(UserSettingsHelperTest, AGC_L1_127_ParseCommaSeparatedLanguages_EmptyInput)
{
    JsonArray result;
    ParseCommaSeparatedLanguages("", result);
    
    EXPECT_EQ(0u, result.Length());
}

TEST(UserSettingsHelperTest, AGC_L1_128_ParseCommaSeparatedLanguages_SingleLanguage)
{
    JsonArray result;
    ParseCommaSeparatedLanguages("eng", result);
    
    EXPECT_EQ(1u, result.Length());
    
    JsonArray::Iterator it = result.Elements();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("eng", it.Current().String());
}

TEST(UserSettingsHelperTest, AGC_L1_129_ParseCommaSeparatedLanguages_WhitespaceOnly)
{
    JsonArray result;
    ParseCommaSeparatedLanguages("   ,   ,   ", result);
    
    // All whitespace tokens should be skipped
    EXPECT_EQ(0u, result.Length());
}

TEST(UserSettingsHelperTest, AGC_L1_130_ConvertToCommaSeparatedLanguages_JsonArray)
{
    string result = ConvertToCommaSeparatedLanguages("[\"eng\",\"fra\",\"spa\"]");
    
    EXPECT_EQ("eng,fra,spa", result);
}

TEST(UserSettingsHelperTest, AGC_L1_131_ConvertToCommaSeparatedLanguages_EmptyArray)
{
    string result = ConvertToCommaSeparatedLanguages("[]");
    
    EXPECT_EQ("", result);
}

TEST(UserSettingsHelperTest, AGC_L1_132_ConvertToCommaSeparatedLanguages_QuotedString)
{
    string result = ConvertToCommaSeparatedLanguages("\"eng\"");
    
    EXPECT_EQ("eng", result);
}

TEST(UserSettingsHelperTest, AGC_L1_133_ConvertToCommaSeparatedLanguages_UnquotedString)
{
    string result = ConvertToCommaSeparatedLanguages("eng");
    
    EXPECT_EQ("eng", result);
}

TEST(UserSettingsHelperTest, AGC_L1_134_ConvertToCommaSeparatedLanguages_SingleElementArray)
{
    string result = ConvertToCommaSeparatedLanguages("[\"eng\"]");
    
    EXPECT_EQ("eng", result);
}

TEST(UserSettingsHelperTest, AGC_L1_135_BuildClosedCaptionsStyleJson_AllFieldsSet)
{
    Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle style;
    style.fontFamily = Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CASUAL;
    style.fontSize = Exchange::ITextTrackClosedCaptionsStyle::FontSize::LARGE;
    style.fontColor = "#FFFFFF";
    style.fontOpacity = 100;
    style.fontEdge = Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RAISED;
    style.fontEdgeColor = "#000000";
    style.backgroundColor = "#000000";
    style.backgroundOpacity = 80;
    style.windowColor = "#FF0000";
    style.windowOpacity = 50;
    
    JsonObject styles;
    BuildClosedCaptionsStyleJson(style, styles);
    
    EXPECT_EQ("casual", styles["fontFamily"].String());
    EXPECT_EQ(2, styles["fontSize"].Number());
    EXPECT_EQ("#FFFFFF", styles["fontColor"].String());
    EXPECT_EQ(100, styles["fontOpacity"].Number());
    EXPECT_EQ("raised", styles["fontEdge"].String());
    EXPECT_EQ("#000000", styles["fontEdgeColor"].String());
    EXPECT_EQ("#000000", styles["backgroundColor"].String());
    EXPECT_EQ(80, styles["backgroundOpacity"].Number());
    EXPECT_EQ("#FF0000", styles["windowColor"].String());
    EXPECT_EQ(50, styles["windowOpacity"].Number());
}

TEST(UserSettingsHelperTest, AGC_L1_136_BuildClosedCaptionsStyleJson_DefaultValues)
{
    Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle style;
    style.fontFamily = Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CONTENT_DEFAULT;
    style.fontSize = Exchange::ITextTrackClosedCaptionsStyle::FontSize::CONTENT_DEFAULT;
    style.fontColor = "";
    style.fontOpacity = -1;
    style.fontEdge = Exchange::ITextTrackClosedCaptionsStyle::FontEdge::CONTENT_DEFAULT;
    style.fontEdgeColor = "";
    style.backgroundColor = "";
    style.backgroundOpacity = -1;
    style.windowColor = "";
    style.windowOpacity = -1;
    
    JsonObject styles;
    BuildClosedCaptionsStyleJson(style, styles);
    
    // With default values, these fields should NOT be present in the JSON
    EXPECT_FALSE(styles.HasLabel("fontFamily"));
    EXPECT_FALSE(styles.HasLabel("fontSize"));
    EXPECT_FALSE(styles.HasLabel("fontColor"));
    EXPECT_FALSE(styles.HasLabel("fontOpacity"));
    EXPECT_FALSE(styles.HasLabel("fontEdge"));
    EXPECT_FALSE(styles.HasLabel("fontEdgeColor"));
    EXPECT_FALSE(styles.HasLabel("backgroundColor"));
    EXPECT_FALSE(styles.HasLabel("backgroundOpacity"));
    EXPECT_FALSE(styles.HasLabel("windowColor"));
    EXPECT_FALSE(styles.HasLabel("windowOpacity"));
}

TEST(UserSettingsHelperTest, AGC_L1_137_BuildClosedCaptionsStyleJson_PartialValues)
{
    Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle style;
    style.fontFamily = Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CURSIVE;
    style.fontSize = Exchange::ITextTrackClosedCaptionsStyle::FontSize::CONTENT_DEFAULT;
    style.fontColor = "#FF0000";
    style.fontOpacity = -1;
    style.fontEdge = Exchange::ITextTrackClosedCaptionsStyle::FontEdge::CONTENT_DEFAULT;
    style.fontEdgeColor = "";
    style.backgroundColor = "#0000FF";
    style.backgroundOpacity = 75;
    style.windowColor = "";
    style.windowOpacity = -1;
    
    JsonObject styles;
    BuildClosedCaptionsStyleJson(style, styles);
    
    // Present fields
    EXPECT_EQ("cursive", styles["fontFamily"].String());
    EXPECT_EQ("#FF0000", styles["fontColor"].String());
    EXPECT_EQ("#0000FF", styles["backgroundColor"].String());
    EXPECT_EQ(75, styles["backgroundOpacity"].Number());
    
    // Absent fields
    EXPECT_FALSE(styles.HasLabel("fontSize"));
    EXPECT_FALSE(styles.HasLabel("fontOpacity"));
    EXPECT_FALSE(styles.HasLabel("fontEdge"));
    EXPECT_FALSE(styles.HasLabel("fontEdgeColor"));
    EXPECT_FALSE(styles.HasLabel("windowColor"));
    EXPECT_FALSE(styles.HasLabel("windowOpacity"));
}

TEST(UserSettingsHelperTest, AGC_L1_138_BuildClosedCaptionsSettingsResponse_WithLanguages)
{
    JsonObject styles;
    styles["fontFamily"] = "casual";
    styles["fontSize"] = 2;
    
    string result = BuildClosedCaptionsSettingsResponse(true, "eng,fra", styles);
    
    JsonObject parsed;
    ASSERT_TRUE(parsed.FromString(result));
    
    EXPECT_EQ(true, parsed["enabled"].Boolean());
    EXPECT_TRUE(parsed.HasLabel("styles"));
    EXPECT_TRUE(parsed.HasLabel("preferredLanguages"));
    
    JsonArray languages = parsed["preferredLanguages"].Array();
    EXPECT_EQ(2u, languages.Length());
}

TEST(UserSettingsHelperTest, AGC_L1_139_BuildClosedCaptionsSettingsResponse_EmptyLanguages)
{
    JsonObject styles;
    
    string result = BuildClosedCaptionsSettingsResponse(false, "", styles);
    
    JsonObject parsed;
    ASSERT_TRUE(parsed.FromString(result));
    
    EXPECT_EQ(false, parsed["enabled"].Boolean());
    
    // Empty languages should default to ["eng"]
    JsonArray languages = parsed["preferredLanguages"].Array();
    EXPECT_EQ(1u, languages.Length());
    
    JsonArray::Iterator it = languages.Elements();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("eng", it.Current().String());
}

// ============================================================================
// LIFECYCLEDELEGATE STATIC HELPER FUNCTIONS
// ============================================================================

TEST(LifecycleDelegateTest, AGC_L1_140_LifecycleStateToString_AllValues)
{
    // Test all LifecycleState enum values
    EXPECT_EQ("unloaded", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::UNLOADED));
    EXPECT_EQ("loading", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::LOADING));
    EXPECT_EQ("initializing", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::INITIALIZING));
    EXPECT_EQ("paused", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::PAUSED));
    EXPECT_EQ("active", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::ACTIVE));
    EXPECT_EQ("suspended", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::SUSPENDED));
    EXPECT_EQ("hibernated", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::HIBERNATED));
    EXPECT_EQ("terminating", LifecycleDelegate::LifecycleStateToString(Exchange::ILifecycleManager::TERMINATING));
    
    // Test invalid/unknown value falls through to default
    EXPECT_EQ("", LifecycleDelegate::LifecycleStateToString(static_cast<Exchange::ILifecycleManager::LifecycleState>(999)));
}

TEST(LifecycleDelegateTest, AGC_L1_141_Lifecycle2StateToLifecycle1String_AllValues)
{
    // Test mapping from Lifecycle2 states to Lifecycle1 strings
    // UNLOADED and TERMINATING both map to "unloading"
    EXPECT_EQ("unloading", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::UNLOADED));
    EXPECT_EQ("unloading", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::TERMINATING));
    
    // LOADING and INITIALIZING both map to "initializing"
    EXPECT_EQ("initializing", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::LOADING));
    EXPECT_EQ("initializing", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::INITIALIZING));
    
    // PAUSED maps to "inactive"
    EXPECT_EQ("inactive", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::PAUSED));
    
    // ACTIVE maps to "foreground"
    EXPECT_EQ("foreground", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::ACTIVE));
    
    // SUSPENDED and HIBERNATED both map to "suspended"
    EXPECT_EQ("suspended", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::SUSPENDED));
    EXPECT_EQ("suspended", LifecycleDelegate::Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::HIBERNATED));
    
    // Test invalid/unknown value falls through to default
    EXPECT_EQ("", LifecycleDelegate::Lifecycle2StateToLifecycle1String(static_cast<Exchange::ILifecycleManager::LifecycleState>(999)));
}

// ============================================================================
// LIFECYCLEDELEGATE INNER CLASSES - AppIdInstanceIdMap
// ============================================================================

TEST(LifecycleDelegateInnerClassTest, AGC_L1_142_AppIdInstanceIdMap_AddAndGet)
{
    LifecycleDelegate::AppIdInstanceIdMap map;
    
    map.AddAppInstanceId("app1", "instance1");
    map.AddAppInstanceId("app2", "instance2");
    
    EXPECT_EQ("instance1", map.GetAppInstanceId("app1"));
    EXPECT_EQ("instance2", map.GetAppInstanceId("app2"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_143_AppIdInstanceIdMap_GetAppId_ReverseLookup)
{
    LifecycleDelegate::AppIdInstanceIdMap map;
    
    map.AddAppInstanceId("app1", "instance1");
    map.AddAppInstanceId("app2", "instance2");
    
    EXPECT_EQ("app1", map.GetAppId("instance1"));
    EXPECT_EQ("app2", map.GetAppId("instance2"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_144_AppIdInstanceIdMap_GetNonExistent)
{
    LifecycleDelegate::AppIdInstanceIdMap map;
    
    // Non-existent app should return empty string
    EXPECT_EQ("", map.GetAppInstanceId("nonexistent"));
    EXPECT_EQ("", map.GetAppId("nonexistent"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_145_AppIdInstanceIdMap_Remove)
{
    LifecycleDelegate::AppIdInstanceIdMap map;
    
    map.AddAppInstanceId("app1", "instance1");
    EXPECT_EQ("instance1", map.GetAppInstanceId("app1"));
    
    map.RemoveAppInstanceId("app1");
    EXPECT_EQ("", map.GetAppInstanceId("app1"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_146_AppIdInstanceIdMap_OverwriteExisting)
{
    LifecycleDelegate::AppIdInstanceIdMap map;
    
    map.AddAppInstanceId("app1", "instance1");
    EXPECT_EQ("instance1", map.GetAppInstanceId("app1"));
    
    // Overwrite with new instance
    map.AddAppInstanceId("app1", "instance2");
    EXPECT_EQ("instance2", map.GetAppInstanceId("app1"));
}

// ============================================================================
// LIFECYCLEDELEGATE INNER CLASSES - NavigationIntentRegistry
// ============================================================================

TEST(LifecycleDelegateInnerClassTest, AGC_L1_147_NavigationIntentRegistry_AddAndGet)
{
    LifecycleDelegate::NavigationIntentRegistry registry;
    
    registry.AddNavigationIntent("instance1", "navigate-to-home");
    registry.AddNavigationIntent("instance2", "navigate-to-settings");
    
    EXPECT_EQ("navigate-to-home", registry.GetNavigationIntent("instance1"));
    EXPECT_EQ("navigate-to-settings", registry.GetNavigationIntent("instance2"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_148_NavigationIntentRegistry_GetNonExistent)
{
    LifecycleDelegate::NavigationIntentRegistry registry;
    
    EXPECT_EQ("", registry.GetNavigationIntent("nonexistent"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_149_NavigationIntentRegistry_Remove)
{
    LifecycleDelegate::NavigationIntentRegistry registry;
    
    registry.AddNavigationIntent("instance1", "navigate-to-home");
    EXPECT_EQ("navigate-to-home", registry.GetNavigationIntent("instance1"));
    
    registry.RemoveNavigationIntent("instance1");
    EXPECT_EQ("", registry.GetNavigationIntent("instance1"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_150_NavigationIntentRegistry_OverwriteExisting)
{
    LifecycleDelegate::NavigationIntentRegistry registry;
    
    registry.AddNavigationIntent("instance1", "navigate-to-home");
    EXPECT_EQ("navigate-to-home", registry.GetNavigationIntent("instance1"));
    
    // Overwrite with new intent
    registry.AddNavigationIntent("instance1", "navigate-to-settings");
    EXPECT_EQ("navigate-to-settings", registry.GetNavigationIntent("instance1"));
}

// ============================================================================
// LIFECYCLEDELEGATE INNER CLASSES - FocusedAppRegistry
// ============================================================================

TEST(LifecycleDelegateInnerClassTest, AGC_L1_151_FocusedAppRegistry_SetAndGet)
{
    LifecycleDelegate::FocusedAppRegistry registry;
    
    registry.SetFocusedAppInstanceId("instance1");
    
    EXPECT_EQ("instance1", registry.GetFocusedAppInstanceId());
    EXPECT_TRUE(registry.IsAppInstanceIdFocused("instance1"));
    EXPECT_FALSE(registry.IsAppInstanceIdFocused("instance2"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_152_FocusedAppRegistry_Clear)
{
    LifecycleDelegate::FocusedAppRegistry registry;
    
    registry.SetFocusedAppInstanceId("instance1");
    EXPECT_TRUE(registry.IsAppInstanceIdFocused("instance1"));
    
    registry.ClearFocusedAppInstanceId();
    EXPECT_FALSE(registry.IsAppInstanceIdFocused("instance1"));
    EXPECT_EQ("", registry.GetFocusedAppInstanceId());
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_153_FocusedAppRegistry_GetFocusedEventData_Focused)
{
    LifecycleDelegate::FocusedAppRegistry registry;
    
    registry.SetFocusedAppInstanceId("instance1");
    
    string eventData = registry.GetFocusedEventData("instance1");
    
    JsonObject parsed;
    ASSERT_TRUE(parsed.FromString(eventData));
    EXPECT_TRUE(parsed["value"].Boolean());
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_154_FocusedAppRegistry_GetFocusedEventData_NotFocused)
{
    LifecycleDelegate::FocusedAppRegistry registry;
    
    registry.SetFocusedAppInstanceId("instance1");
    
    // Get event data for a different instance (not focused)
    string eventData = registry.GetFocusedEventData("instance2");
    
    JsonObject parsed;
    ASSERT_TRUE(parsed.FromString(eventData));
    EXPECT_FALSE(parsed["value"].Boolean());
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_155_FocusedAppRegistry_ChangeFocus)
{
    LifecycleDelegate::FocusedAppRegistry registry;
    
    registry.SetFocusedAppInstanceId("instance1");
    EXPECT_TRUE(registry.IsAppInstanceIdFocused("instance1"));
    
    // Change focus to another instance
    registry.SetFocusedAppInstanceId("instance2");
    EXPECT_FALSE(registry.IsAppInstanceIdFocused("instance1"));
    EXPECT_TRUE(registry.IsAppInstanceIdFocused("instance2"));
}

// ============================================================================
// LIFECYCLEDELEGATE INNER CLASSES - LifecycleStateRegistry
// ============================================================================

TEST(LifecycleDelegateInnerClassTest, AGC_L1_156_LifecycleStateRegistry_AddAndGetState)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    registry.AddLifecycleState("instance1", Exchange::ILifecycleManager::UNLOADED, Exchange::ILifecycleManager::INITIALIZING);
    
    LifecycleDelegate::LifecycleStateInfo info = registry.GetLifecycleStateInfo("instance1");
    
    EXPECT_EQ(Exchange::ILifecycleManager::UNLOADED, info.previousState);
    EXPECT_EQ(Exchange::ILifecycleManager::INITIALIZING, info.currentState);
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_157_LifecycleStateRegistry_UpdateState)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    registry.AddLifecycleState("instance1", Exchange::ILifecycleManager::UNLOADED, Exchange::ILifecycleManager::INITIALIZING);
    
    // Update to new state
    registry.UpdateLifecycleState("instance1", Exchange::ILifecycleManager::ACTIVE);
    
    LifecycleDelegate::LifecycleStateInfo info = registry.GetLifecycleStateInfo("instance1");
    
    // Previous state should now be INITIALIZING, current should be ACTIVE
    EXPECT_EQ(Exchange::ILifecycleManager::INITIALIZING, info.previousState);
    EXPECT_EQ(Exchange::ILifecycleManager::ACTIVE, info.currentState);
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_158_LifecycleStateRegistry_IsAppLifecycleActive)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    registry.AddLifecycleState("instance1", Exchange::ILifecycleManager::INITIALIZING, Exchange::ILifecycleManager::ACTIVE);
    registry.AddLifecycleState("instance2", Exchange::ILifecycleManager::ACTIVE, Exchange::ILifecycleManager::PAUSED);
    
    EXPECT_TRUE(registry.IsAppLifecycleActive("instance1"));
    EXPECT_FALSE(registry.IsAppLifecycleActive("instance2"));
    EXPECT_FALSE(registry.IsAppLifecycleActive("nonexistent"));
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_159_LifecycleStateRegistry_GetNonExistent)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    // Non-existent app should return default state (UNLOADED, UNLOADED)
    LifecycleDelegate::LifecycleStateInfo info = registry.GetLifecycleStateInfo("nonexistent");
    
    EXPECT_EQ(Exchange::ILifecycleManager::UNLOADED, info.previousState);
    EXPECT_EQ(Exchange::ILifecycleManager::UNLOADED, info.currentState);
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_160_LifecycleStateRegistry_Remove)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    registry.AddLifecycleState("instance1", Exchange::ILifecycleManager::UNLOADED, Exchange::ILifecycleManager::ACTIVE);
    EXPECT_TRUE(registry.IsAppLifecycleActive("instance1"));
    
    registry.RemoveLifecycleStateInfo("instance1");
    
    // After removal, should return default (UNLOADED, UNLOADED)
    LifecycleDelegate::LifecycleStateInfo info = registry.GetLifecycleStateInfo("instance1");
    EXPECT_EQ(Exchange::ILifecycleManager::UNLOADED, info.previousState);
    EXPECT_EQ(Exchange::ILifecycleManager::UNLOADED, info.currentState);
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_161_LifecycleStateRegistry_GetLifecycle1StateJson)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    registry.AddLifecycleState("instance1", Exchange::ILifecycleManager::INITIALIZING, Exchange::ILifecycleManager::ACTIVE);
    
    string jsonState = registry.GetLifecycle1StateJson("instance1");
    
    JsonObject parsed;
    ASSERT_TRUE(parsed.FromString(jsonState));
    
    // INITIALIZING maps to "initializing", ACTIVE maps to "foreground"
    EXPECT_EQ("initializing", parsed["previous"].String());
    EXPECT_EQ("foreground", parsed["state"].String());
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_162_LifecycleStateRegistry_GetLifecycle2StateJson)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    registry.AddLifecycleState("instance1", Exchange::ILifecycleManager::INITIALIZING, Exchange::ILifecycleManager::ACTIVE);
    
    string jsonState = registry.GetLifecycle2StateJson("instance1");
    
    JsonObject parsed;
    ASSERT_TRUE(parsed.FromString(jsonState));
    
    EXPECT_EQ("initializing", parsed["oldState"].String());
    EXPECT_EQ("active", parsed["newState"].String());
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_163_LifecycleStateRegistry_GetLifecycle1StateJson_NonExistent)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    string jsonState = registry.GetLifecycle1StateJson("nonexistent");
    
    EXPECT_EQ("{}", jsonState);
}

TEST(LifecycleDelegateInnerClassTest, AGC_L1_164_LifecycleStateRegistry_GetLifecycle2StateJson_NonExistent)
{
    LifecycleDelegate::LifecycleStateRegistry registry;
    
    string jsonState = registry.GetLifecycle2StateJson("nonexistent");
    
    EXPECT_EQ("{}", jsonState);
}

// ============================================================================
// APPGATEWAYCOMMON.H - Information() method
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_165_Information_ReturnsEmptyString)
{
    // Information() method should return an empty string
    string info = plugin.Information();
    EXPECT_EQ("", info);
}

// ============================================================================
// APPGATEWAYCOMMON.H - EventRegistrationJob inner class
// ============================================================================

// Mock emitter for testing EventRegistrationJob
class MockEmitter : public Exchange::IAppNotificationHandler::IEmitter {
public:
    MockEmitter() : mRefCount(1) {}
    
    void Emit(const string& event, const string& payload, const string& appId) override {
        mLastEvent = event;
        mLastPayload = payload;
        mLastAppId = appId;
    }
    
    void AddRef() const override {
        ++mRefCount;
    }
    
    uint32_t Release() const override {
        if (--mRefCount == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }
    
    BEGIN_INTERFACE_MAP(MockEmitter)
    INTERFACE_ENTRY(Exchange::IAppNotificationHandler::IEmitter)
    END_INTERFACE_MAP
    
    string mLastEvent;
    string mLastPayload;
    string mLastAppId;
    mutable uint32_t mRefCount;
};

TEST_F(AppGatewayCommonTest, AGC_L1_166_EventRegistrationJob_Create)
{
    MockEmitter* emitter = new MockEmitter();
    
    // Create an EventRegistrationJob
    Core::ProxyType<Core::IDispatch> job = AppGatewayCommon::EventRegistrationJob::Create(
        &plugin, emitter, "test.event", true);
    
    EXPECT_TRUE(job.IsValid());
    
    // The emitter should have been AddRef'd by the job
    EXPECT_EQ(2u, emitter->mRefCount);
    
    // Release the job - this should release the emitter reference
    job.Release();
    
    // Clean up the emitter
    emitter->Release();
}

TEST_F(AppGatewayCommonTest, AGC_L1_167_EventRegistrationJob_CreateWithNullCallback)
{
    // Create an EventRegistrationJob with null callback
    Core::ProxyType<Core::IDispatch> job = AppGatewayCommon::EventRegistrationJob::Create(
        &plugin, nullptr, "test.event", false);
    
    EXPECT_TRUE(job.IsValid());
    
    // Release the job
    job.Release();
}

TEST_F(AppGatewayCommonTest, AGC_L1_168_EventRegistrationJob_Dispatch_WithDelegate)
{
    // Initialize the plugin first so mDelegate is set
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(1).WillOnce(Return(Core::ERROR_NONE));

    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    
    Core::ProxyType<Core::IDispatch> job = AppGatewayCommon::EventRegistrationJob::Create(
        &plugin, emitter, "localization.onlanguagechanged", true);
    
    EXPECT_TRUE(job.IsValid());
    
    // Dispatch should call HandleAppEventNotifier on the delegate
    // This will exercise the Dispatch method
    job->Dispatch();
    
    // Wait for async job to complete before cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    job.Release();
    emitter->Release();
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_169_EventRegistrationJob_Dispatch_UnsubscribeListen)
{
    // Initialize the plugin first so mDelegate is set
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(1).WillOnce(Return(Core::ERROR_NONE));

    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    
    // Test with listen=false (unsubscribe)
    Core::ProxyType<Core::IDispatch> job = AppGatewayCommon::EventRegistrationJob::Create(
        &plugin, emitter, "localization.onlanguagechanged", false);
    
    EXPECT_TRUE(job.IsValid());
    
    job->Dispatch();
    
    job.Release();
    emitter->Release();
    
    plugin.Deinitialize(&service);
}

// ============================================================================
// APPGATEWAYCOMMON.H - QueryInterface (via INTERFACE_MAP macros)
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_170_QueryInterface_IPlugin)
{
    void* result = plugin.QueryInterface(PluginHost::IPlugin::ID);
    EXPECT_NE(nullptr, result);
    if (result) {
        static_cast<PluginHost::IPlugin*>(result)->Release();
    }
}

TEST_F(AppGatewayCommonTest, AGC_L1_171_QueryInterface_IAppGatewayRequestHandler)
{
    void* result = plugin.QueryInterface(Exchange::IAppGatewayRequestHandler::ID);
    EXPECT_NE(nullptr, result);
    if (result) {
        static_cast<Exchange::IAppGatewayRequestHandler*>(result)->Release();
    }
}

TEST_F(AppGatewayCommonTest, AGC_L1_172_QueryInterface_IAppNotificationHandler)
{
    void* result = plugin.QueryInterface(Exchange::IAppNotificationHandler::ID);
    EXPECT_NE(nullptr, result);
    if (result) {
        static_cast<Exchange::IAppNotificationHandler*>(result)->Release();
    }
}

TEST_F(AppGatewayCommonTest, AGC_L1_173_QueryInterface_IAppGatewayAuthenticator)
{
    void* result = plugin.QueryInterface(Exchange::IAppGatewayAuthenticator::ID);
    EXPECT_NE(nullptr, result);
    if (result) {
        static_cast<Exchange::IAppGatewayAuthenticator*>(result)->Release();
    }
}

TEST_F(AppGatewayCommonTest, AGC_L1_174_QueryInterface_UnknownInterface)
{
    // Query for an unknown interface ID should return nullptr
    void* result = plugin.QueryInterface(0xDEADBEEF);
    EXPECT_EQ(nullptr, result);
}

// ============================================================================
// APPGATEWAYCOMMON.CPP - Deactivated() method
// ============================================================================

// Mock IRemoteConnection for testing Deactivated
class MockRemoteConnection : public RPC::IRemoteConnection {
public:
    MockRemoteConnection(uint32_t id) : mId(id), mRefCount(1) {}
    
    uint32_t Id() const override { return mId; }
    uint32_t RemoteId() const override { return 0; }
    void* Acquire(const uint32_t waitTime, const string& className, const uint32_t interfaceId, const uint32_t version) override {
        (void)waitTime; (void)className; (void)interfaceId; (void)version;
        return nullptr;
    }
    void Terminate() override {}
    uint32_t Launch() override { return Core::ERROR_NONE; }
    void PostMortem() override {}
    
    void AddRef() const override { ++mRefCount; }
    uint32_t Release() const override {
        if (--mRefCount == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }
    
    BEGIN_INTERFACE_MAP(MockRemoteConnection)
    INTERFACE_ENTRY(RPC::IRemoteConnection)
    END_INTERFACE_MAP
    
private:
    uint32_t mId;
    mutable uint32_t mRefCount;
};

TEST_F(AppGatewayCommonTest, AGC_L1_175_Deactivated_MatchingConnectionId)
{
    // Initialize the plugin first to set up mShell (required by Deactivated's ASSERT)
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string response = plugin.Initialize(&service);
    EXPECT_TRUE(response.empty());
    
    // Set the plugin's mConnectionId to a known value
    plugin.mConnectionId = 12345;
    
    // Create a mock connection with the same ID
    MockRemoteConnection* connection = new MockRemoteConnection(12345);
    
    // Call Deactivated - this should trigger the deactivation logic
    // which submits a job to the worker pool
    plugin.Deactivated(connection);
    
    // Give time for the async job to potentially execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    connection->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_176_Deactivated_DifferentConnectionId)
{
    // Set the plugin's mConnectionId to a known value
    plugin.mConnectionId = 12345;
    
    // Create a mock connection with a different ID
    MockRemoteConnection* connection = new MockRemoteConnection(99999);
    
    // Call Deactivated with non-matching ID - should not trigger deactivation
    plugin.Deactivated(connection);
    
    connection->Release();
}

// ============================================================================
// APPGATEWAYCOMMON.CPP - HandleAppEventNotifier() method
// ============================================================================

// Note: These tests verify that HandleAppEventNotifier submits an EventRegistrationJob
// to the worker pool. The job's Dispatch() method accesses mDelegate, which is set up
// during Initialize(). Tests that don't initialize the plugin will cause segfaults
// when the async job runs. We initialize the plugin to avoid this.

TEST_F(AppGatewayCommonTest, AGC_L1_177_HandleAppEventNotifier_Subscribe)
{
    // Initialize the plugin first to set up mDelegate
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string response = plugin.Initialize(&service);
    EXPECT_TRUE(response.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Test subscribing to an event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "localization.onlanguagechanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Give time for the async job to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_178_HandleAppEventNotifier_Unsubscribe)
{
    // Initialize the plugin first to set up mDelegate
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string response = plugin.Initialize(&service);
    EXPECT_TRUE(response.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Test unsubscribing from an event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "localization.onlanguagechanged", false, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Give time for the async job to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_179_HandleAppEventNotifier_NullEmitter)
{
    // Initialize the plugin first to set up mDelegate
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string response = plugin.Initialize(&service);
    EXPECT_TRUE(response.empty());
    
    bool status = false;
    
    // Test with null emitter
    Core::hresult rc = plugin.HandleAppEventNotifier(nullptr, "test.event", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Give time for the async job to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_180_HandleAppEventNotifier_MultipleEvents)
{
    // Initialize the plugin first to set up mDelegate
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string response = plugin.Initialize(&service);
    EXPECT_TRUE(response.empty());
    
    MockEmitter* emitter1 = new MockEmitter();
    MockEmitter* emitter2 = new MockEmitter();
    bool status1 = false;
    bool status2 = false;
    
    // Subscribe to multiple events
    Core::hresult rc1 = plugin.HandleAppEventNotifier(emitter1, "localization.onlanguagechanged", true, status1);
    Core::hresult rc2 = plugin.HandleAppEventNotifier(emitter2, "accessibility.onvoiceguidancesettingschanged", true, status2);
    
    EXPECT_EQ(Core::ERROR_NONE, rc1);
    EXPECT_EQ(Core::ERROR_NONE, rc2);
    EXPECT_TRUE(status1);
    EXPECT_TRUE(status2);
    
    // Give time for the async jobs to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter1->Release();
    emitter2->Release();
    plugin.Deinitialize(&service);
}

// ============================================================================
// USERSETTINGSDELEGATE - Tests with mocked IUserSettings COM interface
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_181_UserSettings_GetVoiceGuidance_Success)
{
    // Create mock IUserSettings
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    // Set up the mock to return success with enabled=true
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    // Create service mock that returns our mock UserSettings
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_182_UserSettings_GetVoiceGuidance_Disabled)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = false;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("false", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_183_UserSettings_SetVoiceGuidance_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidance(true))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":true}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_184_UserSettings_GetAudioDescription_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetAudioDescription(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.enabled", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_185_UserSettings_SetAudioDescription_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetAudioDescription(true))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", "{\"value\":true}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_186_UserSettings_GetCaptions_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.enabled", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_187_UserSettings_SetCaptions_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetCaptions(true))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{\"value\":true}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_188_UserSettings_GetHighContrast_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetHighContrast(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_189_UserSettings_GetPresentationLanguage_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce([](string& lang) {
            lang = "en-US";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("\"en\"", result);  // Should extract "en" from "en-US"
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_190_UserSettings_GetLocale_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce([](string& lang) {
            lang = "en-US";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.locale", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("\"en-US\"", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_191_UserSettings_SetLocale_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setlocale", "{\"value\":\"en-US\"}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_192_UserSettings_GetPreferredAudioLanguages_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetPreferredAudioLanguages(_))
        .WillOnce([](string& langs) {
            langs = "eng,fra,spa";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.preferredaudiolanguages", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("[\"eng\",\"fra\",\"spa\"]", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_193_UserSettings_SetPreferredAudioLanguages_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", "{\"value\":[\"eng\",\"fra\"]}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_194_UserSettings_SetPreferredCaptionsLanguages_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", "{\"value\":[\"eng\",\"fra\"]}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_195_UserSettings_SetVoiceGuidanceRate_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":1.5}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_196_UserSettings_SetVoiceGuidanceHints_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceHints(true))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", "{\"value\":true}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_197_UserSettings_GetVoiceGuidanceHints_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce([](bool& hints) {
            hints = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.navigationhints", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// ============================================================================
// NETWORKDELEGATE - Tests with mocked INetworkManager COM interface
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_198_Network_GetNetworkConnected_HasInterface)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, GetPrimaryInterface(_))
        .WillOnce([](string& iface) {
            iface = "eth0";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_199_Network_GetNetworkConnected_NoInterface)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, GetPrimaryInterface(_))
        .WillOnce([](string& iface) {
            iface = "";  // Empty means not connected
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("false", result);
    
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_200_Network_GetInternetConnectionStatus_Ethernet)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    NiceMock<Exchange::MockInterfaceDetailsIterator>* mockIterator = new NiceMock<Exchange::MockInterfaceDetailsIterator>();
    
    // Set up iterator to return one connected ethernet interface
    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce([](Exchange::INetworkManager::InterfaceDetails& iface) {
            iface.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
            iface.name = "eth0";
            iface.connected = true;
            return true;
        })
        .WillRepeatedly(Return(false));
    
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(_))
        .WillOnce([mockIterator](Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces) {
            mockIterator->AddRef();
            interfaces = mockIterator;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("\"type\":\"ethernet\"") != string::npos);
    EXPECT_TRUE(result.find("\"state\":\"connected\"") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockIterator;
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_201_Network_GetInternetConnectionStatus_WiFi)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    NiceMock<Exchange::MockInterfaceDetailsIterator>* mockIterator = new NiceMock<Exchange::MockInterfaceDetailsIterator>();
    
    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce([](Exchange::INetworkManager::InterfaceDetails& iface) {
            iface.type = Exchange::INetworkManager::INTERFACE_TYPE_WIFI;
            iface.name = "wlan0";
            iface.connected = true;
            return true;
        })
        .WillRepeatedly(Return(false));
    
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(_))
        .WillOnce([mockIterator](Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces) {
            mockIterator->AddRef();
            interfaces = mockIterator;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("\"type\":\"wifi\"") != string::npos);
    EXPECT_TRUE(result.find("\"state\":\"connected\"") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockIterator;
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_202_Network_GetInternetConnectionStatus_NoConnectedInterface)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    NiceMock<Exchange::MockInterfaceDetailsIterator>* mockIterator = new NiceMock<Exchange::MockInterfaceDetailsIterator>();
    
    // No connected interfaces
    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce([](Exchange::INetworkManager::InterfaceDetails& iface) {
            iface.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
            iface.name = "eth0";
            iface.connected = false;  // Not connected
            return true;
        })
        .WillRepeatedly(Return(false));
    
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(_))
        .WillOnce([mockIterator](Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces) {
            mockIterator->AddRef();
            interfaces = mockIterator;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{}", result);  // Empty object when no connected interface
    
    plugin.Deinitialize(&service);
    delete mockIterator;
    delete mockNetworkManager;
}

// ============================================================================
// APPDELEGATE - Tests with mocked ISharedStorage COM interface
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_203_App_GetDeviceUID_ExistingValue)
{
    NiceMock<Exchange::MockISharedStorage>* mockSharedStorage = new NiceMock<Exchange::MockISharedStorage>();
    
    // GetValue returns success with existing UID
    EXPECT_CALL(*mockSharedStorage, GetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce([](Exchange::ISharedStorage::ScopeType, const string&, const string& key, string& value, uint32_t& ttl, bool& success) {
            if (key == "fireboltDeviceUid") {
                value = "existing-uid-12345";
                ttl = 0;
                success = true;
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockSharedStorage](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.SharedStorage") {
                mockSharedStorage->AddRef();
                return static_cast<void*>(mockSharedStorage);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("existing-uid-12345", result);
    
    plugin.Deinitialize(&service);
    delete mockSharedStorage;
}

TEST_F(AppGatewayCommonTest, AGC_L1_204_App_GetDeviceUID_NewValue)
{
    NiceMock<Exchange::MockISharedStorage>* mockSharedStorage = new NiceMock<Exchange::MockISharedStorage>();
    
    // GetValue returns error (no existing value), then SetValue is called
    EXPECT_CALL(*mockSharedStorage, GetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));  // No existing value
    
    EXPECT_CALL(*mockSharedStorage, SetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce([](Exchange::ISharedStorage::ScopeType, const string&, const string&, const string&, uint32_t, Exchange::ISharedStorage::Success& success) {
            success.success = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockSharedStorage](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.SharedStorage") {
                mockSharedStorage->AddRef();
                return static_cast<void*>(mockSharedStorage);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Result should be a UUID (36 chars with hyphens)
    EXPECT_EQ(36u, result.length());
    
    plugin.Deinitialize(&service);
    delete mockSharedStorage;
}

TEST_F(AppGatewayCommonTest, AGC_L1_205_App_GetAdvertisingId_ExistingValue)
{
    NiceMock<Exchange::MockISharedStorage>* mockSharedStorage = new NiceMock<Exchange::MockISharedStorage>();
    
    EXPECT_CALL(*mockSharedStorage, GetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce([](Exchange::ISharedStorage::ScopeType, const string&, const string& key, string& value, uint32_t& ttl, bool& success) {
            if (key == "fireboltAdvertisingId") {
                value = "ad-id-12345";
                ttl = 0;
                success = true;
                return Core::ERROR_NONE;
            }
            return Core::ERROR_GENERAL;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockSharedStorage](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.SharedStorage") {
                mockSharedStorage->AddRef();
                return static_cast<void*>(mockSharedStorage);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("\"ifa\":\"ad-id-12345\"") != string::npos);
    EXPECT_TRUE(result.find("\"ifa_type\":\"sessionid\"") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockSharedStorage;
}

TEST_F(AppGatewayCommonTest, AGC_L1_206_App_GetAdvertisingId_NewValue)
{
    NiceMock<Exchange::MockISharedStorage>* mockSharedStorage = new NiceMock<Exchange::MockISharedStorage>();
    
    EXPECT_CALL(*mockSharedStorage, GetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));  // No existing value
    
    EXPECT_CALL(*mockSharedStorage, SetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce([](Exchange::ISharedStorage::ScopeType, const string&, const string&, const string&, uint32_t, Exchange::ISharedStorage::Success& success) {
            success.success = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockSharedStorage](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.SharedStorage") {
                mockSharedStorage->AddRef();
                return static_cast<void*>(mockSharedStorage);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("\"ifa\":") != string::npos);
    EXPECT_TRUE(result.find("\"ifa_type\":\"sessionid\"") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockSharedStorage;
}

// ============================================================================
// TTSDELEGATE - Tests with mocked ITextToSpeech COM interface
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_207_TTS_HandleSubscription_Subscribe)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to a TTS event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "TextToSpeech.onVoiceChanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockTTS;
}

TEST_F(AppGatewayCommonTest, AGC_L1_208_TTS_HandleSubscription_Unsubscribe)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Unsubscribe from a TTS event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "TextToSpeech.onSpeechComplete", false, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockTTS;
}

// ============================================================================
// USERSETTINGSDELEGATE - Additional error path tests
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_209_UserSettings_GetVoiceGuidance_COMError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_210_UserSettings_SetVoiceGuidance_COMError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":true}", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// ============================================================================
// NETWORKDELEGATE - Event subscription tests
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_211_Network_HandleSubscription_Subscribe)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to a network event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "device.onnetworkchanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_212_Network_HandleSubscription_NetworkConnectedChanged)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to network.onconnectedchanged event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "network.onconnectedchanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

// ============================================================================
// USERSETTINGSDELEGATE - Event subscription tests
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_213_UserSettings_HandleSubscription_LocalizationEvents)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to localization.onlocalechanged event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "localization.onlocalechanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_214_UserSettings_HandleSubscription_AccessibilityEvents)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to accessibility.onaudiodescriptionsettingschanged event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "accessibility.onaudiodescriptionsettingschanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_215_UserSettings_HandleSubscription_ClosedCaptionsEvents)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to closedcaptions.onenabledchanged event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "closedcaptions.onenabledchanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// ============================================================================
// APPDELEGATE - Error handling tests
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_216_App_GetDeviceUID_SetValueFails)
{
    NiceMock<Exchange::MockISharedStorage>* mockSharedStorage = new NiceMock<Exchange::MockISharedStorage>();
    
    EXPECT_CALL(*mockSharedStorage, GetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));  // No existing value
    
    EXPECT_CALL(*mockSharedStorage, SetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce([](Exchange::ISharedStorage::ScopeType, const string&, const string&, const string&, uint32_t, Exchange::ISharedStorage::Success& success) {
            success.success = false;  // SetValue fails
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockSharedStorage](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.SharedStorage") {
                mockSharedStorage->AddRef();
                return static_cast<void*>(mockSharedStorage);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result);
    
    // When SetValue fails, the code returns ERROR_GENERAL and populates result with error message
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    // The error message is a JSON-RPC formatted error with "code" and "Text" fields
    EXPECT_TRUE(result.find("Failed to set") != string::npos || result.find("code") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockSharedStorage;
}

TEST_F(AppGatewayCommonTest, AGC_L1_217_App_GetAdvertisingId_SetValueFails)
{
    NiceMock<Exchange::MockISharedStorage>* mockSharedStorage = new NiceMock<Exchange::MockISharedStorage>();
    
    EXPECT_CALL(*mockSharedStorage, GetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));  // No existing value
    
    EXPECT_CALL(*mockSharedStorage, SetValue(Exchange::ISharedStorage::DEVICE, _, _, _, _, _))
        .WillOnce([](Exchange::ISharedStorage::ScopeType, const string&, const string&, const string&, uint32_t, Exchange::ISharedStorage::Success& success) {
            success.success = false;  // SetValue fails
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockSharedStorage](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.SharedStorage") {
                mockSharedStorage->AddRef();
                return static_cast<void*>(mockSharedStorage);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result);
    
    // When SetValue fails, the code returns ERROR_GENERAL and populates result with error message
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    // The error message is a JSON-RPC formatted error with "code" and "Text" fields
    EXPECT_TRUE(result.find("Failed to set") != string::npos || result.find("code") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockSharedStorage;
}

// ============================================================================
// NETWORKDELEGATE - GetPrimaryInterface error handling
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_218_Network_GetNetworkConnected_COMError)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, GetPrimaryInterface(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    // ErrorUtils::CustomInternal returns a JSONRPC error message containing the custom text
    EXPECT_TRUE(result.find("Failed to get NetworkInfo") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_219_Network_GetAvailableInterfaces_COMError)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_312_Network_GetInternetConnectionStatus_NullIterator)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    // Return success but with null iterator
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(_))
        .WillOnce([](Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces) {
            interfaces = nullptr;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{}", result);  // Empty object when iterator is null
    
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_313_Network_GetInternetConnectionStatus_UnknownInterfaceType)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    NiceMock<Exchange::MockInterfaceDetailsIterator>* mockIterator = new NiceMock<Exchange::MockInterfaceDetailsIterator>();
    
    // Return an interface with unknown type
    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce([](Exchange::INetworkManager::InterfaceDetails& iface) {
            iface.type = static_cast<Exchange::INetworkManager::InterfaceType>(99);  // Unknown type
            iface.name = "unknown0";
            iface.connected = true;
            return true;
        })
        .WillRepeatedly(Return(false));
    
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(_))
        .WillOnce([mockIterator](Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces) {
            mockIterator->AddRef();
            interfaces = mockIterator;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("\"type\":\"unknown\"") != string::npos);
    EXPECT_TRUE(result.find("\"state\":\"connected\"") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockIterator;
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_314_Network_HandleEvent_UnrecognizedEvent)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Try to subscribe to an unrecognized network event
    // Note: HandleAppEventNotifier submits an async job and always sets status=true synchronously
    // The actual event validation happens asynchronously in the worker pool
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "network.unrecognizedevent", true, status);
    
    // The synchronous call always returns success and sets status=true
    // Event validation happens asynchronously
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Wait for the async job to complete before cleanup to avoid segfault
    // Use longer delay for CI environments which may be slower
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_315_Network_HandleSubscription_Unsubscribe)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Subscribe to a network event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "device.onnetworkchanged", true, status);
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Now unsubscribe
    rc = plugin.HandleAppEventNotifier(emitter, "device.onnetworkchanged", false, status);
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_316_Network_HandleSubscription_AlreadyRegistered)
{
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    
    EXPECT_CALL(*mockNetworkManager, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));  // Only called once
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter1 = new MockEmitter();
    MockEmitter* emitter2 = new MockEmitter();
    bool status = false;
    
    // First subscription - should register
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter1, "device.onnetworkchanged", true, status);
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Second subscription - should NOT register again (already registered)
    rc = plugin.HandleAppEventNotifier(emitter2, "network.onconnectedchanged", true, status);
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter1->Release();
    emitter2->Release();
    plugin.Deinitialize(&service);
    delete mockNetworkManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_317_Network_GetNetworkConnected_NullNetworkManager)
{
    // Don't inject NetworkManager - should return error
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_318_Network_GetInternetConnectionStatus_NullNetworkManager)
{
    // Don't inject NetworkManager - should return error
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_319_Network_HandleSubscription_NoNetworkManager)
{
    // Test the error path in HandleSubscription when NetworkManager is not available
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Access the NetworkDelegate and call HandleSubscription directly
    // This tests line 75-76 when networkManager is nullptr
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Try to subscribe to a valid network event - should fail because NetworkManager is null
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "device.onnetworkchanged", true, status);
    
    // The async job will fail, but the synchronous call returns success
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    // Wait for async job to complete before cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_320_Network_NotificationHandler_OnInterfaceStateChange)
{
    // Test the notification callback methods in NetworkNotificationHandler
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    // Allow leak since mock lifecycle is managed by COM reference counting, not gtest
    testing::Mock::AllowLeak(mockNetworkManager);
    
    EXPECT_CALL(*mockNetworkManager, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Subscribe to a network event to trigger registration
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "device.onnetworkchanged", true, status);
    
    // Give the async job time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Access mDelegate through plugin, then get NetworkDelegate
    auto networkDelegate = plugin.mDelegate->getNetworkDelegate();
    if (networkDelegate) {
        // Access the notification handler through the NetworkDelegate
        auto& handler = networkDelegate->mNotificationHandler;
        
        // Test onInterfaceStateChange
        handler.onInterfaceStateChange(Exchange::INetworkManager::INTERFACE_LINK_UP, "eth0");
        
        // Test onActiveInterfaceChange  
        handler.onActiveInterfaceChange("", "eth0");
        
        // Test onIPAddressChange
        handler.onIPAddressChange("eth0", "ipv4", "192.168.1.100", Exchange::INetworkManager::IP_ACQUIRED);
        
        // Test onAvailableSSIDs
        handler.onAvailableSSIDs("{\"ssids\":[]}");
        
        // Test onWiFiStateChange
        handler.onWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
        
        // Test onWiFiSignalQualityChange
        handler.onWiFiSignalQualityChange("TestSSID", "80", "10", "70", Exchange::INetworkManager::WIFI_SIGNAL_EXCELLENT);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
    // Note: mockNetworkManager is cleaned up via Release() calls from the delegate
}

TEST_F(AppGatewayCommonTest, AGC_L1_321_Network_NotificationHandler_OnInternetStatusChange)
{
    // Test onInternetStatusChange notification callback
    NiceMock<Exchange::MockINetworkManager>* mockNetworkManager = new NiceMock<Exchange::MockINetworkManager>();
    // Allow leak since mock lifecycle is managed by COM reference counting, not gtest
    testing::Mock::AllowLeak(mockNetworkManager);
    
    EXPECT_CALL(*mockNetworkManager, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockNetworkManager](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.NetworkManager") {
                mockNetworkManager->AddRef();
                return static_cast<void*>(mockNetworkManager);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Subscribe to get the handler registered
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "device.onnetworkchanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto networkDelegate = plugin.mDelegate->getNetworkDelegate();
    if (networkDelegate) {
        auto& handler = networkDelegate->mNotificationHandler;
        
        // Test all internet status combinations
        handler.onInternetStatusChange(
            Exchange::INetworkManager::INTERNET_NOT_AVAILABLE,
            Exchange::INetworkManager::INTERNET_FULLY_CONNECTED,
            "eth0");
        
        handler.onInternetStatusChange(
            Exchange::INetworkManager::INTERNET_FULLY_CONNECTED,
            Exchange::INetworkManager::INTERNET_CAPTIVE_PORTAL,
            "wlan0");
        
        handler.onInternetStatusChange(
            Exchange::INetworkManager::INTERNET_CAPTIVE_PORTAL,
            Exchange::INetworkManager::INTERNET_LIMITED,
            "eth0");
        
        handler.onInternetStatusChange(
            Exchange::INetworkManager::INTERNET_LIMITED,
            Exchange::INetworkManager::INTERNET_NOT_AVAILABLE,
            "eth0");
        
        // Test with default/unknown status
        handler.onInternetStatusChange(
            static_cast<Exchange::INetworkManager::InternetStatus>(99),
            static_cast<Exchange::INetworkManager::InternetStatus>(100),
            "eth0");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
    // Note: mockNetworkManager is cleaned up via Release() calls from the delegate
}

// ============================================================================
// USERSETTINGSDELEGATE - Additional method tests
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_220_UserSettings_GetAudioDescriptionSettings_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetAudioDescription(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescriptionsettings", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("\"enabled\":true") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// ============================================================================
// Additional UserSettingsDelegate tests for improved coverage
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_221_UserSettings_GetPreferredAudioLanguages_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetPreferredAudioLanguages(_))
        .WillOnce([](string& languages) {
            languages = "eng,fra,spa";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.preferredaudiolanguages", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Result should contain the languages as a JSON array
    EXPECT_TRUE(result.find("eng") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_222_UserSettings_SetPreferredAudioLanguages_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", "{\"value\":[\"eng\",\"fra\"]}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_223_UserSettings_GetHighContrast_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetHighContrast(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    // Use correct method name: accessibility.highcontrastui
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Note: AGC_L1_224 and AGC_L1_226 removed - no set methods for highcontrast and setlanguage in AppGatewayCommon

TEST_F(AppGatewayCommonTest, AGC_L1_225_UserSettings_GetPresentationLanguage_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce([](string& language) {
            language = "en-US";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("en-US") != string::npos || result.find("en") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// AGC_L1_226 removed - no localization.setlanguage method in AppGatewayCommon

TEST_F(AppGatewayCommonTest, AGC_L1_227_UserSettings_GetCaptions_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.enabled", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_228_UserSettings_SetCaptions_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetCaptions(true))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{\"value\":true}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_229_UserSettings_GetVoiceGuidanceRate_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.5;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("1.5") != string::npos || result.find("1.") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

TEST_F(AppGatewayCommonTest, AGC_L1_230_UserSettings_SetVoiceGuidanceRate_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":2.0}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// AGC_L1_231 removed - no voiceguidance.hints method in AppGatewayCommon

// ============================================================================
// LifecycleDelegate Tests with Mocked ILifecycleManagerState
// These tests directly manipulate the LifecycleDelegate to inject mocked interfaces
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_232_LifecycleClose_UserExit_Success)
{
    // Create mock for ILifecycleManagerState
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    // Expect CloseApp to be called with USER_EXIT reason
    EXPECT_CALL(*mockLifecycleManager, CloseApp(_, Exchange::ILifecycleManagerState::USER_EXIT))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Directly inject the mock into the LifecycleDelegate
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    // Clear the mock pointer before cleanup to avoid double-free
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_233_LifecycleClose_Error_Success)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    // Expect CloseApp to be called with ERROR reason (non-userExit)
    EXPECT_CALL(*mockLifecycleManager, CloseApp(_, Exchange::ILifecycleManagerState::ERROR))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"error"})", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_234_LifecycleClose_InvalidPayload_Failure)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    // Invalid JSON payload
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", "invalid json", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_235_Lifecycle2Close_Deactivate_Success)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    // deactivate maps to USER_EXIT
    EXPECT_CALL(*mockLifecycleManager, CloseApp(_, Exchange::ILifecycleManagerState::USER_EXIT))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_236_Lifecycle2Close_KillReload_Success)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    // killReload maps to KILL_AND_RUN
    EXPECT_CALL(*mockLifecycleManager, CloseApp(_, Exchange::ILifecycleManagerState::KILL_AND_RUN))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"killReload"})", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_237_Lifecycle2Close_KillReactivate_Success)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    // killReactivate maps to KILL_AND_ACTIVATE
    EXPECT_CALL(*mockLifecycleManager, CloseApp(_, Exchange::ILifecycleManagerState::KILL_AND_ACTIVATE))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"killReactivate"})", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_238_Lifecycle2Close_UnknownType_MapsToError)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    // Unknown type maps to ERROR
    EXPECT_CALL(*mockLifecycleManager, CloseApp(_, Exchange::ILifecycleManagerState::ERROR))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"unknown"})", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_239_Lifecycle2Close_InvalidPayload_Failure)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    // Invalid JSON payload
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", "not valid json", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_240_LifecycleReady_Success)
{
    NiceMock<Exchange::MockILifecycleManagerState>* mockLifecycleManager = new NiceMock<Exchange::MockILifecycleManagerState>();
    
    EXPECT_CALL(*mockLifecycleManager, AppReady(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    lifecycleDelegate->mLifecycleManagerState = mockLifecycleManager;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
    
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    plugin.Deinitialize(&service);
    delete mockLifecycleManager;
}

TEST_F(AppGatewayCommonTest, AGC_L1_241_LifecycleReady_NoInterface_ReturnsSuccess)
{
    // When mLifecycleManagerState is nullptr, LifecycleReady still returns ERROR_NONE
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    // Don't inject mock - leave mLifecycleManagerState as nullptr
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);
    
    // LifecycleReady returns ERROR_NONE even when interface is null
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_242_LifecycleState_ReturnsUnloading)
{
    // LifecycleState returns the current state from the registry
    // When no state is registered, it returns "unloading" (default UNLOADED state)
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Default state when not registered is UNLOADED which maps to "unloading" in lifecycle1
    EXPECT_EQ("unloading", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_243_Lifecycle2State_ReturnsUnloaded)
{
    // Lifecycle2State returns the current state from the registry
    // When no state is registered, it returns "unloaded"
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Default state when not registered is UNLOADED which maps to "unloaded"
    EXPECT_EQ("unloaded", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_244_LifecycleFinished_Success)
{
    // LifecycleFinished always returns ERROR_NONE with "null" result
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.finished", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_245_CommonInternalDispatchIntent_Success)
{
    // DispatchLastIntent returns ERROR_NONE with "null" result
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.dispatchintent", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_246_CommonInternalGetLastIntent_ReturnsEmpty)
{
    // GetLastIntent returns empty when no intent is registered
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Empty result when no intent is registered for the app
    EXPECT_TRUE(result.empty());
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_247_LifecycleClose_NoInterface_ReturnsError)
{
    // When mLifecycleManagerState is nullptr, LifecycleClose returns ERROR_GENERAL
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    // Ensure mLifecycleManagerState is nullptr
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(result.find("LifecycleManager") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_248_Lifecycle2Close_NoInterface_ReturnsError)
{
    // When mLifecycleManagerState is nullptr, Lifecycle2Close returns ERROR_GENERAL
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    // Ensure mLifecycleManagerState is nullptr
    lifecycleDelegate->mLifecycleManagerState = nullptr;
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(result.find("LifecycleManager") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_249_Authenticate_NoAppInstanceId_ReturnsError)
{
    // Authenticate returns ERROR_GENERAL when no appInstanceId is registered
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    string appId;
    const auto rc = plugin.Authenticate("unknown-session-id", appId);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(appId.empty());
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_250_GetSessionId_NoAppId_ReturnsError)
{
    // GetSessionId returns ERROR_GENERAL when no appId is registered
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    string sessionId;
    const auto rc = plugin.GetSessionId("unknown-app-id", sessionId);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(sessionId.empty());
    
    plugin.Deinitialize(&service);
}

// ============================================================================
// Additional LifecycleDelegate Tests for Notification Handlers and State Management
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_251_LifecycleDelegate_AuthenticateAndGetSessionId_WithRegisteredApp)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Directly add an app to the registry (simulating what OnAppLifecycleStateChanged does)
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("com.test.app", "instance-123");
    
    // Now Authenticate and GetSessionId should work
    string appId;
    const auto authRc = plugin.Authenticate("instance-123", appId);
    EXPECT_EQ(Core::ERROR_NONE, authRc);
    EXPECT_EQ("com.test.app", appId);
    
    string sessionId;
    const auto sessionRc = plugin.GetSessionId("com.test.app", sessionId);
    EXPECT_EQ(Core::ERROR_NONE, sessionRc);
    EXPECT_EQ("instance-123", sessionId);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_252_LifecycleDelegate_LifecycleStateWithRegisteredApp)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Register app and set its lifecycle state
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-456");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-456", 
        Exchange::ILifecycleManager::UNLOADED, 
        Exchange::ILifecycleManager::ACTIVE);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // ACTIVE maps to "foreground" in lifecycle1
    EXPECT_EQ("foreground", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_253_LifecycleDelegate_Lifecycle2StateWithRegisteredApp)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Register app and set its lifecycle state
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-789");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-789", 
        Exchange::ILifecycleManager::LOADING, 
        Exchange::ILifecycleManager::SUSPENDED);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("suspended", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_254_LifecycleDelegate_NavigationIntentFlow)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Register app and navigation intent
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-nav");
    lifecycleDelegate->mNavigationIntentRegistry.AddNavigationIntent("instance-nav", 
        R"({"action":"navigate","data":{"uri":"https://example.com"}})");
    
    const auto ctx = MakeContext();
    string result;
    
    // GetLastIntent should return the registered intent
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("navigate") != string::npos);
    EXPECT_TRUE(result.find("example.com") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_255_LifecycleDelegate_StateConversions_Paused)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-paused");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-paused", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::PAUSED);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // PAUSED maps to "inactive" in lifecycle1
    EXPECT_EQ("inactive", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_256_LifecycleDelegate_StateConversions_Suspended)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-susp");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-susp", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::SUSPENDED);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // SUSPENDED maps to "suspended" in lifecycle1
    EXPECT_EQ("suspended", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_257_LifecycleDelegate_StateConversions_Hibernated)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-hib");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-hib", 
        Exchange::ILifecycleManager::SUSPENDED, 
        Exchange::ILifecycleManager::HIBERNATED);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // HIBERNATED maps to "suspended" in lifecycle1
    EXPECT_EQ("suspended", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_258_LifecycleDelegate_StateConversions_Terminating)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-term");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-term", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::TERMINATING);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // TERMINATING maps to "unloading" in lifecycle1
    EXPECT_EQ("unloading", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_259_LifecycleDelegate_StateConversions_Loading)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-load");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-load", 
        Exchange::ILifecycleManager::UNLOADED, 
        Exchange::ILifecycleManager::LOADING);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // LOADING maps to "initializing" in lifecycle1
    EXPECT_EQ("initializing", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_260_LifecycleDelegate_StateConversions_Initializing)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-init");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-init", 
        Exchange::ILifecycleManager::LOADING, 
        Exchange::ILifecycleManager::INITIALIZING);
    
    const auto ctx = MakeContext();
    string result;
    
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    // INITIALIZING maps to "initializing" in lifecycle1
    EXPECT_EQ("initializing", result);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_261_LifecycleDelegate_Lifecycle2StateConversions_AllStates)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Test lifecycle2.state with different states
    struct TestCase {
        Exchange::ILifecycleManager::LifecycleState state;
        string expected;
    };
    
    std::vector<TestCase> testCases = {
        {Exchange::ILifecycleManager::LOADING, "loading"},
        {Exchange::ILifecycleManager::INITIALIZING, "initializing"},
        {Exchange::ILifecycleManager::PAUSED, "paused"},
        {Exchange::ILifecycleManager::ACTIVE, "active"},
        {Exchange::ILifecycleManager::SUSPENDED, "suspended"},
        {Exchange::ILifecycleManager::HIBERNATED, "hibernated"},
        {Exchange::ILifecycleManager::TERMINATING, "terminating"},
    };
    
    for (size_t i = 0; i < testCases.size(); ++i) {
        const auto& tc = testCases[i];
        string instanceId = "instance-" + std::to_string(i);
        
        lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", instanceId);
        lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState(instanceId, 
            Exchange::ILifecycleManager::UNLOADED, 
            tc.state);
        
        const auto ctx = MakeContext();
        string result;
        
        const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);
        
        EXPECT_EQ(Core::ERROR_NONE, rc);
        EXPECT_EQ(tc.expected, result) << "Failed for state: " << static_cast<int>(tc.state);
        
        // Clean up for next iteration
        lifecycleDelegate->mLifecycleStateRegistry.RemoveLifecycleStateInfo(instanceId);
        lifecycleDelegate->mAppIdInstanceIdMap.RemoveAppInstanceId("test.app");
    }
    
    plugin.Deinitialize(&service);
}

// ============================================================================
// LIFECYCLE DELEGATE - HandleSubscription, HandleEvent, HandleLifecycleUpdate
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_262_LifecycleDelegate_HandleSubscription_Subscribe)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Create a mock emitter for subscription testing
    MockEmitter* emitter = new MockEmitter();
    
    // Test HandleSubscription with listen=true
    bool result = lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onForeground", true);
    EXPECT_TRUE(result);
    
    // Verify notification is registered
    EXPECT_TRUE(lifecycleDelegate->IsNotificationRegistered("Lifecycle.onForeground"));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_263_LifecycleDelegate_HandleSubscription_Unsubscribe)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    
    // First subscribe
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onBackground", true);
    EXPECT_TRUE(lifecycleDelegate->IsNotificationRegistered("Lifecycle.onBackground"));
    
    // Then unsubscribe
    bool result = lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onBackground", false);
    EXPECT_TRUE(result);
    
    // Notification should be removed
    EXPECT_FALSE(lifecycleDelegate->IsNotificationRegistered("Lifecycle.onBackground"));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_264_LifecycleDelegate_HandleEvent_ValidEvent)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = false;
    
    // Test HandleEvent with valid lifecycle event
    bool handled = lifecycleDelegate->HandleEvent(emitter, "lifecycle.onforeground", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_265_LifecycleDelegate_HandleEvent_InvalidEvent)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = false;
    
    // Test HandleEvent with invalid event (not in VALID_LIFECYCLE_EVENT)
    bool handled = lifecycleDelegate->HandleEvent(emitter, "invalid.event", true, registrationError);
    
    EXPECT_FALSE(handled);
    EXPECT_TRUE(registrationError);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_266_LifecycleDelegate_HandleLifecycleUpdate_Active)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-update-1");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-update-1", 
        Exchange::ILifecycleManager::LOADING, 
        Exchange::ILifecycleManager::INITIALIZING);
    
    // Add a navigation intent to be dispatched
    lifecycleDelegate->mNavigationIntentRegistry.AddNavigationIntent("instance-update-1", "test://intent");
    
    // Register emitter for lifecycle events
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle2.onStateChanged", true);
    lifecycleDelegate->HandleSubscription(emitter, "Discovery.onNavigateTo", true);
    
    // Call HandleLifecycleUpdate with ACTIVE state (triggers DispatchLastKnownIntent)
    lifecycleDelegate->HandleLifecycleUpdate("instance-update-1", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    
    // Give time for dispatch job to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify state was updated
    auto stateInfo = lifecycleDelegate->mLifecycleStateRegistry.GetLifecycleStateInfo("instance-update-1");
    EXPECT_EQ(Exchange::ILifecycleManager::ACTIVE, stateInfo.currentState);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_267_LifecycleDelegate_HandleLifecycle1Update_Paused)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-paused");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-paused", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::PAUSED);
    
    // Register emitter for lifecycle events
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onInactive", true);
    
    // Call HandleLifecycle1Update with PAUSED state (dispatches Lifecycle.onInactive)
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-paused", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::PAUSED);
    
    // Give time for dispatch job to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_268_LifecycleDelegate_HandleLifecycle1Update_Suspended)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-suspended");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-suspended", 
        Exchange::ILifecycleManager::PAUSED, 
        Exchange::ILifecycleManager::SUSPENDED);
    
    // Register emitter
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onSuspended", true);
    
    // Call HandleLifecycle1Update with SUSPENDED state
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-suspended", 
        Exchange::ILifecycleManager::PAUSED, 
        Exchange::ILifecycleManager::SUSPENDED);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_269_LifecycleDelegate_HandleLifecycle1Update_Unloaded)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-unloaded");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-unloaded", 
        Exchange::ILifecycleManager::SUSPENDED, 
        Exchange::ILifecycleManager::UNLOADED);
    
    // Register emitter
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onUnloading", true);
    
    // Call HandleLifecycle1Update with UNLOADED state
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-unloaded", 
        Exchange::ILifecycleManager::SUSPENDED, 
        Exchange::ILifecycleManager::UNLOADED);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_270_LifecycleDelegate_HandleLifecycle1Update_ActiveFocused)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data - app is focused
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-active-focused");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-active-focused", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    lifecycleDelegate->mFocusedAppRegistry.SetFocusedAppInstanceId("instance-l1-active-focused");
    
    // Register emitter
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onForeground", true);
    
    // Call HandleLifecycle1Update with ACTIVE state when app is focused (triggers onForeground)
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-active-focused", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_271_LifecycleDelegate_HandleLifecycle1Update_ActiveNotFocused)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data - app is NOT focused
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-active-notfocused");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-active-notfocused", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    // Don't set focus - app is in background
    
    // Register emitter
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onBackground", true);
    
    // Call HandleLifecycle1Update with ACTIVE state when app is NOT focused (triggers onBackground)
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-active-notfocused", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_272_LifecycleDelegate_HandleAppFocusForLifecycle1)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data - app is in ACTIVE state (required for focus handling)
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-focus");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-focus", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    
    // Register emitter for foreground event
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onForeground", true);
    
    // Call HandleAppFocusForLifecycle1
    lifecycleDelegate->HandleAppFocusForLifecycle1("instance-focus");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify app is now focused
    EXPECT_TRUE(lifecycleDelegate->mFocusedAppRegistry.IsAppInstanceIdFocused("instance-focus"));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_273_LifecycleDelegate_HandleAppBlurForLifecycle1)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data - app is in ACTIVE state and focused
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-blur");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-blur", 
        Exchange::ILifecycleManager::INITIALIZING, 
        Exchange::ILifecycleManager::ACTIVE);
    lifecycleDelegate->mFocusedAppRegistry.SetFocusedAppInstanceId("instance-blur");
    
    // Register emitter for background event
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onBackground", true);
    
    // Call HandleAppBlurForLifecycle1
    lifecycleDelegate->HandleAppBlurForLifecycle1("instance-blur");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify app is no longer focused
    EXPECT_FALSE(lifecycleDelegate->mFocusedAppRegistry.IsAppInstanceIdFocused("instance-blur"));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_274_LifecycleDelegate_HandleLifecycle1Update_Hibernated)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-hibernated");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-hibernated", 
        Exchange::ILifecycleManager::SUSPENDED, 
        Exchange::ILifecycleManager::HIBERNATED);
    
    // Register emitter - HIBERNATED maps to Lifecycle.onSuspended
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onSuspended", true);
    
    // Call HandleLifecycle1Update with HIBERNATED state
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-hibernated", 
        Exchange::ILifecycleManager::SUSPENDED, 
        Exchange::ILifecycleManager::HIBERNATED);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_275_LifecycleDelegate_HandleLifecycle1Update_Terminating)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-terminating");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-terminating", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::TERMINATING);
    
    // Register emitter - TERMINATING maps to Lifecycle.onUnloading
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Lifecycle.onUnloading", true);
    
    // Call HandleLifecycle1Update with TERMINATING state
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-terminating", 
        Exchange::ILifecycleManager::ACTIVE, 
        Exchange::ILifecycleManager::TERMINATING);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_276_LifecycleDelegate_HandleLifecycle1Update_DefaultState)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-l1-default");
    lifecycleDelegate->mLifecycleStateRegistry.AddLifecycleState("instance-l1-default", 
        Exchange::ILifecycleManager::UNLOADED, 
        Exchange::ILifecycleManager::LOADING);
    
    // Call HandleLifecycle1Update with LOADING state (falls into default case)
    lifecycleDelegate->HandleLifecycle1Update("instance-l1-default", 
        Exchange::ILifecycleManager::UNLOADED, 
        Exchange::ILifecycleManager::LOADING);
    
    // No dispatch expected for LOADING state - just verify no crash
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_277_LifecycleDelegate_DispatchLastKnownIntent_WithIntent)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Setup test data with navigation intent
    lifecycleDelegate->mAppIdInstanceIdMap.AddAppInstanceId("test.app", "instance-intent");
    lifecycleDelegate->mNavigationIntentRegistry.AddNavigationIntent("instance-intent", "test://navigate/to/page");
    
    // Register emitter for Discovery.onNavigateTo
    MockEmitter* emitter = new MockEmitter();
    lifecycleDelegate->HandleSubscription(emitter, "Discovery.onNavigateTo", true);
    
    // Call DispatchLastKnownIntent
    lifecycleDelegate->DispatchLastKnownIntent("test.app");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_278_LifecycleDelegate_HandleEvent_AllValidEvents)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string&) -> void* {
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);
    
    // Test all valid lifecycle events
    std::vector<string> validEvents = {
        "lifecycle.onbackground",
        "lifecycle.onforeground",
        "lifecycle.oninactive",
        "lifecycle.onsuspended",
        "lifecycle.onunloading",
        "lifecycle2.onstatechanged",
        "discovery.onnavigateto",
        "presentation.onfocusedchanged"
    };
    
    for (const auto& event : validEvents) {
        MockEmitter* emitter = new MockEmitter();
        bool registrationError = false;
        
        bool handled = lifecycleDelegate->HandleEvent(emitter, event, true, registrationError);
        
        EXPECT_TRUE(handled) << "Event " << event << " should be handled";
        EXPECT_FALSE(registrationError) << "Event " << event << " should not have registration error";
    }
    
    plugin.Deinitialize(&service);
}

// Test GetSpeed with rate >= 1.56 (should return speed 2.0)
TEST_F(AppGatewayCommonTest, AGC_L1_279_GetSpeed_Rate156_ReturnsSpeed2)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.56;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_DOUBLE_EQ(2.0, speed);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetSpeed with rate >= 1.38 (should return speed 1.67)
TEST_F(AppGatewayCommonTest, AGC_L1_280_GetSpeed_Rate138_ReturnsSpeed167)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.38;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_DOUBLE_EQ(1.67, speed);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetSpeed with rate >= 1.19 (should return speed 1.33)
TEST_F(AppGatewayCommonTest, AGC_L1_281_GetSpeed_Rate119_ReturnsSpeed133)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.19;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_DOUBLE_EQ(1.33, speed);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetSpeed with rate >= 1.0 (should return speed 1.0)
TEST_F(AppGatewayCommonTest, AGC_L1_282_GetSpeed_Rate100_ReturnsSpeed100)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.0;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_DOUBLE_EQ(1.0, speed);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetSpeed with rate < 1.0 (should return speed 0.5)
TEST_F(AppGatewayCommonTest, AGC_L1_283_GetSpeed_RateLow_ReturnsSpeed05)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 0.5;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_DOUBLE_EQ(0.5, speed);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetSpeed when GetVoiceGuidanceRate fails
TEST_F(AppGatewayCommonTest, AGC_L1_284_GetSpeed_COMError_ReturnsError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test SetSpeed with speed == 2.0 (should set rate 10.0)
TEST_F(AppGatewayCommonTest, AGC_L1_285_SetSpeed_Speed2_SetsRate10)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(10.0))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto rc = plugin.SetSpeed(2.0);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test SetSpeed with speed >= 1.67 (should set rate 1.38)
TEST_F(AppGatewayCommonTest, AGC_L1_286_SetSpeed_Speed167_SetsRate138)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(1.38))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto rc = plugin.SetSpeed(1.67);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test SetSpeed with speed >= 1.33 (should set rate 1.19)
TEST_F(AppGatewayCommonTest, AGC_L1_287_SetSpeed_Speed133_SetsRate119)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(1.19))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto rc = plugin.SetSpeed(1.33);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test SetSpeed with speed >= 1.0 (should set rate 1.0)
TEST_F(AppGatewayCommonTest, AGC_L1_288_SetSpeed_Speed100_SetsRate100)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(1.0))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto rc = plugin.SetSpeed(1.0);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test SetSpeed with speed < 1.0 (should set rate 0.1)
TEST_F(AppGatewayCommonTest, AGC_L1_289_SetSpeed_SpeedLow_SetsRate01)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(0.1))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    const auto rc = plugin.SetSpeed(0.5);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetVoiceGuidanceSettings with valid delegate and addSpeed=true
TEST_F(AppGatewayCommonTest, AGC_L1_290_GetVoiceGuidanceSettings_WithAddSpeed_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillRepeatedly([](double& rate) {
            rate = 1.0;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    string result;
    const auto rc = plugin.GetVoiceGuidanceSettings(true, result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("enabled") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetVoiceGuidanceSettings with valid delegate and addSpeed=false
TEST_F(AppGatewayCommonTest, AGC_L1_291_GetVoiceGuidanceSettings_WithoutAddSpeed_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = false;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillRepeatedly([](double& rate) {
            rate = 1.0;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    string result;
    const auto rc = plugin.GetVoiceGuidanceSettings(false, result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("enabled") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetClosedCaptionsSettings success path
TEST_F(AppGatewayCommonTest, AGC_L1_292_GetClosedCaptionsSettings_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillRepeatedly([](string& languages) {
            languages = "eng";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    string result;
    const auto rc = plugin.GetClosedCaptionsSettings(result);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(result.find("enabled") != string::npos);
    
    plugin.Deinitialize(&service);
    delete mockUserSettings;
}

// Test GetSpeed with null userSettings delegate returns error
TEST_F(AppGatewayCommonTest, AGC_L1_293_GetSpeed_NullUserSettingsDelegate_ReturnsError)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    double speed = 0.0;
    const auto rc = plugin.GetSpeed(speed);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.Deinitialize(&service);
}

// ============================================================================
// Tests for null sub-delegate paths (covers error paths in AppGatewayCommon.cpp)
// These tests create a SettingsDelegate without calling setShell(), so all
// sub-delegates (userSettings, systemDelegate, etc.) are nullptr.
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_294_GetVoiceGuidance_NullUserSettingsDelegate_ReturnsError)
{
    // Create a SettingsDelegate without calling setShell() - all sub-delegates are null
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    // Note: Not calling plugin.mDelegate->setShell() so getUserSettings() returns nullptr
    
    string result;
    const auto rc = plugin.GetVoiceGuidance(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_295_GetAudioDescription_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetAudioDescription(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_296_GetAudioDescriptionsEnabled_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetAudioDescriptionsEnabled(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_297_GetHighContrast_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetHighContrast(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_298_GetCaptions_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetCaptions(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_299_GetPresentationLanguage_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetPresentationLanguage(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_300_GetLocale_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetLocale(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_301_GetPreferredAudioLanguages_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetPreferredAudioLanguages(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    // Default is "[]" for audio languages
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_302_GetPreferredCaptionsLanguages_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetPreferredCaptionsLanguages(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_EQ("[\"eng\"]", result);  // Default is ["eng"] for captions
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_303_SetPreferredAudioLanguages_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    const auto rc = plugin.SetPreferredAudioLanguages("eng,spa");
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_304_SetPreferredCaptionsLanguages_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    const auto rc = plugin.SetPreferredCaptionsLanguages("eng,spa");
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_305_SetVoiceGuidance_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    const auto rc = plugin.SetVoiceGuidance(true);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_306_SetCaptions_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    const auto rc = plugin.SetCaptions(true);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_307_SetAudioDescriptionsEnabled_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    const auto rc = plugin.SetAudioDescriptionsEnabled(true);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_308_GetClosedCaptionsSettings_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetClosedCaptionsSettings(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_309_GetVoiceGuidanceHints_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetVoiceGuidanceHints(result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_310_SetVoiceGuidanceHints_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    const auto rc = plugin.SetVoiceGuidanceHints(true);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    
    plugin.mDelegate.reset();
}

TEST_F(AppGatewayCommonTest, AGC_L1_311_GetVoiceGuidanceSettings_NullUserSettingsDelegate_ReturnsError)
{
    plugin.mDelegate = std::make_shared<SettingsDelegate>();
    
    string result;
    const auto rc = plugin.GetVoiceGuidanceSettings(false, result);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(result.find("error") != string::npos);
    
    plugin.mDelegate.reset();
}

// ============================================================================
// USERSETTINGSDELEGATE - Notification Handler Tests
// ============================================================================

TEST_F(AppGatewayCommonTest, AGC_L1_322_UserSettings_NotificationHandler_OnAudioDescriptionChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Subscribe to trigger registration
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onaudiodescriptionsettingschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnAudioDescriptionChanged
        handler.OnAudioDescriptionChanged(true);
        handler.OnAudioDescriptionChanged(false);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_323_UserSettings_NotificationHandler_OnPreferredAudioLanguagesChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "localization.onpreferredaudiolanguageschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnPreferredAudioLanguagesChanged
        handler.OnPreferredAudioLanguagesChanged("eng,fra,spa");
        handler.OnPreferredAudioLanguagesChanged("tam");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_324_UserSettings_NotificationHandler_OnPresentationLanguageChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "localization.onlocalechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnPresentationLanguageChanged with locale format
        handler.OnPresentationLanguageChanged("en-US");
        handler.OnPresentationLanguageChanged("fr-FR");
        // Test without hyphen (should trigger warning path)
        handler.OnPresentationLanguageChanged("en");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_325_UserSettings_NotificationHandler_OnCaptionsChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillRepeatedly([](string& languages) {
            languages = "eng,spa";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "closedcaptions.onenabledchanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnCaptionsChanged
        handler.OnCaptionsChanged(true);
        handler.OnCaptionsChanged(false);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_326_UserSettings_NotificationHandler_OnPreferredCaptionsLanguagesChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillRepeatedly([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "closedcaptions.onpreferredlanguageschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnPreferredCaptionsLanguagesChanged
        handler.OnPreferredCaptionsLanguagesChanged("eng,fra");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_327_UserSettings_NotificationHandler_OnHighContrastChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onhighcontrastuichanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnHighContrastChanged
        handler.OnHighContrastChanged(true);
        handler.OnHighContrastChanged(false);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_328_UserSettings_NotificationHandler_OnVoiceGuidanceChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    // Mock voice guidance methods needed by DispatchVoiceGuidanceSettingsChanged
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillRepeatedly([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillRepeatedly([](double& rate) {
            rate = 1.5;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillRepeatedly([](bool& hints) {
            hints = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onvoiceguidancesettingschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        auto& handler = userSettingsDelegate->mNotificationHandler;
        
        // Test OnVoiceGuidanceChanged - triggers DispatchVoiceGuidanceSettingsChanged
        handler.OnVoiceGuidanceChanged(true);
        
        // Test OnVoiceGuidanceRateChanged
        handler.OnVoiceGuidanceRateChanged(2.0);
        
        // Test OnVoiceGuidanceHintsChanged
        handler.OnVoiceGuidanceHintsChanged(false);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_329_UserSettings_GetVoiceGuidanceSettings_WithSpeed)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.5;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce([](bool& hints) {
            hints = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Test UpdateVoiceGuidanceSettings with addSpeed=true
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    bool success = userSettingsDelegate->UpdateVoiceGuidanceSettings(true, result);
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.find("\"enabled\":true") != string::npos);
    EXPECT_TRUE(result.find("\"speed\"") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_330_UserSettings_GetVoiceGuidanceSettings_VoiceGuidanceError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    bool success = userSettingsDelegate->UpdateVoiceGuidanceSettings(false, result);
    EXPECT_FALSE(success);
    // ErrorUtils::CustomInternal produces JSON-RPC error with "message" key containing the error text
    EXPECT_TRUE(result.find("couldn't") != string::npos || result.find("message") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_331_UserSettings_GetVoiceGuidanceSettings_RateError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    bool success = userSettingsDelegate->UpdateVoiceGuidanceSettings(false, result);
    EXPECT_FALSE(success);
    // ErrorUtils::CustomInternal produces JSON-RPC error with "message" key containing the error text
    EXPECT_TRUE(result.find("couldn't") != string::npos || result.find("message") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_332_UserSettings_GetVoiceGuidanceSettings_HintsError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce([](double& rate) {
            rate = 1.0;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    bool success = userSettingsDelegate->UpdateVoiceGuidanceSettings(false, result);
    EXPECT_FALSE(success);
    // ErrorUtils::CustomInternal produces JSON-RPC error with "message" key containing the error text
    EXPECT_TRUE(result.find("couldn't") != string::npos || result.find("message") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_333_UserSettings_GetClosedCaptionSettings_CaptionsError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetClosedCaptionSettings(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    // ErrorUtils::CustomInternal produces JSON-RPC error with "message" key containing the error text
    EXPECT_TRUE(result.find("couldn't") != string::npos || result.find("message") != string::npos);
    
    plugin.Deinitialize(&service);
}

TEST_F(AppGatewayCommonTest, AGC_L1_334_UserSettings_GetClosedCaptionSettings_LanguagesError)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetClosedCaptionSettings(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    // ErrorUtils::CustomInternal produces JSON-RPC error with "message" key containing the error text
    EXPECT_TRUE(result.find("couldn't") != string::npos || result.find("message") != string::npos);
    
    plugin.Deinitialize(&service);
}

// Test GetClosedCaptionSettings success path
TEST_F(AppGatewayCommonTest, AGC_L1_335_UserSettings_GetClosedCaptionSettings_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillOnce([](string& languages) {
            languages = "eng,fra";
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetClosedCaptionSettings(result);
    EXPECT_EQ(rc, Core::ERROR_NONE);
    EXPECT_TRUE(result.find("enabled") != string::npos);
    EXPECT_TRUE(result.find("true") != string::npos);
    EXPECT_TRUE(result.find("preferredLanguages") != string::npos);
    
    plugin.Deinitialize(&service);
}

// Test GetAudioDescriptionsEnabled success path
TEST_F(AppGatewayCommonTest, AGC_L1_336_UserSettings_GetAudioDescriptionsEnabled_Success)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetAudioDescription(_))
        .WillOnce([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetAudioDescriptionsEnabled(result);
    EXPECT_EQ(rc, Core::ERROR_NONE);
    EXPECT_EQ(result, "true");
    
    plugin.Deinitialize(&service);
}

// Test GetAudioDescriptionsEnabled error path
TEST_F(AppGatewayCommonTest, AGC_L1_337_UserSettings_GetAudioDescriptionsEnabled_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetAudioDescription(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetAudioDescriptionsEnabled(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetHighContrast error path
TEST_F(AppGatewayCommonTest, AGC_L1_338_UserSettings_GetHighContrast_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetHighContrast(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetHighContrast(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetCaptions error path
TEST_F(AppGatewayCommonTest, AGC_L1_339_UserSettings_GetCaptions_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetCaptions(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetVoiceGuidance error path
TEST_F(AppGatewayCommonTest, AGC_L1_340_UserSettings_SetVoiceGuidance_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetVoiceGuidance(true);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetAudioDescriptionsEnabled error path
TEST_F(AppGatewayCommonTest, AGC_L1_341_UserSettings_SetAudioDescriptionsEnabled_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetAudioDescription(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetAudioDescriptionsEnabled(true);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetCaptions error path
TEST_F(AppGatewayCommonTest, AGC_L1_342_UserSettings_SetCaptions_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetCaptions(true);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetVoiceGuidanceRate error path
TEST_F(AppGatewayCommonTest, AGC_L1_343_UserSettings_SetVoiceGuidanceRate_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetVoiceGuidanceRate(1.0);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetVoiceGuidanceHints error path
TEST_F(AppGatewayCommonTest, AGC_L1_344_UserSettings_SetVoiceGuidanceHints_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetVoiceGuidanceHints(true);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetVoiceGuidanceHints error path
TEST_F(AppGatewayCommonTest, AGC_L1_345_UserSettings_GetVoiceGuidanceHints_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetVoiceGuidanceHints(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetPresentationLanguage error path
TEST_F(AppGatewayCommonTest, AGC_L1_346_UserSettings_GetPresentationLanguage_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetPresentationLanguage(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetPresentationLanguage empty result
TEST_F(AppGatewayCommonTest, AGC_L1_347_UserSettings_GetPresentationLanguage_Empty)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce([](string& language) {
            language = "";  // Empty result
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetPresentationLanguage(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetPresentationLanguage without dash
TEST_F(AppGatewayCommonTest, AGC_L1_348_UserSettings_GetPresentationLanguage_NoDash)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce([](string& language) {
            language = "eng";  // No dash - just language code
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetPresentationLanguage(result);
    EXPECT_EQ(rc, Core::ERROR_NONE);
    EXPECT_EQ(result, "\"eng\"");
    
    plugin.Deinitialize(&service);
}

// Test GetLocale error path
TEST_F(AppGatewayCommonTest, AGC_L1_349_UserSettings_GetLocale_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetLocale(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetLocale empty result
TEST_F(AppGatewayCommonTest, AGC_L1_350_UserSettings_GetLocale_Empty)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetPresentationLanguage(_))
        .WillOnce([](string& language) {
            language = "";  // Empty result
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetLocale(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetLocale error path
TEST_F(AppGatewayCommonTest, AGC_L1_351_UserSettings_SetLocale_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetLocale("en-US");
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetPreferredAudioLanguages error path
TEST_F(AppGatewayCommonTest, AGC_L1_352_UserSettings_GetPreferredAudioLanguages_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetPreferredAudioLanguages(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    EXPECT_EQ(result, "[]");
    
    plugin.Deinitialize(&service);
}

// Test SetPreferredAudioLanguages error path
TEST_F(AppGatewayCommonTest, AGC_L1_353_UserSettings_SetPreferredAudioLanguages_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetPreferredAudioLanguages("[\"eng\",\"fra\"]");
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test SetPreferredCaptionsLanguages error path
TEST_F(AppGatewayCommonTest, AGC_L1_354_UserSettings_SetPreferredCaptionsLanguages_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    Core::hresult rc = userSettingsDelegate->SetPreferredCaptionsLanguages("[\"eng\"]");
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test GetAudioDescription error path
TEST_F(AppGatewayCommonTest, AGC_L1_355_UserSettings_GetAudioDescription_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetAudioDescription(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    string result;
    Core::hresult rc = userSettingsDelegate->GetAudioDescription(result);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// Test OnPreferredCaptionsLanguagesChanged notification
TEST_F(AppGatewayCommonTest, AGC_L1_356_UserSettings_NotificationHandler_OnPreferredCaptionsLanguagesChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, GetCaptions(_))
        .WillRepeatedly([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "closedcaptions.onpreferredlanguageschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        // Trigger notification
        userSettingsDelegate->mNotificationHandler.OnPreferredCaptionsLanguagesChanged("eng,fra,spa");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test OnHighContrastChanged notification
TEST_F(AppGatewayCommonTest, AGC_L1_357_UserSettings_NotificationHandler_OnHighContrastChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onhighcontrastuichanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        // Trigger notification
        userSettingsDelegate->mNotificationHandler.OnHighContrastChanged(true);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test OnPresentationLanguageChanged notification with dash
TEST_F(AppGatewayCommonTest, AGC_L1_358_UserSettings_NotificationHandler_OnPresentationLanguageChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "localization.onpresentationlanguagechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        // Trigger notification with dash-delimited locale
        userSettingsDelegate->mNotificationHandler.OnPresentationLanguageChanged("en-US");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test OnPresentationLanguageChanged notification without dash (warning path)
TEST_F(AppGatewayCommonTest, AGC_L1_359_UserSettings_NotificationHandler_OnPresentationLanguageChanged_NoDash)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillRepeatedly(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "localization.onlocalechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        // Trigger notification without dash - exercises warning path
        userSettingsDelegate->mNotificationHandler.OnPresentationLanguageChanged("eng");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test OnVoiceGuidanceRateChanged notification
TEST_F(AppGatewayCommonTest, AGC_L1_360_UserSettings_NotificationHandler_OnVoiceGuidanceRateChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillRepeatedly([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillRepeatedly([](double& rate) {
            rate = 1.5;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillRepeatedly([](bool& hints) {
            hints = true;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onvoiceguidancesettingschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        // Trigger notification
        userSettingsDelegate->mNotificationHandler.OnVoiceGuidanceRateChanged(1.5);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test OnVoiceGuidanceHintsChanged notification
TEST_F(AppGatewayCommonTest, AGC_L1_361_UserSettings_NotificationHandler_OnVoiceGuidanceHintsChanged)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidance(_))
        .WillRepeatedly([](bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillRepeatedly([](double& rate) {
            rate = 1.0;
            return Core::ERROR_NONE;
        });
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceHints(_))
        .WillRepeatedly([](bool& hints) {
            hints = false;
            return Core::ERROR_NONE;
        });
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onvoiceguidancesettingschanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    if (userSettingsDelegate) {
        // Trigger notification
        userSettingsDelegate->mNotificationHandler.OnVoiceGuidanceHintsChanged(true);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test HandleEvent with invalid event
TEST_F(AppGatewayCommonTest, AGC_L1_362_UserSettings_HandleEvent_InvalidEvent)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Try to register for invalid event using heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = false;
    plugin.HandleAppEventNotifier(emitter, "invalidevent.onsomething", true, registrationError);
    // Invalid events should set registrationError to true
    EXPECT_TRUE(registrationError);
    
    // Allow async EventRegistrationJob to complete before cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test HandleSubscription unsubscribe path
TEST_F(AppGatewayCommonTest, AGC_L1_363_UserSettings_HandleSubscription_Unsubscribe)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockUserSettings, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for notifications using proper heap-allocated emitter
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "accessibility.onaudiodescriptionchanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // status will be true if event was handled (which is expected behavior)
    
    // Unsubscribe - this is the main test path
    plugin.HandleAppEventNotifier(emitter, "accessibility.onaudiodescriptionchanged", false, status);
    
    // Allow async EventRegistrationJob to complete before cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test GetVoiceGuidanceRate error path
TEST_F(AppGatewayCommonTest, AGC_L1_364_UserSettings_GetVoiceGuidanceRate_Error)
{
    NiceMock<Exchange::MockIUserSettings>* mockUserSettings = new NiceMock<Exchange::MockIUserSettings>();
    testing::Mock::AllowLeak(mockUserSettings);
    
    EXPECT_CALL(*mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockUserSettings](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.UserSettings") {
                mockUserSettings->AddRef();
                return static_cast<void*>(mockUserSettings);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto userSettingsDelegate = plugin.mDelegate->getUserSettings();
    ASSERT_NE(userSettingsDelegate, nullptr);
    
    double rate;
    Core::hresult rc = userSettingsDelegate->GetVoiceGuidanceRate(rate);
    EXPECT_NE(rc, Core::ERROR_NONE);
    
    plugin.Deinitialize(&service);
}

// ============================================================================
// TTSDELEGATE - Additional tests for improved coverage
// ============================================================================

// Test TTS HandleSubscription when TTS interface is not available
TEST_F(AppGatewayCommonTest, AGC_L1_365_TTS_HandleSubscription_NoTTSInterface)
{
    NiceMock<ServiceMock> service;
    // Return nullptr for TTS callsign to simulate unavailable interface
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([](const uint32_t, const string& callsign) -> void* {
            return nullptr; // No interfaces available
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Try to subscribe - should fail because TTS interface is not available
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onvoicechanged", true, status);
    // status should be true (error) because HandleSubscription returns false
    EXPECT_TRUE(status);
    
    // Allow async EventRegistrationJob to complete before cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS double registration (already registered path)
TEST_F(AppGatewayCommonTest, AGC_L1_366_TTS_HandleSubscription_AlreadyRegistered)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    // Register for first TTS event
    MockEmitter* emitter1 = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter1, "texttospeech.onvoicechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Register for second TTS event - should use already registered path
    MockEmitter* emitter2 = new MockEmitter();
    plugin.HandleAppEventNotifier(emitter2, "texttospeech.onspeechcomplete", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    emitter1->Release();
    emitter2->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnVoiceChanged notification
TEST_F(AppGatewayCommonTest, AGC_L1_367_TTS_NotificationHandler_OnVoiceChanged)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onvoicechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnVoiceChanged("en-US-Wavenet-D");
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnSpeechReady notification
TEST_F(AppGatewayCommonTest, AGC_L1_368_TTS_NotificationHandler_OnSpeechReady)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onwillspeak", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnSpeechReady(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnSpeechStarted notification
TEST_F(AppGatewayCommonTest, AGC_L1_369_TTS_NotificationHandler_OnSpeechStarted)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onspeechstart", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnSpeechStarted(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnSpeechPaused notification
TEST_F(AppGatewayCommonTest, AGC_L1_370_TTS_NotificationHandler_OnSpeechPaused)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onspeechpause", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnSpeechPaused(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnSpeechResumed notification
TEST_F(AppGatewayCommonTest, AGC_L1_371_TTS_NotificationHandler_OnSpeechResumed)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onspeechresume", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnSpeechResumed(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnSpeechInterrupted notification
TEST_F(AppGatewayCommonTest, AGC_L1_372_TTS_NotificationHandler_OnSpeechInterrupted)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onspeechinterrupted", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnSpeechInterrupted(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnNetworkError notification
TEST_F(AppGatewayCommonTest, AGC_L1_373_TTS_NotificationHandler_OnNetworkError)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onnetworkerror", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnNetworkError(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnPlaybackError notification
TEST_F(AppGatewayCommonTest, AGC_L1_374_TTS_NotificationHandler_OnPlaybackError)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onplaybackerror", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnPlaybackError(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnSpeechComplete notification
TEST_F(AppGatewayCommonTest, AGC_L1_375_TTS_NotificationHandler_OnSpeechComplete)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onspeechcomplete", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnSpeechComplete(12345);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS OnTTSStateChanged notification
TEST_F(AppGatewayCommonTest, AGC_L1_376_TTS_NotificationHandler_OnTTSStateChanged)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onttsstatechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Trigger notification
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    if (ttsDelegate) {
        ttsDelegate->mNotificationHandler.OnTTSStateChanged(true);
    }
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test TTS destructor with registered notification handler
TEST_F(AppGatewayCommonTest, AGC_L1_377_TTS_Destructor_WithRegisteredNotifications)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onvoicechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Don't release emitter before deinitialize - let destructor clean up
    emitter->Release();
    
    // Deinitialize will trigger TTSDelegate destructor with registered notifications
    plugin.Deinitialize(&service);
}

// Test TTS SetRegistered and GetRegistered methods
TEST_F(AppGatewayCommonTest, AGC_L1_378_TTS_NotificationHandler_SetGetRegistered)
{
    NiceMock<Exchange::MockITextToSpeech>* mockTTS = new NiceMock<Exchange::MockITextToSpeech>();
    testing::Mock::AllowLeak(mockTTS);
    
    EXPECT_CALL(*mockTTS, Register(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockTTS, Unregister(_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockTTS](const uint32_t, const string& callsign) -> void* {
            if (callsign == "org.rdk.TextToSpeech") {
                mockTTS->AddRef();
                return static_cast<void*>(mockTTS);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto ttsDelegate = plugin.mDelegate->ttsDelegate;
    ASSERT_NE(ttsDelegate, nullptr);
    
    // Initially not registered
    EXPECT_FALSE(ttsDelegate->mNotificationHandler.GetRegistered());
    
    // Subscribe to trigger registration
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "texttospeech.onvoicechanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Now should be registered
    EXPECT_TRUE(ttsDelegate->mNotificationHandler.GetRegistered());
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// ============================================================================
// SYSTEMDELEGATE.H - Tests for SystemDelegate
// ============================================================================

// Mock for ILocalDispatcher to test JSONRPC-based methods
class MockLocalDispatcher : public PluginHost::ILocalDispatcher {
public:
    MockLocalDispatcher() : mRefCount(1), mPlugin(nullptr) {}
    
    void AddRef() const override { ++mRefCount; }
    
    uint32_t Release() const override {
        if (--mRefCount == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }
    
    // IDispatcher interface - required methods
    Core::hresult Validate(const string& token, const string& method, const string& parameters) const override {
        (void)token;
        (void)method;
        (void)parameters;
        return Core::ERROR_NONE;
    }
    
    Core::hresult Invoke(ICallback* callback, const uint32_t channelId, const uint32_t id, const string& token,
                         const string& method, const string& parameters, string& response) override {
        (void)callback;
        return Invoke(channelId, id, token, method, parameters, response);
    }
    
    Core::hresult Revoke(ICallback* callback) override {
        (void)callback;
        return Core::ERROR_NONE;
    }
    
    // ILocalDispatcher interface
    Core::hresult Invoke(const uint32_t channelId, const uint32_t id, const string& token,
                         const string& method, const string& parameters, string& response) override {
        (void)channelId;
        (void)id;
        (void)token;
        
        // Extract method name from designator (format: "callsign.1.method")
        auto lastDot = method.rfind('.');
        string methodName = (lastDot != string::npos) ? method.substr(lastDot + 1) : method;
        
        // Return configured response based on method
        auto it = mResponses.find(methodName);
        if (it != mResponses.end()) {
            response = it->second.response;
            return it->second.result;
        }
        
        // Default response
        response = "{}";
        return Core::ERROR_NONE;
    }
    
    ILocalDispatcher* Local() override {
        // Return this to indicate dispatcher is valid
        return this;
    }
    
    void Activate(PluginHost::IShell* service) override {
        (void)service;
    }
    
    void Deactivate() override {
    }
    
    void Dropped(const uint32_t channelId) override {
        (void)channelId;
    }
    
    BEGIN_INTERFACE_MAP(MockLocalDispatcher)
    INTERFACE_ENTRY(PluginHost::ILocalDispatcher)
    END_INTERFACE_MAP
    
    // Configure responses for methods
    struct MethodResponse {
        string response;
        Core::hresult result;
    };
    
    void SetResponse(const string& method, const string& response, Core::hresult result = Core::ERROR_NONE) {
        mResponses[method] = {response, result};
    }
    
    void ClearResponses() {
        mResponses.clear();
    }
    
    std::map<string, MethodResponse> mResponses;
    mutable uint32_t mRefCount;
    PluginHost::IPlugin* mPlugin;
};

// Test SystemDelegate::SetFlagsFromSupported - Empty array
TEST(SystemDelegateTest, AGC_L1_379_SetFlagsFromSupported_EmptyArray)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_FALSE(result);  // No recognized tokens
    EXPECT_FALSE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - PCM/Stereo detection
TEST(SystemDelegateTest, AGC_L1_380_SetFlagsFromSupported_PCM)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"PCM\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - STEREO detection
TEST(SystemDelegateTest, AGC_L1_381_SetFlagsFromSupported_STEREO)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"STEREO\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - AC3 (Dolby Digital 5.1)
TEST(SystemDelegateTest, AGC_L1_382_SetFlagsFromSupported_AC3)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"AC3\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_TRUE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - DOLBY AC3
TEST(SystemDelegateTest, AGC_L1_383_SetFlagsFromSupported_DolbyAC3)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"DOLBY AC3\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_TRUE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - DOLBY DIGITAL
TEST(SystemDelegateTest, AGC_L1_384_SetFlagsFromSupported_DolbyDigital)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"DOLBY DIGITAL\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_TRUE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - EAC3 (Dolby Digital Plus)
TEST(SystemDelegateTest, AGC_L1_385_SetFlagsFromSupported_EAC3)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"EAC3\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_FALSE(dd51);  // EAC3 should NOT trigger dd51
    EXPECT_TRUE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - DD+
TEST(SystemDelegateTest, AGC_L1_386_SetFlagsFromSupported_DDPlus)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"DD+\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_TRUE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - DOLBY DIGITAL PLUS
TEST(SystemDelegateTest, AGC_L1_387_SetFlagsFromSupported_DolbyDigitalPlus)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"DOLBY DIGITAL PLUS\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_TRUE(dd51);  // Contains "DOLBY DIGITAL"
    EXPECT_TRUE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - AC4
TEST(SystemDelegateTest, AGC_L1_388_SetFlagsFromSupported_AC4)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"AC4\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_TRUE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - ATMOS
TEST(SystemDelegateTest, AGC_L1_389_SetFlagsFromSupported_ATMOS)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"ATMOS\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_TRUE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - Multiple formats
TEST(SystemDelegateTest, AGC_L1_390_SetFlagsFromSupported_MultipleFormats)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"PCM\",\"AC3\",\"EAC3\",\"ATMOS\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(stereo);
    EXPECT_TRUE(dd51);
    EXPECT_TRUE(dd51p);
    EXPECT_TRUE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - Not an array
TEST(SystemDelegateTest, AGC_L1_391_SetFlagsFromSupported_NotArray)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":\"PCM\"}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_FALSE(result);  // Not an array
    EXPECT_FALSE(stereo);
    EXPECT_FALSE(dd51);
    EXPECT_FALSE(dd51p);
    EXPECT_FALSE(atmos);
}

// Test SystemDelegate::SetFlagsFromSupported - Empty string token
TEST(SystemDelegateTest, AGC_L1_392_SetFlagsFromSupported_EmptyToken)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"\",\"PCM\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(stereo);  // PCM should still be recognized
}

// Test SystemDelegate::SetFlagsFromSupported - Case insensitive
TEST(SystemDelegateTest, AGC_L1_393_SetFlagsFromSupported_CaseInsensitive)
{
    bool stereo = false, dd51 = false, dd51p = false, atmos = false;
    
    WPEFramework::Core::JSON::VariantContainer container;
    container.FromString("{\"supportedAudioFormat\":[\"pcm\",\"ac3\",\"eac3\",\"atmos\"]}");
    auto supported = container.Get("supportedAudioFormat");
    
    bool result = SystemDelegate::SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(stereo);
    EXPECT_TRUE(dd51);
    EXPECT_TRUE(dd51p);
    EXPECT_TRUE(atmos);
}

// Test SystemDelegate::HandleEvent - Register for video resolution event
TEST_F(AppGatewayCommonTest, AGC_L1_394_SystemDelegate_HandleEvent_VideoResolution_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onVideoResolutionChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Register for screen resolution event
TEST_F(AppGatewayCommonTest, AGC_L1_395_SystemDelegate_HandleEvent_ScreenResolution_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onScreenResolutionChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Register for HDCP event
TEST_F(AppGatewayCommonTest, AGC_L1_396_SystemDelegate_HandleEvent_Hdcp_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onHdcpChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Register for HDR event
TEST_F(AppGatewayCommonTest, AGC_L1_397_SystemDelegate_HandleEvent_Hdr_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onHdrChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Register for audio event
TEST_F(AppGatewayCommonTest, AGC_L1_398_SystemDelegate_HandleEvent_Audio_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onAudioChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Register for device name event
TEST_F(AppGatewayCommonTest, AGC_L1_399_SystemDelegate_HandleEvent_DeviceName_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onDeviceNameChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Register for name event (alternate)
TEST_F(AppGatewayCommonTest, AGC_L1_400_SystemDelegate_HandleEvent_NameChanged_Register)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onNameChanged", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Unregister
TEST_F(AppGatewayCommonTest, AGC_L1_401_SystemDelegate_HandleEvent_Unregister)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    // First register
    systemDelegate->HandleEvent(emitter, "Device.onVideoResolutionChanged", true, registrationError);
    EXPECT_FALSE(registrationError);
    
    // Now unregister
    registrationError = true;
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onVideoResolutionChanged", false, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Unknown event
TEST_F(AppGatewayCommonTest, AGC_L1_402_SystemDelegate_HandleEvent_UnknownEvent)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = false;
    
    bool handled = systemDelegate->HandleEvent(emitter, "Device.onUnknownEvent", true, registrationError);
    
    EXPECT_FALSE(handled);
    EXPECT_TRUE(registrationError);  // Unknown event
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::HandleEvent - Case insensitive
TEST_F(AppGatewayCommonTest, AGC_L1_403_SystemDelegate_HandleEvent_CaseInsensitive)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    MockEmitter* emitter = new MockEmitter();
    bool registrationError = true;
    
    // Test with different case - should still work
    bool handled = systemDelegate->HandleEvent(emitter, "DEVICE.ONVIDEORESOLUTIONCHANGED", true, registrationError);
    
    EXPECT_TRUE(handled);
    EXPECT_FALSE(registrationError);
    
    emitter->Release();
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceMake - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_404_SystemDelegate_GetDeviceMake_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string make;
    Core::hresult result = systemDelegate->GetDeviceMake(make);
    
    // When link invoke fails, code falls back to "unknown" and returns ERROR_NONE
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"unknown\"", make);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceMake - Success with ILocalDispatcher
TEST_F(AppGatewayCommonTest, AGC_L1_405_SystemDelegate_GetDeviceMake_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getDeviceInfo", "{\"make\":\"TestMake\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string make;
    Core::hresult result = systemDelegate->GetDeviceMake(make);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"TestMake\"", make);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceMake - Empty make returns "unknown"
TEST_F(AppGatewayCommonTest, AGC_L1_406_SystemDelegate_GetDeviceMake_EmptyReturnsUnknown)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getDeviceInfo", "{\"make\":\"\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string make;
    Core::hresult result = systemDelegate->GetDeviceMake(make);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"unknown\"", make);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceName - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_407_SystemDelegate_GetDeviceName_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string name;
    Core::hresult result = systemDelegate->GetDeviceName(name);
    
    // When invoke fails, falls back to "Living Room" and returns ERROR_NONE
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"Living Room\"", name);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceName - Success
TEST_F(AppGatewayCommonTest, AGC_L1_408_SystemDelegate_GetDeviceName_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getFriendlyName", "{\"friendlyName\":\"My Device\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string name;
    Core::hresult result = systemDelegate->GetDeviceName(name);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"My Device\"", name);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceName - Empty returns default
TEST_F(AppGatewayCommonTest, AGC_L1_409_SystemDelegate_GetDeviceName_EmptyReturnsDefault)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getFriendlyName", "{\"friendlyName\":\"\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string name;
    Core::hresult result = systemDelegate->GetDeviceName(name);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"Living Room\"", name);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetDeviceName - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_410_SystemDelegate_SetDeviceName_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetDeviceName("New Name");
    
    // When invoke fails, returns ERROR_GENERAL
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetDeviceName - Success
TEST_F(AppGatewayCommonTest, AGC_L1_411_SystemDelegate_SetDeviceName_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setFriendlyName", "{\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetDeviceName("New Name");
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetDeviceName - Failure
TEST_F(AppGatewayCommonTest, AGC_L1_412_SystemDelegate_SetDeviceName_Failure)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setFriendlyName", "{\"success\":false}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetDeviceName("New Name");
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_413_SystemDelegate_GetDeviceSku_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Success with underscore split
TEST_F(AppGatewayCommonTest, AGC_L1_414_SystemDelegate_GetDeviceSku_SuccessWithUnderscore)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"stbVersion\":\"SKU123_VERSION\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"SKU123\"", sku);  // Split by underscore
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Success without underscore
TEST_F(AppGatewayCommonTest, AGC_L1_415_SystemDelegate_GetDeviceSku_SuccessNoUnderscore)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"stbVersion\":\"SKU123\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"SKU123\"", sku);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Missing stbVersion
TEST_F(AppGatewayCommonTest, AGC_L1_416_SystemDelegate_GetDeviceSku_MissingStbVersion)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"1.0\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Empty stbVersion
TEST_F(AppGatewayCommonTest, AGC_L1_417_SystemDelegate_GetDeviceSku_EmptyStbVersion)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"stbVersion\":\"_VERSION\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    // When stbVersion starts with underscore, sku is empty
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - RPC failure
TEST_F(AppGatewayCommonTest, AGC_L1_418_SystemDelegate_GetDeviceSku_RpcFailure)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{}", Core::ERROR_GENERAL);
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_419_SystemDelegate_GetFirmwareVersion_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Success
TEST_F(AppGatewayCommonTest, AGC_L1_420_SystemDelegate_GetFirmwareVersion_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"99.99.15.07\",\"stbVersion\":\"SKXI11ADS_DEV\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_FALSE(version.empty());
    // Check that version contains expected structure
    EXPECT_TRUE(version.find("\"api\"") != string::npos);
    EXPECT_TRUE(version.find("\"firmware\"") != string::npos);
    EXPECT_TRUE(version.find("\"os\"") != string::npos);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Missing receiverVersion
TEST_F(AppGatewayCommonTest, AGC_L1_421_SystemDelegate_GetFirmwareVersion_MissingReceiverVersion)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"stbVersion\":\"SKU123\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Empty receiverVersion
TEST_F(AppGatewayCommonTest, AGC_L1_422_SystemDelegate_GetFirmwareVersion_EmptyReceiverVersion)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"\",\"stbVersion\":\"SKU123\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Empty stbVersion
TEST_F(AppGatewayCommonTest, AGC_L1_423_SystemDelegate_GetFirmwareVersion_EmptyStbVersion)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"99.99.15.07\",\"stbVersion\":\"\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Invalid version format
TEST_F(AppGatewayCommonTest, AGC_L1_424_SystemDelegate_GetFirmwareVersion_InvalidFormat)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"invalid\",\"stbVersion\":\"SKU123\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - RPC failure
TEST_F(AppGatewayCommonTest, AGC_L1_425_SystemDelegate_GetFirmwareVersion_RpcFailure)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{}", Core::ERROR_GENERAL);
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Cached response
TEST_F(AppGatewayCommonTest, AGC_L1_426_SystemDelegate_GetFirmwareVersion_Cached)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"99.99.15.07\",\"stbVersion\":\"SKXI11ADS_DEV\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    // First call - should populate cache
    string version1;
    Core::hresult result1 = systemDelegate->GetFirmwareVersion(version1);
    EXPECT_EQ(Core::ERROR_NONE, result1);
    
    // Second call - should use cache
    string version2;
    Core::hresult result2 = systemDelegate->GetFirmwareVersion(version2);
    EXPECT_EQ(Core::ERROR_NONE, result2);
    EXPECT_EQ(version1, version2);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_427_SystemDelegate_GetCountryCode_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    // When invoke fails, code is empty and wrapped in quotes, returns ERROR_NONE
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - USA territory
TEST_F(AppGatewayCommonTest, AGC_L1_428_SystemDelegate_GetCountryCode_USA)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"USA\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"US\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - CAN territory
TEST_F(AppGatewayCommonTest, AGC_L1_429_SystemDelegate_GetCountryCode_CAN)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"CAN\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"CA\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - ITA territory
TEST_F(AppGatewayCommonTest, AGC_L1_430_SystemDelegate_GetCountryCode_ITA)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"ITA\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"IT\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - GBR territory
TEST_F(AppGatewayCommonTest, AGC_L1_431_SystemDelegate_GetCountryCode_GBR)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"GBR\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"GB\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - IRL territory
TEST_F(AppGatewayCommonTest, AGC_L1_432_SystemDelegate_GetCountryCode_IRL)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"IRL\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"IE\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - AUS territory
TEST_F(AppGatewayCommonTest, AGC_L1_433_SystemDelegate_GetCountryCode_AUS)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"AUS\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"AU\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - AUT territory
TEST_F(AppGatewayCommonTest, AGC_L1_434_SystemDelegate_GetCountryCode_AUT)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"AUT\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"AT\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - CHE territory
TEST_F(AppGatewayCommonTest, AGC_L1_435_SystemDelegate_GetCountryCode_CHE)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"CHE\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"CH\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - DEU territory
TEST_F(AppGatewayCommonTest, AGC_L1_436_SystemDelegate_GetCountryCode_DEU)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"DEU\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"DE\"", code);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetCountryCode - Unknown territory returns empty
TEST_F(AppGatewayCommonTest, AGC_L1_437_SystemDelegate_GetCountryCode_UnknownTerritory)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTerritory", "{\"territory\":\"XYZ\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string code;
    Core::hresult result = systemDelegate->GetCountryCode(code);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"\"", code);  // Unknown territory returns empty
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetCountryCode - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_438_SystemDelegate_SetCountryCode_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetCountryCode("US");
    
    // When invoke fails, returns ERROR_GENERAL
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetCountryCode - Success
TEST_F(AppGatewayCommonTest, AGC_L1_439_SystemDelegate_SetCountryCode_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setTerritory", "{\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetCountryCode("US");
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetCountryCode - Failure
TEST_F(AppGatewayCommonTest, AGC_L1_440_SystemDelegate_SetCountryCode_Failure)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setTerritory", "{\"success\":false}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetCountryCode("US");
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetCountryCode - Country code conversions
TEST_F(AppGatewayCommonTest, AGC_L1_441_SystemDelegate_SetCountryCode_Conversions)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setTerritory", "{\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    // Test various country code conversions
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("CA"));  // CAN
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("IT"));  // ITA
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("GB"));  // GBR
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("IE"));  // IRL
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("AU"));  // AUS
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("AT"));  // AUT
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("CH"));  // CHE
    EXPECT_EQ(Core::ERROR_NONE, systemDelegate->SetCountryCode("DE"));  // DEU
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetTimeZone - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_442_SystemDelegate_GetTimeZone_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string tz;
    Core::hresult result = systemDelegate->GetTimeZone(tz);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetTimeZone - Success
TEST_F(AppGatewayCommonTest, AGC_L1_443_SystemDelegate_GetTimeZone_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTimeZoneDST", "{\"timeZone\":\"America/New_York\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string tz;
    Core::hresult result = systemDelegate->GetTimeZone(tz);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"America/New_York\"", tz);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetTimeZone - Failure
TEST_F(AppGatewayCommonTest, AGC_L1_444_SystemDelegate_GetTimeZone_Failure)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTimeZoneDST", "{\"success\":false}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string tz;
    Core::hresult result = systemDelegate->GetTimeZone(tz);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetTimeZone - Link unavailable
TEST_F(AppGatewayCommonTest, AGC_L1_445_SystemDelegate_SetTimeZone_LinkUnavailable)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetTimeZone("America/New_York");
    
    // When invoke fails, returns ERROR_GENERAL
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetTimeZone - Success
TEST_F(AppGatewayCommonTest, AGC_L1_446_SystemDelegate_SetTimeZone_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setTimeZoneDST", "{\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetTimeZone("America/New_York");
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::SetTimeZone - Failure
TEST_F(AppGatewayCommonTest, AGC_L1_447_SystemDelegate_SetTimeZone_Failure)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("setTimeZoneDST", "{\"success\":false}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    Core::hresult result = systemDelegate->SetTimeZone("America/New_York");
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetSecondScreenFriendlyName - Alias to GetDeviceName
TEST_F(AppGatewayCommonTest, AGC_L1_448_SystemDelegate_GetSecondScreenFriendlyName)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getFriendlyName", "{\"friendlyName\":\"My Device\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string name;
    Core::hresult result = systemDelegate->GetSecondScreenFriendlyName(name);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"My Device\"", name);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetScreenResolution - Success with w/h at top level
TEST_F(AppGatewayCommonTest, AGC_L1_449_SystemDelegate_GetScreenResolution_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getCurrentResolution", "{\"w\":3840,\"h\":2160,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string resolution;
    Core::hresult result = systemDelegate->GetScreenResolution(resolution);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("[3840,2160]", resolution);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetScreenResolution - Invoke fails returns default
TEST_F(AppGatewayCommonTest, AGC_L1_450_SystemDelegate_GetScreenResolution_InvokeFails)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string resolution;
    Core::hresult result = systemDelegate->GetScreenResolution(resolution);
    
    // Returns ERROR_GENERAL when invoke fails
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    // Default value is still set
    EXPECT_EQ("[1920,1080]", resolution);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetScreenResolution - Nested result with w/h
TEST_F(AppGatewayCommonTest, AGC_L1_451_SystemDelegate_GetScreenResolution_NestedResult)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getCurrentResolution", "{\"result\":{\"w\":1280,\"h\":720},\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string resolution;
    Core::hresult result = systemDelegate->GetScreenResolution(resolution);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("[1280,720]", resolution);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetScreenResolution - Nested result with width/height
TEST_F(AppGatewayCommonTest, AGC_L1_452_SystemDelegate_GetScreenResolution_NestedWidthHeight)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getCurrentResolution", "{\"result\":{\"width\":1920,\"height\":1080},\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string resolution;
    Core::hresult result = systemDelegate->GetScreenResolution(resolution);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("[1920,1080]", resolution);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetVideoResolution - 4K screen gives 4K video
TEST_F(AppGatewayCommonTest, AGC_L1_453_SystemDelegate_GetVideoResolution_UHD)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getCurrentResolution", "{\"w\":3840,\"h\":2160,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string resolution;
    Core::hresult result = systemDelegate->GetVideoResolution(resolution);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("[3840,2160]", resolution);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetVideoResolution - HD screen gives HD video
TEST_F(AppGatewayCommonTest, AGC_L1_454_SystemDelegate_GetVideoResolution_FHD)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getCurrentResolution", "{\"w\":1920,\"h\":1080,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string resolution;
    Core::hresult result = systemDelegate->GetVideoResolution(resolution);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("[1920,1080]", resolution);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdcp - HDCP 1.4 detected
TEST_F(AppGatewayCommonTest, AGC_L1_455_SystemDelegate_GetHdcp_14)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getHDCPStatus", "{\"HDCPStatus\":{\"hdcpReason\":2,\"currentHDCPVersion\":\"1.4\"},\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.HdcpProfile") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdcp;
    Core::hresult result = systemDelegate->GetHdcp(hdcp);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdcp1.4\":true,\"hdcp2.2\":false}", hdcp);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdcp - HDCP 2.2 detected
TEST_F(AppGatewayCommonTest, AGC_L1_456_SystemDelegate_GetHdcp_22)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getHDCPStatus", "{\"HDCPStatus\":{\"hdcpReason\":2,\"currentHDCPVersion\":\"2.2\"},\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.HdcpProfile") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdcp;
    Core::hresult result = systemDelegate->GetHdcp(hdcp);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":true}", hdcp);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdcp - Invoke fails returns default
TEST_F(AppGatewayCommonTest, AGC_L1_457_SystemDelegate_GetHdcp_InvokeFails)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdcp;
    Core::hresult result = systemDelegate->GetHdcp(hdcp);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":false}", hdcp);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdcp - Nested result structure
TEST_F(AppGatewayCommonTest, AGC_L1_458_SystemDelegate_GetHdcp_NestedResult)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getHDCPStatus", "{\"result\":{\"success\":true,\"HDCPStatus\":{\"hdcpReason\":2,\"currentHDCPVersion\":\"2.2\"}}}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.HdcpProfile") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdcp;
    Core::hresult result = systemDelegate->GetHdcp(hdcp);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":true}", hdcp);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdr - HDR10 capability
TEST_F(AppGatewayCommonTest, AGC_L1_459_SystemDelegate_GetHdr_HDR10)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    // capabilities=1 is HDRSTANDARD_HDR10
    mockDispatcher->SetResponse("getTVHDRCapabilities", "{\"capabilities\":1,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdr;
    Core::hresult result = systemDelegate->GetHdr(hdr);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdr10\":true,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}", hdr);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdr - Dolby Vision capability
TEST_F(AppGatewayCommonTest, AGC_L1_460_SystemDelegate_GetHdr_DolbyVision)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    // capabilities=4 is HDRSTANDARD_DolbyVision
    mockDispatcher->SetResponse("getTVHDRCapabilities", "{\"capabilities\":4,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdr;
    Core::hresult result = systemDelegate->GetHdr(hdr);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":true,\"hlg\":false,\"hdr10Plus\":false}", hdr);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdr - HLG capability
TEST_F(AppGatewayCommonTest, AGC_L1_461_SystemDelegate_GetHdr_HLG)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    // capabilities=2 is HDRSTANDARD_HLG
    mockDispatcher->SetResponse("getTVHDRCapabilities", "{\"capabilities\":2,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdr;
    Core::hresult result = systemDelegate->GetHdr(hdr);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":true,\"hdr10Plus\":false}", hdr);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdr - HDR10Plus capability
TEST_F(AppGatewayCommonTest, AGC_L1_462_SystemDelegate_GetHdr_HDR10Plus)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    // capabilities=0x10 is HDRSTANDARD_HDR10PLUS
    mockDispatcher->SetResponse("getTVHDRCapabilities", "{\"capabilities\":16,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdr;
    Core::hresult result = systemDelegate->GetHdr(hdr);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":true}", hdr);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdr - Multiple HDR capabilities
TEST_F(AppGatewayCommonTest, AGC_L1_463_SystemDelegate_GetHdr_Multiple)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    // capabilities=0x17 = HDR10 (0x01) + HLG (0x02) + DolbyVision (0x04) + HDR10Plus (0x10)
    mockDispatcher->SetResponse("getTVHDRCapabilities", "{\"capabilities\":23,\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdr;
    Core::hresult result = systemDelegate->GetHdr(hdr);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"hdr10\":true,\"dolbyVision\":true,\"hlg\":true,\"hdr10Plus\":true}", hdr);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetHdr - Invoke fails
TEST_F(AppGatewayCommonTest, AGC_L1_464_SystemDelegate_GetHdr_InvokeFails)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string hdr;
    Core::hresult result = systemDelegate->GetHdr(hdr);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}", hdr);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Stereo PCM
TEST_F(AppGatewayCommonTest, AGC_L1_465_SystemDelegate_GetAudio_Stereo)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"PCM\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Dolby Digital (AC3)
TEST_F(AppGatewayCommonTest, AGC_L1_466_SystemDelegate_GetAudio_DolbyDigital)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"AC3\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":false,\"dolbyDigital5.1\":true,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Dolby Digital Plus (EAC3)
TEST_F(AppGatewayCommonTest, AGC_L1_467_SystemDelegate_GetAudio_DolbyDigitalPlus)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"EAC3\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":true,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Dolby Atmos
TEST_F(AppGatewayCommonTest, AGC_L1_468_SystemDelegate_GetAudio_Atmos)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"ATMOS\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":true}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Multiple formats
TEST_F(AppGatewayCommonTest, AGC_L1_469_SystemDelegate_GetAudio_Multiple)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"PCM\",\"AC3\",\"EAC3\",\"ATMOS\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":true,\"dolbyDigital5.1\":true,\"dolbyDigital5.1+\":true,\"dolbyAtmos\":true}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Invoke fails
TEST_F(AppGatewayCommonTest, AGC_L1_470_SystemDelegate_GetAudio_InvokeFails)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_GENERAL, result);
    EXPECT_EQ("{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - Nested result structure
TEST_F(AppGatewayCommonTest, AGC_L1_471_SystemDelegate_GetAudio_NestedResult)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"result\":{\"supportedAudioFormat\":[\"STEREO\",\"DOLBY DIGITAL\"],\"success\":true}}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":true,\"dolbyDigital5.1\":true,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - DD+ format
TEST_F(AppGatewayCommonTest, AGC_L1_472_SystemDelegate_GetAudio_DDPlus)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"DD+\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":true,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetAudio - AC4 format
TEST_F(AppGatewayCommonTest, AGC_L1_473_SystemDelegate_GetAudio_AC4)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getAudioFormat", "{\"supportedAudioFormat\":[\"AC4\"],\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.DisplaySettings") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string audio;
    Core::hresult result = systemDelegate->GetAudio(audio);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":true,\"dolbyAtmos\":false}", audio);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Success
TEST_F(AppGatewayCommonTest, AGC_L1_474_SystemDelegate_GetDeviceSku_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"stbVersion\":\"AX061AEI_12345\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"AX061AEI\"", sku);  // Extracts portion before underscore
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - No underscore in stbVersion
TEST_F(AppGatewayCommonTest, AGC_L1_475_SystemDelegate_GetDeviceSku_NoUnderscore)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"stbVersion\":\"WHOLESTING\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"WHOLESTING\"", sku);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Missing stbVersion
TEST_F(AppGatewayCommonTest, AGC_L1_476_SystemDelegate_GetDeviceSku_MissingStbVersion)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getSystemVersions", "{\"otherField\":\"value\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    EXPECT_TRUE(sku.empty());
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetDeviceSku - Invoke fails
TEST_F(AppGatewayCommonTest, AGC_L1_477_SystemDelegate_GetDeviceSku_InvokeFails)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string sku;
    Core::hresult result = systemDelegate->GetDeviceSku(sku);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetFirmwareVersion - Success
TEST_F(AppGatewayCommonTest, AGC_L1_478_SystemDelegate_GetFirmwareVersion_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    // GetFirmwareVersion needs receiverVersion (format x.x.x.x) and stbVersion
    mockDispatcher->SetResponse("getSystemVersions", "{\"receiverVersion\":\"1.2.3.456\",\"stbVersion\":\"TestFirmware_123\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string version;
    Core::hresult result = systemDelegate->GetFirmwareVersion(version);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    // Version is a complex JSON with api, firmware, os, debug fields
    EXPECT_FALSE(version.empty());
    EXPECT_NE(version.find("firmware"), string::npos);
    EXPECT_NE(version.find("major"), string::npos);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetTimeZone - Success
TEST_F(AppGatewayCommonTest, AGC_L1_479_SystemDelegate_GetTimeZone_Success)
{
    MockLocalDispatcher* mockDispatcher = new MockLocalDispatcher();
    testing::Mock::AllowLeak(mockDispatcher);
    mockDispatcher->SetResponse("getTimeZoneDST", "{\"timeZone\":\"America/New_York\",\"success\":true}");
    
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly([mockDispatcher](const uint32_t id, const string& callsign) -> void* {
            if (id == PluginHost::ILocalDispatcher::ID && callsign == "org.rdk.System") {
                mockDispatcher->AddRef();
                return static_cast<void*>(mockDispatcher);
            }
            return nullptr;
        });
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string tz;
    Core::hresult result = systemDelegate->GetTimeZone(tz);
    
    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ("\"America/New_York\"", tz);
    
    plugin.Deinitialize(&service);
}

// Test SystemDelegate::GetTimeZone - Invoke fails
TEST_F(AppGatewayCommonTest, AGC_L1_480_SystemDelegate_GetTimeZone_InvokeFails)
{
    NiceMock<ServiceMock> service;
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const string initResponse = plugin.Initialize(&service);
    EXPECT_TRUE(initResponse.empty());
    
    auto systemDelegate = plugin.mDelegate->getSystemDelegate();
    ASSERT_NE(systemDelegate, nullptr);
    
    string tz;
    Core::hresult result = systemDelegate->GetTimeZone(tz);
    
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, result);
    
    plugin.Deinitialize(&service);
}

} // namespace
