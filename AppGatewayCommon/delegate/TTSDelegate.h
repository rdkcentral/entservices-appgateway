/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 **/
#pragma once
#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/ITextToSpeech.h>
#include "UtilsLogging.h"
#include "ContextUtils.h"
#include "UtilsFirebolt.h"
// UtilsFirebolt.h defines plain ERROR_NOT_SUPPORTED/ERROR_NOT_AVAILABLE/ERROR_NOT_PERMITTED
// macros that collide with Core::ERROR_NOT_SUPPORTED etc. used below; undef locally to this file.
#undef ERROR_NOT_SUPPORTED
#undef ERROR_NOT_AVAILABLE
#undef ERROR_NOT_PERMITTED
#include <mutex>
#include <limits>
#include "ObjectUtils.h"


#define TTS_CALLSIGN "org.rdk.TextToSpeech"
#define APP_API_METHOD_PREFIX "TextToSpeech."
#define APP_SPEECH_SYNTHESIS_METHOD_PREFIX "SpeechSynthesis."

using namespace WPEFramework;

class TTSDelegate : public BaseEventDelegate
{
public:
    TTSDelegate(PluginHost::IShell *shell) : BaseEventDelegate(), mTextToSpeech(nullptr), mShell(shell), mNotificationHandler(*this) {}

    ~TTSDelegate()
    {
        {
            std::lock_guard<std::mutex> lock(mTTSMutex);
            if (nullptr != mTextToSpeech)
            {
                if (GetRegistered())
                {
                    mTextToSpeech->Unregister(&mNotificationHandler);
                    SetRegistered(false);
                }
                mTextToSpeech->Release();
                mTextToSpeech = nullptr;
            }
        }
        
    }

    bool HandleSubscription(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen)
    {
        if (listen)
        {
            if (Register())
            {
                AddNotification(event, cb);
                return true;
            }
            return false;
        }
        else
        {
            // Not removing the notification subscription for cases where one event is removed
            RemoveNotification(event, cb);
        }
        return true;
    }

    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen, bool &registrationError)
    {
        // Check if event starts with TextToSpeech or SpeechSynthesis, case insensitive.
        if (StringUtils::checkStartsWithCaseInsensitive(event, APP_API_METHOD_PREFIX) ||
            StringUtils::checkStartsWithCaseInsensitive(event, APP_SPEECH_SYNTHESIS_METHOD_PREFIX))
        {
            registrationError = !HandleSubscription(cb, event, listen);
            return true;
        }
        registrationError = true; // event not recognized - signal error to caller
        return false;
    }

    // ---- SpeechSynthesis.* request handling ----
    // Called by AppGatewayCommon; reuses the same cached ITextToSpeech interface as notifications.
    Core::hresult SpeechSynthesisVoices(const std::string& payload, std::string& result)
    {
        result = "[]";

        auto* tts = GetTTS();
        if (nullptr == tts) {
            return Core::ERROR_NONE;
        }

#if defined(ITEXTTOSPEECH_VERSION) && (ITEXTTOSPEECH_VERSION >= 2)
        std::string languageFilter;
        if (!payload.empty() && (payload != "null")) {
            Core::JSON::VariantContainer params;
            if (!params.FromString(payload)) {
                return BadSpeechSynthesisRequest("SpeechSynthesis.voices requires a valid JSON object payload", result);
            }

            if (!(TryGetStringField(params, "lang", languageFilter) ||
                  TryGetStringField(params, "language", languageFilter) ||
                  TryGetStringField(params, "value", languageFilter))) {
                languageFilter.clear();
            }
        }

        Exchange::ITextToSpeech::IVoiceInfoIterator* voices = nullptr;
        const auto status = tts->GetVoices(languageFilter, voices);

        if (Core::ERROR_NONE != status) {
            return MapTtsTransportError(status, "voices", result);
        }

        if (nullptr == voices) {
            return Core::ERROR_NONE;
        }

        std::string serializedVoices = "[";
        bool firstVoice = true;
        while (voices->Next()) {
            const auto voice = voices->Current();
            if (!firstVoice) {
                serializedVoices += ",";
            }
            serializedVoices += SerializeVoiceInfo(voice);
            firstVoice = false;
        }
        voices->Release();

        serializedVoices += "]";
        result = serializedVoices;
        return Core::ERROR_NONE;
#else
        ErrorUtils::NotSupported(result);
        return Core::ERROR_NOT_SUPPORTED;
#endif
    }

    Core::hresult SpeechSynthesisSpeak(const std::string& appId, const std::string& payload, std::string& result)
    {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.speak requires a valid JSON object payload", result);
        }

        std::string text;
        if (!TryGetStringField(params, "text", text) || text.empty()) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.speak requires a non-empty 'text' field", result);
        }

        auto* tts = GetTTS();
        if (nullptr == tts) {
            ErrorUtils::NotAvailable(result);
            return Core::ERROR_UNAVAILABLE;
        }

        const std::string callsign = appId.empty() ? "AppGateway" : appId;
        Exchange::ITextToSpeech::TTSErrorDetail detailStatus = Exchange::ITextToSpeech::TTS_OK;
        uint32_t utteranceId = 0;

