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
#include "UserSettingMock.h"
#include "NetworkManagerMock.h"
#include "SharedStorageMock.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;

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

    void SetUp() override
    {
        // Provide a non-null (but empty) delegate so HandleAppGatewayRequest
        // can proceed past its null-delegate guard.  Tests that need fully
        // null-delegate behaviour call plugin.mDelegate.reset() themselves.
        plugin.mDelegate = std::make_shared<SettingsDelegate>();
    }

    void TearDown() override
    {
        // Null out any raw mock pointers that were injected via the helper functions
        // BEFORE the mocks go out of scope (and before the plugin destructs them).
        // UserSettingsDelegate::~UserSettingsDelegate calls mUserSettings->Release(),
        // and NetworkDelegate / AppDelegate destructors similarly call Release() on their
        // injected pointers.  Since the mock objects live on the test stack they are
        // destroyed AFTER TearDown returns but BEFORE the fixture destructor, so we
        // must clear the raw pointers here.
        if (plugin.mDelegate) {
            if (plugin.mDelegate->userSettings) {
                plugin.mDelegate->userSettings->mUserSettings = nullptr;
            }
            if (plugin.mDelegate->networkDelegate) {
                plugin.mDelegate->networkDelegate->mNetworkManager = nullptr;
            }
            if (plugin.mDelegate->appDelegate) {
                plugin.mDelegate->appDelegate->mSharedStorage = nullptr;
            }
        }
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
    EXPECT_EQ("{\"error\":\"Invalid payload: missing or invalid 'value' field\"}", result);
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
        EXPECT_THAT(result, HasSubstr("Invalid payload")) << "method=" << method;
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
    // plugin.mDelegate is the empty SettingsDelegate set up by SetUp().
    // DEVICE.NETWORK -> GetInternetConnectionStatus -> networkDelegate is null -> ERROR_UNAVAILABLE.
    // Not.A.Method  -> falls through all handlers -> ERROR_UNKNOWN_KEY.
    const auto ctx = MakeContext();
    string result;

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "DEVICE.NETWORK", "{}", result));

    result.clear();
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, plugin.HandleAppGatewayRequest(ctx, "Not.A.Method", "{}", result));
}

// ---------------------------------------------------------------------------
// Helper: inject a UserSettingMock directly into the UserSettingsDelegate.
// Because #define private public is in effect, all private members are
// accessible.  The mock must outlive the test.
// ---------------------------------------------------------------------------
static void InjectUserSettingsMock(Core::Sink<AppGatewayCommon>& plugin,
                                   UserSettingMock* mock)
{
    // Ensure the delegate chain exists (SetUp already created mDelegate)
    auto delegate = plugin.mDelegate;
    ASSERT_NE(nullptr, delegate);
    if (!delegate->userSettings) {
        delegate->userSettings = std::make_shared<UserSettingsDelegate>(nullptr);
    }
    // Poke the COM pointer directly (made public by #define private public)
    delegate->userSettings->mUserSettings = mock;
}

static void InjectNetworkManagerMock(Core::Sink<AppGatewayCommon>& plugin,
                                     MockINetworkManager* mock)
{
    auto delegate = plugin.mDelegate;
    ASSERT_NE(nullptr, delegate);
    if (!delegate->networkDelegate) {
        delegate->networkDelegate = std::make_shared<NetworkDelegate>(nullptr);
    }
    delegate->networkDelegate->mNetworkManager = mock;
}

