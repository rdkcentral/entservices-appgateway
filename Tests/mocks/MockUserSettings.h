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

#pragma once

#include <gmock/gmock.h>
#include <interfaces/IUserSettings.h>

class MockUserSettings : public WPEFramework::Exchange::IUserSettings {
public:
    ~MockUserSettings() override = default;

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, Register, (INotification * notification), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unregister, (INotification * notification), (override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetAudioDescription, (const bool enabled), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetAudioDescription, (bool & enabled), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPreferredAudioLanguages, (const string& preferredLanguages), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPreferredAudioLanguages, (string & preferredLanguages), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPresentationLanguage, (const string& presentationLanguage), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPresentationLanguage, (string & presentationLanguage), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetCaptions, (const bool enabled), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetCaptions, (bool & enabled), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPreferredCaptionsLanguages, (const string& preferredLanguages), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPreferredCaptionsLanguages, (string & preferredLanguages), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPreferredClosedCaptionService, (const string& service), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPreferredClosedCaptionService, (string & service), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPrivacyMode, (const string& privacyMode), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPrivacyMode, (string & privacyMode), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPinControl, (const bool pinControl), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPinControl, (bool & pinControl), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetViewingRestrictions, (const string& viewingRestrictions), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetViewingRestrictions, (string & viewingRestrictions), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetViewingRestrictionsWindow, (const string& viewingRestrictionsWindow), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetViewingRestrictionsWindow, (string & viewingRestrictionsWindow), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetLiveWatershed, (const bool liveWatershed), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetLiveWatershed, (bool & liveWatershed), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPlaybackWatershed, (const bool playbackWatershed), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPlaybackWatershed, (bool & playbackWatershed), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetBlockNotRatedContent, (const bool blockNotRatedContent), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetBlockNotRatedContent, (bool & blockNotRatedContent), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetPinOnPurchase, (const bool pinOnPurchase), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPinOnPurchase, (bool & pinOnPurchase), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetHighContrast, (const bool enabled), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetHighContrast, (bool & enabled), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetVoiceGuidance, (const bool enabled), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetVoiceGuidance, (bool & enabled), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetVoiceGuidanceRate, (const double rate), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetVoiceGuidanceRate, (double & rate), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetVoiceGuidanceHints, (const bool hints), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetVoiceGuidanceHints, (bool & hints), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetContentPin, (const string& contentPin), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetContentPin, (string & contentPin), (const, override));

    BEGIN_INTERFACE_MAP(MockUserSettings)
    INTERFACE_ENTRY(WPEFramework::Exchange::IUserSettings)
    END_INTERFACE_MAP
};