#if defined(ITEXTTOSPEECH_VERSION) && (ITEXTTOSPEECH_VERSION >= 2)
        Exchange::ITextToSpeech::SpeechUtterance utterance{};
        utterance.language = "";
        utterance.voice = "";
        utterance.volume = -1.0;
        utterance.rate = -1.0;
        utterance.pitch = -1.0;

        bool piiProvided = false;
        ApplyUtteranceOverrides(params, utterance, piiProvided);

        Core::JSON::VariantContainer options;
        if (TryGetObjectField(params, "options", options)) {
            ApplyUtteranceOverrides(options, utterance, piiProvided);
        }

        if ((utterance.volume >= 0.0) && !IsInRange(utterance.volume, 0.0, 1.0)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.speak 'volume' must be between 0.0 and 1.0", result);
        }
        if ((utterance.rate >= 0.0) && !IsInRange(utterance.rate, 0.1, 10.0)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.speak 'rate' must be between 0.1 and 10.0", result);
        }
        if ((utterance.pitch >= 0.0) && !IsInRange(utterance.pitch, 0.0, 2.0)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.speak 'pitch' must be between 0.0 and 2.0", result);
        }
        if (piiProvided) {
            LOGINFO("SpeechSynthesis.speak received 'pii' but ITextToSpeech v2 has no matching field; ignoring it");
        }

        const auto status = tts->SpeakWithUtterance(callsign, utterance, text, utteranceId, detailStatus);
#else
        const auto status = tts->Speak(callsign, text, utteranceId, detailStatus);
