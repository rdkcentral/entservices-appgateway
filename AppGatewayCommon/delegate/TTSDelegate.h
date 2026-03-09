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
#include <mutex>
#include "ObjectUtils.h"


#define TTS_CALLSIGN "org.rdk.TextToSpeech"
#define APP_API_METHOD_PREFIX "TextToSpeech."

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
        // Check if event starts with "TextToSpeech" make check case insensitive
        if (StringUtils::checkStartsWithCaseInsensitive(event, APP_API_METHOD_PREFIX))
        {
            // Handle TextToSpeech event
            registrationError = !HandleSubscription(cb, event, listen);
            return true;
        }
        registrationError = true; // event not recognized - signal error to caller
        return false;
    }

private:

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
            
        void OnVoiceChanged(const string voice)
        {
            mParent.Dispatch("TextToSpeech.onVoiceChanged", ObjectUtils::CreateStringObject(voice));
        }
        void OnSpeechReady(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onWillSpeak", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnSpeechStarted(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechStart", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnSpeechPaused(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechPause", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnSpeechResumed(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechResume", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnSpeechInterrupted(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechInterrupted", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnNetworkError(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onNetworkError", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnPlaybackError(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onPlaybackError", ObjectUtils::CreateUInt32Object(speechid));
        }
        void OnSpeechComplete(const uint32_t speechid)
        {
            mParent.Dispatch("TextToSpeech.onSpeechComplete", ObjectUtils::CreateUInt32Object(speechid));
        }

        void OnTTSStateChanged(const bool state) {
            mParent.Dispatch("TextToSpeech.onTtsstatechanged", ObjectUtils::CreateBooleanJsonString("value", state));
        }

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
