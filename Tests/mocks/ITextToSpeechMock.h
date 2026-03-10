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
#include <interfaces/ITextToSpeech.h>

namespace WPEFramework {
namespace Exchange {

class MockITextToSpeech : public ITextToSpeech {
public:
    MockITextToSpeech() = default;
    virtual ~MockITextToSpeech() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    // Registration methods
    MOCK_METHOD(Core::hresult, Register, (ITextToSpeech::INotification* notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (ITextToSpeech::INotification* notification), (override));
    MOCK_METHOD(Core::hresult, RegisterWithCallsign, (const string callsign, ITextToSpeech::INotification* sink), (override));

    // Enable (setter and getter - overloaded)
    MOCK_METHOD(Core::hresult, Enable, (const bool enable), (override));
    MOCK_METHOD(Core::hresult, Enable, (bool& enable), (const, override));

    // Configuration
    MOCK_METHOD(Core::hresult, SetConfiguration, (const Configuration& config, TTSErrorDetail& status), (override));
    MOCK_METHOD(Core::hresult, GetConfiguration, (Configuration& config), (const, override));

    // Other setters
    MOCK_METHOD(Core::hresult, SetFallbackText, (const string scenario, const string value), (override));
    MOCK_METHOD(Core::hresult, SetAPIKey, (const string apikey), (override));
    MOCK_METHOD(Core::hresult, SetPrimaryVolDuck, (const uint8_t prim), (override));
    MOCK_METHOD(Core::hresult, SetACL, (const string method, const string apps), (override));

    // Voices
    MOCK_METHOD(Core::hresult, ListVoices, (const string language, RPC::IStringIterator*& voices), (const, override));

    // Speech operations
    MOCK_METHOD(Core::hresult, Speak, (const string callsign, const string text, uint32_t& speechid, TTSErrorDetail& status), (override));
    MOCK_METHOD(Core::hresult, Cancel, (const uint32_t speechid), (override));
    MOCK_METHOD(Core::hresult, Pause, (const uint32_t speechid, TTSErrorDetail& status), (override));
    MOCK_METHOD(Core::hresult, Resume, (const uint32_t speechid, TTSErrorDetail& status), (override));
    MOCK_METHOD(Core::hresult, GetSpeechState, (const uint32_t speechid, SpeechState& state), (override));

    BEGIN_INTERFACE_MAP(MockITextToSpeech)
    INTERFACE_ENTRY(ITextToSpeech)
    END_INTERFACE_MAP
};

} // namespace Exchange
} // namespace WPEFramework