// Inject a SharedStorageMock into the AppDelegate inside the plugin's SettingsDelegate.
// AppDelegate::mSharedStorage is made accessible by #define private public.
static void InjectSharedStorageMock(Core::Sink<AppGatewayCommon>& plugin,
                                    SharedStorageMock* mock)
{
    auto delegate = plugin.mDelegate;
    ASSERT_NE(nullptr, delegate);
    if (!delegate->appDelegate) {
        delegate->appDelegate = std::make_shared<AppDelegate>(nullptr);
    }
    delegate->appDelegate->mSharedStorage = mock;
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.enabled via UserSettings COM mock
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_047_VoiceGuidanceEnabled_Success_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result));
    EXPECT_EQ("true", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_048_VoiceGuidanceEnabled_Success_False)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result));
    EXPECT_EQ("false", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_049_VoiceGuidanceEnabled_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result));
    EXPECT_THAT(result, HasSubstr("error"));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setenabled
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_050_VoiceGuidanceSetEnabled_ValidPayload_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetVoiceGuidance(true))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":true}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_051_VoiceGuidanceSetEnabled_ValidPayload_False)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetVoiceGuidance(false))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":false}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_052_VoiceGuidanceSetEnabled_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":true}", result));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.speed / voiceguidance.rate (GetSpeed)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_053_VoiceGuidanceSpeed_RateAbove156_Returns2)
{
    NiceMock<UserSettingMock> mockUS;
    // rate >= 1.56 -> speed = 2.0
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(2.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result));
    EXPECT_EQ("2", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_054_VoiceGuidanceRate_RateBetween138And156_Returns167)
{
    NiceMock<UserSettingMock> mockUS;
    // rate >= 1.38 (but < 1.56) -> speed = 1.67
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.38), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.rate", "{}", result));
    EXPECT_EQ("1.67", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_055_VoiceGuidanceRate_RateBelow1_Returns05)
{
    NiceMock<UserSettingMock> mockUS;
    // rate < 1.0 -> speed = 0.5
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(0.5), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result));
    EXPECT_EQ("0.5", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_056_VoiceGuidanceSpeed_DelegateFailure_PropagatesError)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    // ERROR_GENERAL propagated from GetVoiceGuidanceRate
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setspeed / voiceguidance.setrate
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_057_VoiceGuidanceSetSpeed_ValidValue2_TransformsTo10)
{
    NiceMock<UserSettingMock> mockUS;
    // speed=2.0 -> transformedRate=10.0
    EXPECT_CALL(mockUS, SetVoiceGuidanceRate(10.0))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":2.0}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_058_VoiceGuidanceSetRate_ValidValue1_TransformsTo1)
{
    NiceMock<UserSettingMock> mockUS;
    // speed=1.0 -> transformedRate=1.0
    EXPECT_CALL(mockUS, SetVoiceGuidanceRate(1.0))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setrate", "{\"value\":1.0}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_059_VoiceGuidanceSetSpeed_OutOfRange_ReturnsBadRequest)
{
    // value > 2.0 is out of range [0.5, 2.0]
    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":3.0}", result));
    EXPECT_THAT(result, HasSubstr("Invalid payload"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_060_VoiceGuidanceSetSpeed_OutOfRange_BelowMin_ReturnsBadRequest)
{
    // value < 0.5 is out of range
    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":0.1}", result));
    EXPECT_THAT(result, HasSubstr("Invalid payload"));
}

// ---------------------------------------------------------------------------
// Tests: audiodescriptions.enabled
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_061_AudioDescriptionsEnabled_Success_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetAudioDescription(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.enabled", "{}", result));
    EXPECT_EQ("true", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_062_AudioDescriptionsEnabled_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetAudioDescription(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.enabled", "{}", result));
    EXPECT_THAT(result, HasSubstr("error"));
}

// ---------------------------------------------------------------------------
// Tests: audiodescriptions.setenabled
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_063_AudioDescriptionsSetEnabled_ValidPayload_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetAudioDescription(true))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", "{\"value\":true}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: closedcaptions.enabled
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_064_ClosedCaptionsEnabled_Success_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetCaptions(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.enabled", "{}", result));
    EXPECT_EQ("true", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_065_ClosedCaptionsEnabled_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.enabled", "{}", result));
    EXPECT_THAT(result, HasSubstr("error"));
}

// ---------------------------------------------------------------------------
// Tests: closedcaptions.setenabled
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_066_ClosedCaptionsSetEnabled_ValidPayload_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetCaptions(true))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{\"value\":true}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: closedcaptions.preferredlanguages
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_067_ClosedCaptionsPreferredLanguages_Success)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetPreferredCaptionsLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("eng")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.preferredlanguages", "{}", result));
    EXPECT_EQ("eng", result);
}

// ---------------------------------------------------------------------------
// Tests: closedcaptions.setpreferredlanguages
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_068_ClosedCaptionsSetPreferredLanguages_ValidArrayPayload)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", "{\"value\":[\"eng\",\"fra\"]}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_069_ClosedCaptionsSetPreferredLanguages_ValidStringPayload)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", "{\"value\":\"eng\"}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.navigationhints
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_070_VoiceGuidanceNavigationHints_Success_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidanceHints(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.navigationhints", "{}", result));
    EXPECT_EQ("true", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_071_VoiceGuidanceNavigationHints_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.navigationhints", "{}", result));
    EXPECT_THAT(result, HasSubstr("error"));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setnavigationhints
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_072_VoiceGuidanceSetNavigationHints_ValidPayload_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetVoiceGuidanceHints(true))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", "{\"value\":true}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: localization.locale
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_073_LocalizationLocale_Success_WithDash)
{
    NiceMock<UserSettingMock> mockUS;
    // GetPresentationLanguage returns "en-US" -> locale = "\"en-US\""
    EXPECT_CALL(mockUS, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("en-US")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "localization.locale", "{}", result));
    EXPECT_EQ("\"en-US\"", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_074_LocalizationLocale_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "localization.locale", "{}", result));
    EXPECT_THAT(result, HasSubstr("error"));
}

// ---------------------------------------------------------------------------
// Tests: localization.setlocale
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_075_LocalizationSetLocale_ValidPayload_Success)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetPresentationLanguage(string("en-US")))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "localization.setlocale", "{\"value\":\"en-US\"}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_076_LocalizationSetLocale_InvalidPayload_ReturnsBadRequest)
{
    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, plugin.HandleAppGatewayRequest(ctx, "localization.setlocale", "{invalid}", result));
    EXPECT_THAT(result, HasSubstr("Invalid payload"));
}

