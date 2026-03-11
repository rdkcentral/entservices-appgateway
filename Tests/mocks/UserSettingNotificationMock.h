/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#pragma once

#include <gmock/gmock.h>
#include <interfaces/IUserSettings.h>

using ::WPEFramework::Exchange::IUserSettings;

class UserSettingNotificationMock : public IUserSettings::INotification {
public:
    UserSettingNotificationMock() = default;
    virtual ~UserSettingNotificationMock() = default;

    MOCK_METHOD(void, OnAudioDescriptionChanged, (const bool enabled), (override));
    MOCK_METHOD(void, OnPreferredAudioLanguagesChanged, (const string& preferredLanguages), (override));
    MOCK_METHOD(void, OnPresentationLanguageChanged, (const string& presentationLanguage), (override));
    MOCK_METHOD(void, OnCaptionsChanged, (const bool enabled), (override));
    MOCK_METHOD(void, OnPreferredCaptionsLanguagesChanged, (const string& preferredLanguages), (override));
    MOCK_METHOD(void, OnPreferredClosedCaptionServiceChanged, (const string& service), (override));
    MOCK_METHOD(void, OnPrivacyModeChanged, (const string& privacyMode), (override));
    MOCK_METHOD(void, OnPinControlChanged, (const bool pinControl), (override));
    MOCK_METHOD(void, OnViewingRestrictionsChanged, (const string& viewingRestrictions), (override));
    MOCK_METHOD(void, OnViewingRestrictionsWindowChanged, (const string& viewingRestrictionsWindow), (override));
    MOCK_METHOD(void, OnLiveWatershedChanged, (const bool liveWatershed), (override));
    MOCK_METHOD(void, OnPlaybackWatershedChanged, (const bool playbackWatershed), (override));
    MOCK_METHOD(void, OnBlockNotRatedContentChanged, (const bool blockNotRatedContent), (override));
    MOCK_METHOD(void, OnPinOnPurchaseChanged, (const bool pinOnPurchase), (override));
    MOCK_METHOD(void, OnHighContrastChanged, (const bool enabled), (override));
    MOCK_METHOD(void, OnVoiceGuidanceChanged, (const bool enabled), (override));
    MOCK_METHOD(void, OnVoiceGuidanceRateChanged, (const double rate), (override));
    MOCK_METHOD(void, OnVoiceGuidanceHintsChanged, (const bool hints), (override));
    MOCK_METHOD(void, OnContentPinChanged, (const string& contentPin), (override));
};
