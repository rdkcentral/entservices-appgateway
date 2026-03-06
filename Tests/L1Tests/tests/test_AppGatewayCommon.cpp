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
    // Set the plugin's mConnectionId to a known value
    plugin.mConnectionId = 12345;
    
    // Create a mock connection with the same ID
    MockRemoteConnection* connection = new MockRemoteConnection(12345);
    
    // Call Deactivated - this should trigger the deactivation logic
    // Note: This requires mShell to be set, which may not be in test environment
    // The test verifies the method can be called without crashing
    plugin.Deactivated(connection);
    
    connection->Release();
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

TEST_F(AppGatewayCommonTest, AGC_L1_177_HandleAppEventNotifier_Subscribe)
{
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Test subscribing to an event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "localization.onlanguagechanged", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Give time for the async job to potentially execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    emitter->Release();
}

TEST_F(AppGatewayCommonTest, AGC_L1_178_HandleAppEventNotifier_Unsubscribe)
{
    MockEmitter* emitter = new MockEmitter();
    bool status = false;
    
    // Test unsubscribing from an event
    Core::hresult rc = plugin.HandleAppEventNotifier(emitter, "localization.onlanguagechanged", false, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Give time for the async job to potentially execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    emitter->Release();
}

TEST_F(AppGatewayCommonTest, AGC_L1_179_HandleAppEventNotifier_NullEmitter)
{
    bool status = false;
    
    // Test with null emitter
    Core::hresult rc = plugin.HandleAppEventNotifier(nullptr, "test.event", true, status);
    
    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
    
    // Give time for the async job to potentially execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(AppGatewayCommonTest, AGC_L1_180_HandleAppEventNotifier_MultipleEvents)
{
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
    
    // Give time for the async jobs to potentially execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    emitter1->Release();
    emitter2->Release();
}

} // namespace