#endif

        if (Core::ERROR_NONE != status) {
            return MapTtsTransportError(status, "speak", result);
        }

        const auto detailResult = MapTtsDetailError(detailStatus, "speak", result);
        if (Core::ERROR_NONE != detailResult) {
            return detailResult;
        }

        result = std::to_string(utteranceId);
        return Core::ERROR_NONE;
    }

    Core::hresult SpeechSynthesisCancel(const std::string& payload, std::string& result)
    {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.cancel requires a valid JSON object payload", result);
        }

        uint32_t utteranceId = 0;
        if (!(TryGetUInt32Field(params, "utteranceId", utteranceId) ||
              TryGetUInt32Field(params, "speechId", utteranceId) ||
              TryGetUInt32Field(params, "value", utteranceId))) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.cancel requires a numeric 'utteranceId' field", result);
        }

        auto* tts = GetTTS();
        if (nullptr == tts) {
            ErrorUtils::NotAvailable(result);
            return Core::ERROR_UNAVAILABLE;
        }

        const auto status = tts->Cancel(utteranceId);
        if (Core::ERROR_NONE != status) {
            return MapTtsTransportError(status, "cancel", result);
        }

        result = "null";
        return Core::ERROR_NONE;
    }

    Core::hresult SpeechSynthesisPause(const std::string& payload, std::string& result)
    {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.pause requires a valid JSON object payload", result);
        }

        uint32_t utteranceId = 0;
        if (!(TryGetUInt32Field(params, "utteranceId", utteranceId) ||
              TryGetUInt32Field(params, "speechId", utteranceId) ||
              TryGetUInt32Field(params, "value", utteranceId))) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.pause requires a numeric 'utteranceId' field", result);
        }

        auto* tts = GetTTS();
        if (nullptr == tts) {
            ErrorUtils::NotAvailable(result);
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::ITextToSpeech::TTSErrorDetail detailStatus = Exchange::ITextToSpeech::TTS_OK;
        const auto status = tts->Pause(utteranceId, detailStatus);

        if (Core::ERROR_NONE != status) {
            return MapTtsTransportError(status, "pause", result);
        }

        const auto detailResult = MapTtsDetailError(detailStatus, "pause", result);
        if (Core::ERROR_NONE != detailResult) {
            return detailResult;
        }

        result = "null";
        return Core::ERROR_NONE;
    }

    Core::hresult SpeechSynthesisResume(const std::string& payload, std::string& result)
    {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.resume requires a valid JSON object payload", result);
        }

        uint32_t utteranceId = 0;
        if (!(TryGetUInt32Field(params, "utteranceId", utteranceId) ||
              TryGetUInt32Field(params, "speechId", utteranceId) ||
              TryGetUInt32Field(params, "value", utteranceId))) {
            return BadSpeechSynthesisRequest("SpeechSynthesis.resume requires a numeric 'utteranceId' field", result);
        }

        auto* tts = GetTTS();
        if (nullptr == tts) {
            ErrorUtils::NotAvailable(result);
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::ITextToSpeech::TTSErrorDetail detailStatus = Exchange::ITextToSpeech::TTS_OK;
        const auto status = tts->Resume(utteranceId, detailStatus);

        if (Core::ERROR_NONE != status) {
            return MapTtsTransportError(status, "resume", result);
        }

        const auto detailResult = MapTtsDetailError(detailStatus, "resume", result);
        if (Core::ERROR_NONE != detailResult) {
            return detailResult;
        }

        result = "null";
        return Core::ERROR_NONE;
    }

