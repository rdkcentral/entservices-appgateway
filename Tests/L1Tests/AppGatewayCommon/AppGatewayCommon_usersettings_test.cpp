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
#include "MockUserSettings.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;

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

static Exchange::GatewayContext MakeContext()
{
    Exchange::GatewayContext ctx;
    ctx.appId = "test.app";
    ctx.connectionId = 100;
    ctx.requestId = 200;
    return ctx;
}

class UserSettingsTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<MockUserSettings> mockUserSettings;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(service, QueryInterfaceByCallsign(Exchange::IUserSettings::ID, ::testing::StrEq("org.rdk.UserSettings")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                mockUserSettings.AddRef();
                return static_cast<Exchange::IUserSettings*>(&mockUserSettings);
            }));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
        EXPECT_CALL(mockUserSettings, Register(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
        EXPECT_CALL(mockUserSettings, Unregister(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = plugin.Initialize(&service);
        ASSERT_TRUE(response.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

/* ---------- Voice Guidance ---------- */

TEST_F(UserSettingsTest, AGC_L1_054_GetVoiceGuidance_Success)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
}

TEST_F(UserSettingsTest, AGC_L1_055_SetVoiceGuidance_Success)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidance(true))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", R"({"value":true})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- Audio Descriptions ---------- */

TEST_F(UserSettingsTest, AGC_L1_056_GetAudioDescriptionsEnabled_Success)
{
    EXPECT_CALL(mockUserSettings, GetAudioDescription(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.enabled", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
}

TEST_F(UserSettingsTest, AGC_L1_057_SetAudioDescriptionsEnabled_Success)
{
    EXPECT_CALL(mockUserSettings, SetAudioDescription(true))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", R"({"value":true})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- Captions ---------- */

TEST_F(UserSettingsTest, AGC_L1_058_GetCaptions_Success)
{
    EXPECT_CALL(mockUserSettings, GetCaptions(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.enabled", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
}

TEST_F(UserSettingsTest, AGC_L1_059_SetCaptions_Success)
{
    EXPECT_CALL(mockUserSettings, SetCaptions(false))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", R"({"value":false})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- Locale ---------- */

TEST_F(UserSettingsTest, AGC_L1_060_GetLocale_Success)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>("en-US"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.locale", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("en"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_061_SetLocale_Success)
{
    EXPECT_CALL(mockUserSettings, SetPresentationLanguage("en-US"))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setlocale", R"({"value":"en-US"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- Languages ---------- */

TEST_F(UserSettingsTest, AGC_L1_062_GetPreferredAudioLanguages_Success)
{
    EXPECT_CALL(mockUserSettings, GetPreferredAudioLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>("eng,fra"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.preferredaudiolanguages", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("eng"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_063_SetPreferredAudioLanguages_Success)
{
    EXPECT_CALL(mockUserSettings, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":["eng","fra"]})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

TEST_F(UserSettingsTest, AGC_L1_064_GetPreferredCaptionsLanguages_Success)
{
    EXPECT_CALL(mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>("eng"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.preferredlanguages", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("eng"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_065_SetPreferredCaptionsLanguages_Success)
{
    EXPECT_CALL(mockUserSettings, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":"eng"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- Presentation Language ---------- */

TEST_F(UserSettingsTest, AGC_L1_066_GetPresentationLanguage_Success)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>("en-US"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // UserSettingsDelegate splits "en-US" at dash -> "en"
    EXPECT_NE(result.find("en"), std::string::npos);
}

/* ---------- High Contrast ---------- */

TEST_F(UserSettingsTest, AGC_L1_067_GetHighContrast_Success)
{
    EXPECT_CALL(mockUserSettings, GetHighContrast(_))
        .WillOnce(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("false", result);
}

/* ---------- Voice Guidance Hints ---------- */

TEST_F(UserSettingsTest, AGC_L1_068_GetVoiceGuidanceHints_Success)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.navigationhints", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
}

TEST_F(UserSettingsTest, AGC_L1_069_SetVoiceGuidanceHints_Success)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceHints(true))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", R"({"value":true})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- Speed (Firebolt<->Thunder transform) ---------- */

TEST_F(UserSettingsTest, AGC_L1_070_SetSpeed_2_0_TransformsTo_10_0)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(10.0))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":2.0})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_071_SetSpeed_1_67_TransformsTo_1_38)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(1.38))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":1.67})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_072_SetSpeed_1_33_TransformsTo_1_19)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(1.19))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":1.33})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_073_SetSpeed_1_0_TransformsTo_1_0)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(1.0))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":1.0})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_074_SetSpeed_0_5_TransformsTo_0_1)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(0.1))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":0.5})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_075_GetSpeed_HighRate_Returns_2_0)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.56), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("2", result);
}

TEST_F(UserSettingsTest, AGC_L1_076_GetSpeed_MediumRate_Returns_1_0)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("1", result);
}

TEST_F(UserSettingsTest, AGC_L1_077_GetSpeed_LowRate_Returns_0_5)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(0.5), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("0.5", result);
}

/* ---------- Composite settings ---------- */

TEST_F(UserSettingsTest, AGC_L1_078_GetVoiceGuidanceSettings_Success)
{
    // UpdateVoiceGuidanceSettings calls GetVoiceGuidance, GetVoiceGuidanceRate, GetVoiceGuidanceHints
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("enabled"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_079_GetClosedCaptionsSettings_Success)
{
    // GetClosedCaptionSettings calls GetCaptions, GetPreferredCaptionsLanguages
    EXPECT_CALL(mockUserSettings, GetCaptions(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>("eng"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("enabled"), std::string::npos);
}

/* ---------- accessibility.voiceguidance (addSpeed=true) ---------- */

TEST_F(UserSettingsTest, AGC_L1_138_AccessibilityVoiceGuidance_WithSpeed)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidance", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // accessibility.voiceguidance always calls with addSpeed=true
    EXPECT_NE(result.find("\"speed\""), std::string::npos);
    EXPECT_NE(result.find("\"enabled\":true"), std::string::npos);
    EXPECT_NE(result.find("\"rate\""), std::string::npos);
    EXPECT_NE(result.find("\"navigationHints\":false"), std::string::npos);
}

/* ---------- accessibility.audiodescriptionsettings (composite) ---------- */

TEST_F(UserSettingsTest, AGC_L1_139_AudioDescriptionSettings_Success)
{
    EXPECT_CALL(mockUserSettings, GetAudioDescription(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescriptionsettings", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // GetAudioDescription wraps result in {"enabled":...}
    EXPECT_NE(result.find("\"enabled\":true"), std::string::npos);
}

/* ---------- accessibility.closedcaptionssettings (alias) ---------- */

TEST_F(UserSettingsTest, AGC_L1_140_ClosedCaptionsSettingsAlias_Success)
{
    EXPECT_CALL(mockUserSettings, GetCaptions(_))
        .WillOnce(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>("spa,fra"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptionssettings", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("\"enabled\":false"), std::string::npos);
    EXPECT_NE(result.find("preferredLanguages"), std::string::npos);
}

/* ---------- COM-RPC failure paths ---------- */

TEST_F(UserSettingsTest, AGC_L1_141_GetVoiceGuidance_COMFailure)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.enabled", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_142_GetCaptions_COMFailure)
{
    EXPECT_CALL(mockUserSettings, GetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.enabled", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_143_GetHighContrast_COMFailure)
{
    EXPECT_CALL(mockUserSettings, GetHighContrast(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.highcontrastui", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_144_SetVoiceGuidance_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setenabled", R"({"value":true})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_145_SetCaptions_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setenabled", R"({"value":false})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_146_GetSpeed_RateFetchFails)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_147_SetSpeed_RateSetFails)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setspeed", R"({"value":1.5})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_148_VoiceGuidanceSettings_EnabledFetchFails)
{
    // UpdateVoiceGuidanceSettings: if GetVoiceGuidance fails, entire composite fails
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_NE(result.find("voiceguidance"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_149_VoiceGuidanceSettings_RateFetchFails)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_NE(result.find("voiceguidance"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_150_VoiceGuidanceSettings_HintsFetchFails)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_NE(result.find("voiceguidance"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_151_ClosedCaptionsSettings_CaptionsFetchFails)
{
    EXPECT_CALL(mockUserSettings, GetCaptions(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptions", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("captions"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_152_GetAudioDescriptionsEnabled_COMFailure)
{
    EXPECT_CALL(mockUserSettings, GetAudioDescription(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.enabled", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_153_SetAudioDescriptions_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetAudioDescription(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "audiodescriptions.setenabled", R"({"value":true})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_154_GetPresentationLanguage_EmptyResult)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(""), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result);

    // Empty presentation language should produce an error
    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_155_GetLocale_COMFailure)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.locale", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(UserSettingsTest, AGC_L1_156_SetLocale_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetPresentationLanguage(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setlocale", R"({"value":"fr-FR"})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_157_GetPreferredAudioLanguages_COMFailure)
{
    EXPECT_CALL(mockUserSettings, GetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.preferredaudiolanguages", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_EQ("[]", result);
}

TEST_F(UserSettingsTest, AGC_L1_158_SetPreferredAudioLanguages_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetPreferredAudioLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":"eng,fra"})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_159_SetPreferredCaptionsLanguages_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":"eng,spa"})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

TEST_F(UserSettingsTest, AGC_L1_160_SetVoiceGuidanceHints_COMFailure)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setnavigationhints", R"({"value":true})", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ---------- GetSpeed transform: middle ranges ---------- */

TEST_F(UserSettingsTest, AGC_L1_175_GetSpeed_Rate1_38_Returns_1_67)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.38), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("1.67", result);
}

TEST_F(UserSettingsTest, AGC_L1_176_GetSpeed_Rate1_19_Returns_1_33)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.19), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.speed", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("1.33", result);
}

/* ---------- voiceguidance.rate GET alias ---------- */

TEST_F(UserSettingsTest, AGC_L1_177_GetRate_Alias_Success)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.rate", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("1", result);
}

/* ---------- voiceguidance.setrate SET alias ---------- */

TEST_F(UserSettingsTest, AGC_L1_178_SetRate_Alias_Success)
{
    EXPECT_CALL(mockUserSettings, SetVoiceGuidanceRate(10.0))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "voiceguidance.setrate", R"({"value":2.0})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- accessibility.voiceguidancesettings with RDK8 version (addSpeed=false) ---------- */

TEST_F(UserSettingsTest, AGC_L1_179_VoiceGuidanceSettings_RDK8_NoSpeed)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));
    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));

    Exchange::GatewayContext ctx;
    ctx.appId = "test.app";
    ctx.connectionId = 100;
    ctx.requestId = 200;
    ctx.version = "8";

    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidancesettings", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("\"enabled\":true"), std::string::npos);
    EXPECT_NE(result.find("\"rate\""), std::string::npos);
    // RDK8 version → addSpeed=false → no "speed" key
    EXPECT_EQ(result.find("\"speed\""), std::string::npos);
}

/* ---------- accessibility.audiodescription route ---------- */

TEST_F(UserSettingsTest, AGC_L1_180_AccessibilityAudioDescription_Success)
{
    EXPECT_CALL(mockUserSettings, GetAudioDescription(_))
        .WillOnce(DoAll(SetArgReferee<0>(false), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.audiodescription", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("\"enabled\":false"), std::string::npos);
}

/* ========================================================================
 * Category A Coverage: UserSettingsDelegate uncovered branches
 * ======================================================================== */

/* ---------- GetPresentationLanguage: no-dash locale ---------- */

TEST_F(UserSettingsTest, AGC_L1_207_GetPresentationLanguage_NoDashLocale)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("eng")), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // No dash in "eng" → return full string as-is
    EXPECT_EQ("\"eng\"", result);
}

/* ---------- GetPresentationLanguage: empty returns error ---------- */

TEST_F(UserSettingsTest, AGC_L1_208_GetPresentationLanguage_EmptyReturnsError)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("")), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.language", "{}", result);

    // COM call succeeds but empty result → ERROR_GENERAL
    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

/* ---------- GetLocale: empty result → error ---------- */

TEST_F(UserSettingsTest, AGC_L1_209_GetLocale_EmptyReturnsError)
{
    EXPECT_CALL(mockUserSettings, GetPresentationLanguage(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("")), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.locale", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("error"), std::string::npos);
}

/* ---------- SetPreferredAudioLanguages: JSON array input ---------- */

TEST_F(UserSettingsTest, AGC_L1_210_SetPreferredAudioLanguages_JsonArrayInput)
{
    // The delegate should convert JSON array ["eng","fra"] to comma-separated "eng,fra"
    EXPECT_CALL(mockUserSettings, SetPreferredAudioLanguages(string("eng,fra")))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":["eng","fra"]})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- SetPreferredCaptionsLanguages: JSON array input ---------- */

TEST_F(UserSettingsTest, AGC_L1_211_SetPreferredCaptionsLanguages_JsonArrayInput)
{
    EXPECT_CALL(mockUserSettings, SetPreferredCaptionsLanguages(string("eng,spa")))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "closedcaptions.setpreferredlanguages", R"({"value":["eng","spa"]})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- GetClosedCaptionSettings: preferred language failure ---------- */

TEST_F(UserSettingsTest, AGC_L1_212_GetClosedCaptionSettings_PreferredLanguageFailure)
{
    EXPECT_CALL(mockUserSettings, GetCaptions(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    EXPECT_CALL(mockUserSettings, GetPreferredCaptionsLanguages(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.closedcaptions", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("couldn't get preferred captions languages"), std::string::npos);
}

/* ---------- GetPreferredAudioLanguages: comma parsing ---------- */

TEST_F(UserSettingsTest, AGC_L1_213_GetPreferredAudioLanguages_CommaSeparatedParsing)
{
    EXPECT_CALL(mockUserSettings, GetPreferredAudioLanguages(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("eng,fra,spa")), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.preferredaudiolanguages", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Should be a JSON array with three elements
    EXPECT_NE(result.find("eng"), std::string::npos);
    EXPECT_NE(result.find("fra"), std::string::npos);
    EXPECT_NE(result.find("spa"), std::string::npos);
}

/* ---------- SetPreferredAudioLanguages: empty array input ---------- */

TEST_F(UserSettingsTest, AGC_L1_214_SetPreferredAudioLanguages_EmptyArrayInput)
{
    EXPECT_CALL(mockUserSettings, SetPreferredAudioLanguages(string("")))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "localization.setpreferredaudiolanguages", R"({"value":"[]"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- VoiceGuidanceSettings: rate fetch failure ---------- */

TEST_F(UserSettingsTest, AGC_L1_215_VoiceGuidanceSettings_RateFetchFailure)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidance", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("couldn't get voiceguidance rate"), std::string::npos);
}

/* ---------- VoiceGuidanceSettings: hints fetch failure ---------- */

TEST_F(UserSettingsTest, AGC_L1_216_VoiceGuidanceSettings_HintsFetchFailure)
{
    EXPECT_CALL(mockUserSettings, GetVoiceGuidance(_))
        .WillOnce(DoAll(SetArgReferee<0>(true), Return(Core::ERROR_NONE)));

    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceRate(_))
        .WillOnce(DoAll(SetArgReferee<0>(1.0), Return(Core::ERROR_NONE)));

    EXPECT_CALL(mockUserSettings, GetVoiceGuidanceHints(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "accessibility.voiceguidance", "{}", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("couldn't get voiceguidance hints"), std::string::npos);
}

} // namespace