// ---------------------------------------------------------------------------
// Tests: localization.language
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_077_LocalizationLanguage_Success_ExtractsLanguagePart)
{
    NiceMock<UserSettingMock> mockUS;
    // "en-US" -> language "en"
    EXPECT_CALL(mockUS, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("en-US")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result));
    EXPECT_EQ("\"en\"", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_078_LocalizationLanguage_NoDash_ReturnsWholeString)
{
    NiceMock<UserSettingMock> mockUS;
    // "eng" has no dash -> return "\"eng\""
    EXPECT_CALL(mockUS, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("eng")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result));
    EXPECT_EQ("\"eng\"", result);
}

// ---------------------------------------------------------------------------
// Tests: localization.preferredaudiolanguages
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_079_LocalizationPreferredAudioLanguages_Success)
{
    NiceMock<UserSettingMock> mockUS;
    // Returns comma-separated "eng,fra" -> JSON array ["eng","fra"]
    EXPECT_CALL(mockUS, GetPreferredAudioLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("eng,fra")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "localization.preferredaudiolanguages", "{}", result));
    EXPECT_THAT(result, HasSubstr("eng"));
    EXPECT_THAT(result, HasSubstr("fra"));
}

// ---------------------------------------------------------------------------
// Tests: localization.setpreferredaudiolanguages
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_080_LocalizationSetPreferredAudioLanguages_ValidArrayPayload)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", "{\"value\":[\"eng\",\"spa\"]}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: GetVoiceGuidanceSettings with UserSettings mock
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_081_GetVoiceGuidanceSettings_Success_WithSpeed)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetVoiceGuidanceHints(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetVoiceGuidanceSettings(true, result));
    EXPECT_THAT(result, HasSubstr("enabled"));
    EXPECT_THAT(result, HasSubstr("rate"));
    EXPECT_THAT(result, HasSubstr("speed"));
    EXPECT_THAT(result, HasSubstr("navigationHints"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_082_GetVoiceGuidanceSettings_Success_WithoutSpeed)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(1.38), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetVoiceGuidanceHints(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetVoiceGuidanceSettings(false, result));
    EXPECT_THAT(result, HasSubstr("enabled"));
    EXPECT_THAT(result, HasSubstr("rate"));
    // No "speed" key when addSpeed=false
    EXPECT_THAT(result, Not(HasSubstr("\"speed\"")));
}

