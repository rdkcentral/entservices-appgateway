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
#undef private

#include "ServiceMock.h"
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

} // namespace
