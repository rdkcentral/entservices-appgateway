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
#include <interfaces/ITextToSpeech.h>

using ::WPEFramework::Exchange::ITextToSpeech;

class TextToSpeechMock : public ITextToSpeech {
public:
    TextToSpeechMock() = default;
    virtual ~TextToSpeechMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, Register, (ITextToSpeech::INotification* sink), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unregister, (ITextToSpeech::INotification* sink), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, RegisterWithCallsign, (const string callsign, ITextToSpeech::INotification* sink), (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Enable, (const bool enable), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Enable, (bool& enable), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetConfiguration, (const ITextToSpeech::Configuration& config, ITextToSpeech::TTSErrorDetail& status), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFallbackText, (const string scenario, const string value), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetAPIKey, (const string apikey), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetPrimaryVolDuck, (const uint8_t prim), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetACL, (const string method, const string apps), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetConfiguration, (ITextToSpeech::Configuration& config), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, ListVoices, (const string language, RPC::IStringIterator*& voices), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, Speak, (const string callsign, const string text, uint32_t& speechid, ITextToSpeech::TTSErrorDetail& status), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Cancel, (const uint32_t speechid), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Pause, (const uint32_t speechid, ITextToSpeech::TTSErrorDetail& status), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Resume, (const uint32_t speechid, ITextToSpeech::TTSErrorDetail& status), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetSpeechState, (const uint32_t speechid, ITextToSpeech::SpeechState& state), (override));

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

class TextToSpeechNotificationMock : public ITextToSpeech::INotification {
public:
    TextToSpeechNotificationMock() = default;
    virtual ~TextToSpeechNotificationMock() = default;

    MOCK_METHOD(void, OnTTSStateChanged, (const bool state), (override));
    MOCK_METHOD(void, OnVoiceChanged, (const string voice), (override));
    MOCK_METHOD(void, OnSpeechReady, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnSpeechStarted, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnSpeechPaused, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnSpeechResumed, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnSpeechInterrupted, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnNetworkError, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnPlaybackError, (const uint32_t speechid), (override));
    MOCK_METHOD(void, OnSpeechComplete, (const uint32_t speechid), (override));
};