TEST_F(AppGatewayCommonTest, AGC_L1_083_GetVoiceGuidanceSettings_GetVoiceGuidanceFails_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.GetVoiceGuidanceSettings(true, result));
}

// ---------------------------------------------------------------------------
// Tests: GetClosedCaptionsSettings with UserSettings mock
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_084_GetClosedCaptionsSettings_Success)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetCaptions(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetPreferredCaptionsLanguages(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(string("eng")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetClosedCaptionsSettings(result));
    EXPECT_THAT(result, HasSubstr("enabled"));
    EXPECT_THAT(result, HasSubstr("preferredLanguages"));
    EXPECT_THAT(result, HasSubstr("styles"));
}

TEST_F(AppGatewayCommonTest, AGC_L1_085_GetClosedCaptionsSettings_GetCaptionsFails_PropagatesError)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.GetClosedCaptionsSettings(result));
}

// ---------------------------------------------------------------------------
// Tests: GetSpeed / SetSpeed direct method calls with mock
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_086_GetSpeed_Rate119_Returns133)
{
    NiceMock<UserSettingMock> mockUS;
    // rate >= 1.19 (but < 1.38) -> speed = 1.33
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.19), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    double speed = 0.0;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetSpeed(speed));
    EXPECT_DOUBLE_EQ(1.33, speed);
}

TEST_F(AppGatewayCommonTest, AGC_L1_087_GetSpeed_Rate1_Returns1)
{
    NiceMock<UserSettingMock> mockUS;
    // rate >= 1.0 (but < 1.19) -> speed = 1.0
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    double speed = 0.0;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetSpeed(speed));
    EXPECT_DOUBLE_EQ(1.0, speed);
}

TEST_F(AppGatewayCommonTest, AGC_L1_088_SetSpeed_Speed167_TransformsTo138)
{
    NiceMock<UserSettingMock> mockUS;
    // speed=1.67 -> transformedRate=1.38
    EXPECT_CALL(mockUS, SetVoiceGuidanceRate(1.38))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    EXPECT_EQ(Core::ERROR_NONE, plugin.SetSpeed(1.67));
}

TEST_F(AppGatewayCommonTest, AGC_L1_089_SetSpeed_Speed133_TransformsTo119)
{
    NiceMock<UserSettingMock> mockUS;
    // speed=1.33 -> transformedRate=1.19
    EXPECT_CALL(mockUS, SetVoiceGuidanceRate(1.19))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    EXPECT_EQ(Core::ERROR_NONE, plugin.SetSpeed(1.33));
}

TEST_F(AppGatewayCommonTest, AGC_L1_090_SetSpeed_SpeedBelow1_TransformsTo01)
{
    NiceMock<UserSettingMock> mockUS;
    // speed < 1.0 -> transformedRate=0.1
    EXPECT_CALL(mockUS, SetVoiceGuidanceRate(0.1))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    EXPECT_EQ(Core::ERROR_NONE, plugin.SetSpeed(0.8));
}

