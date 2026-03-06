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

namespace WPEFramework {
namespace Exchange {

class MockIUserSettings : public IUserSettings {
public:
    MockIUserSettings() = default;
    virtual ~MockIUserSettings() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    // Registration methods
    MOCK_METHOD(Core::hresult, Register, (IUserSettings::INotification* notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (IUserSettings::INotification* notification), (override));

    // Audio Description
    MOCK_METHOD(Core::hresult, SetAudioDescription, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetAudioDescription, (bool& enabled), (const, override));

    // Preferred Audio Languages
    MOCK_METHOD(Core::hresult, SetPreferredAudioLanguages, (const string& preferredLanguages), (override));
    MOCK_METHOD(Core::hresult, GetPreferredAudioLanguages, (string& preferredLanguages), (const, override));

    // Presentation Language
    MOCK_METHOD(Core::hresult, SetPresentationLanguage, (const string& presentationLanguage), (override));
    MOCK_METHOD(Core::hresult, GetPresentationLanguage, (string& presentationLanguage), (const, override));

    // Captions
    MOCK_METHOD(Core::hresult, SetCaptions, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetCaptions, (bool& enabled), (const, override));

    // Preferred Captions Languages
    MOCK_METHOD(Core::hresult, SetPreferredCaptionsLanguages, (const string& preferredLanguages), (override));
    MOCK_METHOD(Core::hresult, GetPreferredCaptionsLanguages, (string& preferredLanguages), (const, override));

    // Preferred Closed Caption Service
    MOCK_METHOD(Core::hresult, SetPreferredClosedCaptionService, (const string& service), (override));
    MOCK_METHOD(Core::hresult, GetPreferredClosedCaptionService, (string& service), (const, override));

    // Privacy Mode
    MOCK_METHOD(Core::hresult, SetPrivacyMode, (const string& privacyMode), (override));
    MOCK_METHOD(Core::hresult, GetPrivacyMode, (string& privacyMode), (const, override));

    // Pin Control
    MOCK_METHOD(Core::hresult, SetPinControl, (const bool pinControl), (override));
    MOCK_METHOD(Core::hresult, GetPinControl, (bool& pinControl), (const, override));

    // Viewing Restrictions
    MOCK_METHOD(Core::hresult, SetViewingRestrictions, (const string& viewingRestrictions), (override));
    MOCK_METHOD(Core::hresult, GetViewingRestrictions, (string& viewingRestrictions), (const, override));

    // Viewing Restrictions Window
    MOCK_METHOD(Core::hresult, SetViewingRestrictionsWindow, (const string& viewingRestrictionsWindow), (override));
    MOCK_METHOD(Core::hresult, GetViewingRestrictionsWindow, (string& viewingRestrictionsWindow), (const, override));

    // Live Watershed
    MOCK_METHOD(Core::hresult, SetLiveWatershed, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetLiveWatershed, (bool& enabled), (const, override));

    // Playback Watershed
    MOCK_METHOD(Core::hresult, SetPlaybackWatershed, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetPlaybackWatershed, (bool& enabled), (const, override));

    // Block Not Rated Content
    MOCK_METHOD(Core::hresult, SetBlockNotRatedContent, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetBlockNotRatedContent, (bool& enabled), (const, override));

    // Pin On Purchase
    MOCK_METHOD(Core::hresult, SetPinOnPurchase, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetPinOnPurchase, (bool& enabled), (const, override));

    // High Contrast
    MOCK_METHOD(Core::hresult, SetHighContrast, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetHighContrast, (bool& enabled), (const, override));

    // Voice Guidance
    MOCK_METHOD(Core::hresult, SetVoiceGuidance, (const bool enabled), (override));
    MOCK_METHOD(Core::hresult, GetVoiceGuidance, (bool& enabled), (const, override));

    // Voice Guidance Rate
    MOCK_METHOD(Core::hresult, SetVoiceGuidanceRate, (const double rate), (override));
    MOCK_METHOD(Core::hresult, GetVoiceGuidanceRate, (double& rate), (const, override));

    // Voice Guidance Hints
    MOCK_METHOD(Core::hresult, SetVoiceGuidanceHints, (const bool hints), (override));
    MOCK_METHOD(Core::hresult, GetVoiceGuidanceHints, (bool& hints), (const, override));

    // Content Pin
    MOCK_METHOD(Core::hresult, SetContentPin, (const string& contentPin), (override));
    MOCK_METHOD(Core::hresult, GetContentPin, (string& contentPin), (const, override));

    BEGIN_INTERFACE_MAP(MockIUserSettings)
    INTERFACE_ENTRY(IUserSettings)
    END_INTERFACE_MAP
};

} // namespace Exchange
} // namespace WPEFramework