private:

    // ---- SpeechSynthesis helpers ----
    static bool TryGetStringField(const Core::JSON::VariantContainer& container, const char* fieldName, std::string& value)
    {
        if (!container.HasLabel(fieldName)) {
            return false;
        }
        const auto& field = container.Get(fieldName);
        if (field.Content() != Core::JSON::Variant::type::STRING) {
            return false;
        }
        value = field.String();
        return true;
    }

    static bool TryGetDoubleField(const Core::JSON::VariantContainer& container, const char* fieldName, double& value)
    {
        if (!container.HasLabel(fieldName)) {
            return false;
        }
        const auto& field = container.Get(fieldName);
        if (field.Content() != Core::JSON::Variant::type::NUMBER) {
            return false;
        }
        value = field.Number();
        return true;
    }

    static bool TryGetUInt32Field(const Core::JSON::VariantContainer& container, const char* fieldName, uint32_t& value)
    {
        if (!container.HasLabel(fieldName)) {
            return false;
        }
        const auto& field = container.Get(fieldName);
        if (field.Content() == Core::JSON::Variant::type::NUMBER) {
            const double numericValue = field.Number();
            if ((numericValue < 0.0) || (numericValue > static_cast<double>(std::numeric_limits<uint32_t>::max()))) {
                return false;
            }
            value = static_cast<uint32_t>(numericValue);
            return true;
        }
        if (field.Content() == Core::JSON::Variant::type::STRING) {
            try {
                size_t processed = 0;
                const auto parsed = std::stoull(field.String(), &processed, 10);
                if ((processed != field.String().size()) || (parsed > std::numeric_limits<uint32_t>::max())) {
                    return false;
                }
                value = static_cast<uint32_t>(parsed);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    static bool TryGetObjectField(const Core::JSON::VariantContainer& container, const char* fieldName, Core::JSON::VariantContainer& value)
    {
        if (!container.HasLabel(fieldName)) {
            return false;
        }
        const auto& field = container.Get(fieldName);
        if (field.Content() != Core::JSON::Variant::type::OBJECT) {
            return false;
        }
        value = field.Object();
        return true;
    }

    static bool IsInRange(const double value, const double minimum, const double maximum)
    {
        return (value >= minimum) && (value <= maximum);
    }

    static Core::hresult BadSpeechSynthesisRequest(const std::string& message, std::string& result)
    {
        ErrorUtils::CustomBadRequest(message, result);
        return Core::ERROR_BAD_REQUEST;
    }

    static Core::hresult MapTtsTransportError(const Core::hresult status, const std::string& action, std::string& result)
    {
        switch (status) {
        case Core::ERROR_NONE:
            return Core::ERROR_NONE;
        case Core::ERROR_NOT_SUPPORTED:
            ErrorUtils::NotSupported(result);
            return Core::ERROR_NOT_SUPPORTED;
        case Core::ERROR_UNAVAILABLE:
            ErrorUtils::NotAvailable(result);
            return Core::ERROR_UNAVAILABLE;
        default:
            ErrorUtils::CustomInternal("SpeechSynthesis " + action + " failed", result);
            return Core::ERROR_GENERAL;
        }
    }

    static Core::hresult MapTtsDetailError(const Exchange::ITextToSpeech::TTSErrorDetail status, const std::string& action, std::string& result)
    {
        switch (status) {
        case Exchange::ITextToSpeech::TTS_OK:
            return Core::ERROR_NONE;
        case Exchange::ITextToSpeech::TTS_NOT_ENABLED:
            ErrorUtils::NotAvailable(result);
            return Core::ERROR_UNAVAILABLE;
        case Exchange::ITextToSpeech::TTS_NO_ACCESS:
            ErrorUtils::NotPermitted(result);
            return Core::ERROR_GENERAL;
        case Exchange::ITextToSpeech::TTS_INVALID_CONFIGURATION:
            ErrorUtils::CustomBadRequest("Invalid speech synthesis configuration", result);
            return Core::ERROR_BAD_REQUEST;
        case Exchange::ITextToSpeech::TTS_FAIL:
        default:
            ErrorUtils::CustomInternal("SpeechSynthesis " + action + " failed", result);
            return Core::ERROR_GENERAL;
        }
    }

#if defined(ITEXTTOSPEECH_VERSION) && (ITEXTTOSPEECH_VERSION >= 2)
    static void ApplyUtteranceOverrides(const Core::JSON::VariantContainer& container,
                                        Exchange::ITextToSpeech::SpeechUtterance& utterance,
                                        bool& piiProvided)
    {
        std::string stringValue;
        double numericValue = 0.0;

        if (TryGetStringField(container, "lang", stringValue) || TryGetStringField(container, "language", stringValue)) {
            utterance.language = stringValue;
        }
        if (TryGetStringField(container, "voice", stringValue)) {
            utterance.voice = stringValue;
        }
        if (TryGetDoubleField(container, "volume", numericValue)) {
            utterance.volume = numericValue;
        }
        if (TryGetDoubleField(container, "rate", numericValue)) {
            utterance.rate = numericValue;
        }
        if (TryGetDoubleField(container, "pitch", numericValue)) {
            utterance.pitch = numericValue;
        }
        if (container.HasLabel("pii")) {
            piiProvided = true;
        }
    }

    static std::string SerializeVoiceInfo(const Exchange::ITextToSpeech::VoiceInfo& voice)
    {
        JsonObject voiceObject;
        voiceObject["name"] = voice.name;
        voiceObject["lang"] = voice.language;
        voiceObject["default"] = voice.isDefault;

        std::string serialized;
        voiceObject.ToString(serialized);
        return serialized;
    }
#endif

    Exchange::ITextToSpeech* GetTTS() {
        std::lock_guard<std::mutex> lock(mTTSMutex);
        if ((nullptr == mTextToSpeech) && (nullptr != mShell)) {
            mTextToSpeech = mShell->QueryInterfaceByCallsign<Exchange::ITextToSpeech>(TTS_CALLSIGN);
            if (nullptr == mTextToSpeech) {
                LOGERR("Failed to get TextToSpeech COM interface");
            }
        }
        return mTextToSpeech;
    }

    class TTSNotificationHandler : public Exchange::ITextToSpeech::INotification
    {
    public:
        TTSNotificationHandler(TTSDelegate &parent) : mParent(parent) {}
        ~TTSNotificationHandler() {}

        void DispatchUtteranceEvent(const uint32_t speechid, const string& eventName)
        {
            JsonObject payload;
            payload["utteranceId"] = speechid;
            payload["event"] = eventName;

            string payloadString;
            payload.ToString(payloadString);
            mParent.Dispatch("SpeechSynthesis.onUtteranceEvent", payloadString);
        }
            
        void OnVoiceChanged(const string voice)
        {
            mParent.Dispatch("TextToSpeech.onVoiceChanged", ObjectUtils::CreateStringObject(voice));
        }
        void OnSpeechReady(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onWillSpeak", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "synthesisStarting");
        }
        void OnSpeechStarted(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechStart", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "playbackStarting");
        }
        void OnSpeechPaused(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechPause", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "paused");
        }
        void OnSpeechResumed(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechResume", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "resumed");
        }
        void OnSpeechInterrupted(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechInterrupted", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "interrupted");
        }
        void OnNetworkError(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onNetworkError", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "networkFailed");
        }
        void OnPlaybackError(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onPlaybackError", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "playbackFailed");
        }
        void OnSpeechComplete(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechComplete", ObjectUtils::CreateUInt32Object(speechid));
            DispatchUtteranceEvent(speechid, "completed");
        }

        void OnTTSStateChanged(const bool state) {
            mParent.Dispatch("TextToSpeech.onTtsstatechanged", ObjectUtils::CreateBooleanJsonString("value", state));
        }

#if defined(ITEXTTOSPEECH_VERSION) && (ITEXTTOSPEECH_VERSION >= 2)
        void OnVoicesChanged() override
        {
            mParent.Dispatch("SpeechSynthesis.onVoicesChanged", "null");
        }
#endif

        BEGIN_INTERFACE_MAP(TTSNotificationHandler)
        INTERFACE_ENTRY(Exchange::ITextToSpeech::INotification)
        END_INTERFACE_MAP
    private:
        TTSDelegate &mParent;
        
    };
    Exchange::ITextToSpeech *mTextToSpeech;
    PluginHost::IShell *mShell;
    Core::Sink<TTSNotificationHandler> mNotificationHandler;
    mutable std::mutex mTTSMutex;
    bool registered = false;
    mutable std::mutex registerMutex;

    // New Method for Set registered
    void SetRegistered(bool state)
    {
        std::lock_guard<std::mutex> lock(registerMutex);
        registered = state;
    }

    // New Method for get registered
    bool GetRegistered()
    {
        std::lock_guard<std::mutex> lock(registerMutex);
        return registered;
    }

    bool Register()
    {
        auto tts = GetTTS();    
        if (nullptr == tts)
        {
            LOGERR("TextToSpeech interface not available");
            return false;
        }
        std::lock_guard<std::mutex> lock(registerMutex);
        if (!registered)
        {
            if (nullptr != tts) {
                LOGINFO("Registering for TextToSpeech notifications");
                tts->Register(&mNotificationHandler);
                registered = true;
            } else {
                LOGERR("Failed to register for TextToSpeech notifications because interface is null");
                return false;
            }
        }
        return true;
    }
};