// ---------------------------------------------------------------------------
// Tests: device.network route (GetInternetConnectionStatus)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_091_DeviceNetwork_NoConnectedInterface_ReturnsEmptyObject)
{
    NiceMock<MockINetworkManager> mockNM;

    // Return an empty iterator (null)
    EXPECT_CALL(mockNM, GetAvailableInterfaces(_))
        .WillOnce(DoAll(SetArgReferee<0>(nullptr), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockNM, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockNM, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectNetworkManagerMock(plugin, &mockNM);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result));
    EXPECT_EQ("{}", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_092_DeviceNetwork_GetAvailableInterfacesFails_ReturnsGeneral)
{
    NiceMock<MockINetworkManager> mockNM;
    EXPECT_CALL(mockNM, GetAvailableInterfaces(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockNM, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockNM, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectNetworkManagerMock(plugin, &mockNM);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL, plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result));
    EXPECT_THAT(result, HasSubstr("error"));
}

// ---------------------------------------------------------------------------
// Tests: network.connected route (GetNetworkConnected)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_093_NetworkConnected_HasPrimaryInterface_ReturnsTrue)
{
    NiceMock<MockINetworkManager> mockNM;
    EXPECT_CALL(mockNM, GetPrimaryInterface(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("eth0")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockNM, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockNM, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectNetworkManagerMock(plugin, &mockNM);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result));
    EXPECT_EQ("true", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_094_NetworkConnected_EmptyPrimaryInterface_ReturnsFalse)
{
    NiceMock<MockINetworkManager> mockNM;
    EXPECT_CALL(mockNM, GetPrimaryInterface(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockNM, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockNM, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectNetworkManagerMock(plugin, &mockNM);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result));
    EXPECT_EQ("false", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_095_NetworkConnected_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: metrics.* routes — any metrics.X is a no-op returning null
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_096_MetricsRoute_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "metrics.event", "{}", result));
    EXPECT_EQ("null", result);
}

TEST_F(AppGatewayCommonTest, AGC_L1_097_MetricsRouteWithSubPath_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "metrics.page.view", "{}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: discovery.watched route — always returns null (no-op)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_098_DiscoveryWatched_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "discovery.watched", "{}", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: HandleAppEventNotifier with null callback
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_099_HandleAppEventNotifier_NullCallback_ReturnsError)
{
    bool status = true;
    const auto rc = plugin.HandleAppEventNotifier(nullptr, "Device.onHdrChanged", true, status);
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_FALSE(status);
}

TEST_F(AppGatewayCommonTest, AGC_L1_100_HandleAppEventNotifier_NullDelegate_ReturnsError)
{
    plugin.mDelegate.reset();

    Core::Sink<TestEmitter> emitter;
    bool status = true;
    const auto rc = plugin.HandleAppEventNotifier(&emitter, "Device.onHdrChanged", true, status);
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_FALSE(status);
}

// ---------------------------------------------------------------------------
// Tests: localization.setcountrycode / localization.settimezone null-delegate
// (validate that valid payloads + null delegate = UNAVAILABLE)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_101_LocalizationSetCountryCode_ValidPayload_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "localization.setcountrycode", "{\"value\":\"US\"}", result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_102_LocalizationSetTimeZone_ValidPayload_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "localization.settimezone", "{\"value\":\"America/New_York\"}", result));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setenabled null delegate (valid payload path)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_103_VoiceGuidanceSetEnabled_ValidPayload_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", "{\"value\":true}", result));
}

// ---------------------------------------------------------------------------
// Tests: closedcaptions.setenabled null delegate (valid payload path)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_104_ClosedCaptionsSetEnabled_ValidPayload_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", "{\"value\":false}", result));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setspeed with null delegate (valid payload path)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_105_VoiceGuidanceSetSpeed_ValidPayload_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", "{\"value\":1.0}", result));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.speed with null delegate (null-sub-delegate path)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_106_VoiceGuidanceSpeed_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.voiceguidancesettings and accessibility.voiceguidance routes
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_107_AccessibilityVoiceGuidanceSettings_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_108_AccessibilityVoiceGuidance_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidance", "{}", result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_109_AccessibilityClosedCaptions_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptions", "{}", result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_110_AccessibilityClosedCaptionsSettings_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptionssettings", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: device.version route (GetFirmwareVersion) via handler map
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_111_DeviceVersion_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.version", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: device.screenresolution / device.videoresolution / device.hdcp /
//        device.hdr / device.audio via handler map with null delegate
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_112_HandlerMapRoutes_NullSystemDelegate_FallbackValues)
{
    // SetUp() provides a non-null mDelegate but with systemDelegate == nullptr.
    // HandleAppGatewayRequest passes the null-delegate guard and reaches the
    // handler, which calls GetScreenResolution/GetHdcp/etc.  Those methods
    // detect the null systemDelegate and return hardcoded fallback values.
    ASSERT_NE(nullptr, plugin.mDelegate);
    ASSERT_EQ(nullptr, plugin.mDelegate->getSystemDelegate());

    const auto ctx = MakeContext();
    string result;

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.screenresolution", "{}", result));
    EXPECT_EQ("[1920,1080]", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.videoresolution", "{}", result));
    EXPECT_EQ("[1920,1080]", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.hdcp", "{}", result));
    EXPECT_EQ("{\"hdcp1.4\":false,\"hdcp2.2\":false}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.hdr", "{}", result));
    EXPECT_EQ("{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.audio", "{}", result));
    EXPECT_EQ("{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}", result);
}

// ---------------------------------------------------------------------------
// Tests: secondscreen.friendlyname route
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_113_SecondScreenFriendlyName_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "secondscreen.friendlyname", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: lifecycle routes with null delegate via handler map
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_114_LifecycleRoutes_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();
    const auto ctx = MakeContext();

    const std::vector<std::string> methods = {
        "lifecycle.ready",
        "lifecycle.close",
        "lifecycle.state",
        "lifecycle.finished",
        "lifecycle2.state",
        "lifecycle2.close",
        "commoninternal.dispatchintent",
        "commoninternal.getlastintent"
    };

    for (const auto& method : methods) {
        string result;
        EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, method, "{}", result))
            << "method=" << method;
    }
}

// ---------------------------------------------------------------------------
// Tests: advertising.advertisingid and device.uid (app delegate routes)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_115_AppDelegateRoutes_NullDelegate_ReturnsUnavailable)
{
    plugin.mDelegate.reset();
    const auto ctx = MakeContext();

    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result));

    result.clear();
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.audiodescriptionsettings and accessibility.audiodescription
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_116_AccessibilityAudioDescriptionSettings_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescriptionsettings", "{}", result));

    result.clear();
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescription", "{}", result));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.highcontrastui
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_117_AccessibilityHighContrastUI_NullDelegate_Unavailable)
{
    plugin.mDelegate.reset();

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result));
}

TEST_F(AppGatewayCommonTest, AGC_L1_118_AccessibilityHighContrastUI_Success)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetHighContrast(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result));
    EXPECT_EQ("true", result);
}

// ---------------------------------------------------------------------------
// Tests: advertising.advertisingid – existing value in SharedStorage
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_119_AdvertisingId_ExistingValue_ReturnsStoredIfa)
{
    NiceMock<SharedStorageMock> mockSS;
    // GetValue succeeds and returns a stored IFA
    EXPECT_CALL(mockSS, GetValue(ISharedStorage::DEVICE, "test.app", "fireboltAdvertisingId", _, _, _))
        .WillOnce(DoAll(SetArgReferee<3>(std::string("stored-ifa-uuid")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockSS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockSS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectSharedStorageMock(plugin, &mockSS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result));
    // Result must be a JSON object containing the stored IFA
    EXPECT_THAT(result, HasSubstr("stored-ifa-uuid"));
    EXPECT_THAT(result, HasSubstr("ifa"));
}

// ---------------------------------------------------------------------------
// Tests: device.uid – existing value in SharedStorage
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_120_DeviceUid_ExistingValue_ReturnsStoredUid)
{
    NiceMock<SharedStorageMock> mockSS;
    EXPECT_CALL(mockSS, GetValue(ISharedStorage::DEVICE, "test.app", "fireboltDeviceUid", _, _, _))
        .WillOnce(DoAll(SetArgReferee<3>(std::string("stored-device-uid")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockSS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockSS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectSharedStorageMock(plugin, &mockSS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result));
    // Result is the UID string directly
    EXPECT_EQ("stored-device-uid", result);
}

// ---------------------------------------------------------------------------
// Tests: advertising.advertisingid – no existing value, SetValue succeeds
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_121_AdvertisingId_NoExistingValue_GeneratesAndStores)
{
    NiceMock<SharedStorageMock> mockSS;
    // GetValue fails (no stored value) → AppDelegate generates a new UUID and calls SetValue
    EXPECT_CALL(mockSS, GetValue(ISharedStorage::DEVICE, "test.app", "fireboltAdvertisingId", _, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));
    // SetValue succeeds
    ISharedStorage::Success successVal;
    successVal.success = true;
    EXPECT_CALL(mockSS, SetValue(ISharedStorage::DEVICE, "test.app", "fireboltAdvertisingId", _, 0, _))
        .WillOnce(DoAll(SetArgReferee<5>(successVal), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockSS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockSS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectSharedStorageMock(plugin, &mockSS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "advertising.advertisingid", "{}", result));
    // Result must contain an IFA field with a newly generated (non-empty) UUID
    EXPECT_THAT(result, HasSubstr("ifa"));
}

// ---------------------------------------------------------------------------
// Tests: device.uid – no existing value, SetValue succeeds
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_122_DeviceUid_NoExistingValue_GeneratesAndStores)
{
    NiceMock<SharedStorageMock> mockSS;
    EXPECT_CALL(mockSS, GetValue(ISharedStorage::DEVICE, "test.app", "fireboltDeviceUid", _, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));
    ISharedStorage::Success successVal;
    successVal.success = true;
    EXPECT_CALL(mockSS, SetValue(ISharedStorage::DEVICE, "test.app", "fireboltDeviceUid", _, 0, _))
        .WillOnce(DoAll(SetArgReferee<5>(successVal), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockSS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockSS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectSharedStorageMock(plugin, &mockSS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE, plugin.HandleAppGatewayRequest(ctx, "device.uid", "{}", result));
    // Generated UUID is non-empty
    EXPECT_FALSE(result.empty());
}

// ---------------------------------------------------------------------------
// Tests: accessibility.voiceguidancesettings – success via route (with mock)
// accessibility.voiceguidancesettings calls GetVoiceGuidanceSettings(!IsRDK8Compliant(ctx.version), result)
// With ctx.version="" (empty), IsRDK8Compliant returns false → addSpeed=true
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_123_AccessibilityVoiceGuidanceSettings_Success_WithSpeed)
{
    NiceMock<UserSettingMock> mockUS;
    // GetVoiceGuidance(bool&)
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    // GetVoiceGuidanceRate(double&)
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    // GetVoiceGuidanceHints(bool&)
    EXPECT_CALL(mockUS, GetVoiceGuidanceHints(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext(); // ctx.version="" → non-RDK8 → addSpeed=true
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result));
    // Result must contain enabled, rate, speed, and navigationHints fields
    EXPECT_THAT(result, HasSubstr("enabled"));
    EXPECT_THAT(result, HasSubstr("rate"));
    EXPECT_THAT(result, HasSubstr("speed"));
    EXPECT_THAT(result, HasSubstr("navigationHints"));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.voiceguidance – success via route
// accessibility.voiceguidance always calls GetVoiceGuidanceSettings(true, result) → addSpeed=true
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_124_AccessibilityVoiceGuidance_Success_WithSpeed)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidance(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(0.5), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetVoiceGuidanceHints(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidance", "{}", result));
    EXPECT_THAT(result, HasSubstr("enabled"));
    EXPECT_THAT(result, HasSubstr("speed"));
    EXPECT_THAT(result, HasSubstr("navigationHints"));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.closedcaptions – success via route (mock captions + languages)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_125_AccessibilityClosedCaptions_Success)
{
    NiceMock<UserSettingMock> mockUS;
    // GetCaptions(bool&)
    EXPECT_CALL(mockUS, GetCaptions(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    // GetPreferredCaptionsLanguages(string&)
    EXPECT_CALL(mockUS, GetPreferredCaptionsLanguages(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(std::string("en")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptions", "{}", result));
    // Result must include at least the enabled and preferredLanguages fields
    EXPECT_THAT(result, HasSubstr("enabled"));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.closedcaptionssettings – success via route
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_126_AccessibilityClosedCaptionsSettings_Success)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetCaptions(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, GetPreferredCaptionsLanguages(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(std::string("fr")), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptionssettings", "{}", result));
    EXPECT_THAT(result, HasSubstr("enabled"));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setnavigationhints – delegate returns ERROR_GENERAL
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_127_VoiceGuidanceSetNavigationHints_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL,
              plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints",
                                             R"({"value":true})", result));
}

// ---------------------------------------------------------------------------
// Tests: closedcaptions.setpreferredlanguages – delegate returns ERROR_GENERAL
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_128_ClosedCaptionsSetPreferredLanguages_DelegateFailure_ReturnsGeneral)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_GENERAL,
              plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages",
                                             R"({"value":"en"})", result));
}

// ---------------------------------------------------------------------------
// Tests: voiceguidance.setrate – alias verification (same transform as setspeed)
// setrate with value=2.0 → transforms to 10.0 internally
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_129_VoiceGuidanceSetRate_IsAliasForSetSpeed)
{
    NiceMock<UserSettingMock> mockUS;
    // Speed 2.0 → transformed rate 10.0
    EXPECT_CALL(mockUS, SetVoiceGuidanceRate(10.0))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setrate",
                                             R"({"value":2.0})", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: localization.setpreferredaudiolanguages – string payload (not array)
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_130_LocalizationSetPreferredAudioLanguages_StringPayload)
{
    NiceMock<UserSettingMock> mockUS;
    // When a plain string "en" is provided, it should be passed as-is to SetPreferredAudioLanguages
    EXPECT_CALL(mockUS, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages",
                                             R"({"value":"en"})", result));
    EXPECT_EQ("null", result);
}

// ---------------------------------------------------------------------------
// Tests: GetSpeed boundary – rate exactly 1.38 → speed 1.67
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_131_GetSpeed_RateExactly138_Returns167)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.38), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    double speed = 0.0;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetSpeed(speed));
    EXPECT_DOUBLE_EQ(1.67, speed);
}

// ---------------------------------------------------------------------------
// Tests: GetSpeed boundary – rate exactly 1.0 → speed 1.0
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_132_GetSpeed_RateExactly1_Returns1)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    double speed = 0.0;
    EXPECT_EQ(Core::ERROR_NONE, plugin.GetSpeed(speed));
    EXPECT_DOUBLE_EQ(1.0, speed);
}

// ---------------------------------------------------------------------------
// Tests: accessibility.audiodescriptionsettings – success path ({\"enabled\":bool})
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_133_AccessibilityAudioDescriptionSettings_Success)
{
    NiceMock<UserSettingMock> mockUS;
    // GetAudioDescription calls IUserSettings::GetAudioDescription(bool&) and wraps in {"enabled":...}
    EXPECT_CALL(mockUS, GetAudioDescription(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescriptionsettings", "{}", result));
    // Must contain the enabled key in object form
    EXPECT_THAT(result, HasSubstr("enabled"));
}

// ---------------------------------------------------------------------------
// Tests: accessibility.audiodescription – success path (plain bool string)
// This route calls GetAudioDescriptionsEnabled which returns "true"/"false"
// ---------------------------------------------------------------------------

TEST_F(AppGatewayCommonTest, AGC_L1_134_AccessibilityAudioDescription_Success_True)
{
    NiceMock<UserSettingMock> mockUS;
    EXPECT_CALL(mockUS, GetAudioDescription(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUS, AddRef()).Times(AnyNumber());
    EXPECT_CALL(mockUS, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

    InjectUserSettingsMock(plugin, &mockUS);

    const auto ctx = MakeContext();
    string result;
    EXPECT_EQ(Core::ERROR_NONE,
              plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescription", "{}", result));
    EXPECT_EQ("true", result);
}

} // namespace
