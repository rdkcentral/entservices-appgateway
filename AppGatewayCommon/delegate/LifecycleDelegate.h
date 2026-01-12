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
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#pragma once

#ifndef __LIFECYCLEDELEGATE_H__
#define __LIFECYCLEDELEGATE_H__

#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/ILifecycleManagerState.h>
#include "UtilsLogging.h"
using namespace WPEFramework;

#define LIFECYCLE_MANAGER_CALLSIGN "org.rdk.LifecycleManager"

// Valid lifecycle events that can be subscribed to
static const std::set<string> VALID_LIFECYCLE_EVENT = {
    "device.onlifecyclechanged"
};

class LifecycleDelegate : public BaseEventDelegate
{
    public:
    LifecycleDelegate(PluginHost::IShell *shell) : BaseEventDelegate(), mShell(shell),mLifecycleManagerState(nullptr), mNotificationHandler(*this)
    {
        #ifdef USE_APP_MANAGERS
           Exchange::ILifecycleManagerState *lifecycleManagerState = GetLifecycleManagerStateInterface();
           if (lifecycleManagerState == nullptr)
           {
               LOGERR("LifecycleManagerState interface not available");
           }
           mLifecycleManagerState->Register(&mNotificationHandler);
           mNotificationHandler.SetRegistered(true);
        #endif
    }

    ~LifecycleDelegate()
    {
        if (mLifecycleManagerState != nullptr)
        {
            mLifecycleManagerState->Release();
            mLifecycleManagerState = nullptr;
        }
        
    }

    bool HandleSubscription(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen)
    {
        // Check if event is present in VALID_LIFECYCLE_EVENT make check case insensitive
        if (listen)
        {
           Exchange::ILifecycleManagerState *lifecycleManagerState = GetLifecycleManagerStateInterface();
           if (lifecycleManagerState == nullptr)
           {
               LOGERR("LifecycleManagerState interface not available");
               return false;
           }
            AddNotification(event, cb);
        }
        else
        {
            RemoveNotification(event, cb);
        }
        return false;
    }

    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen, bool &registrationError)
    {
        LOGDBG("Checking for handle event");
        // Check if event is present in VALID_LIFECYCLE_EVENT make check case insensitive
        if (VALID_LIFECYCLE_EVENT.find(StringUtils::toLower(event)) != VALID_LIFECYCLE_EVENT.end())
        {
            // Handle LifecycleManagerState event
            registrationError = HandleSubscription(cb, event, listen);
            return true;
        }
        return false;
    }

    Exchange::ILifecycleManagerState* GetLifecycleManagerStateInterface()
    {
        if (mLifecycleManagerState == nullptr && mShell != nullptr)
        {
            mLifecycleManagerState = mShell->QueryInterfaceByCallsign<Exchange::ILifecycleManagerState>(LIFECYCLE_MANAGER_CALLSIGN);
            if (mLifecycleManagerState == nullptr)
            {
                LOGERR("Failed to get LifecycleManagerState COM interface");
            }
        }
        return mLifecycleManagerState;
    }

    private:
    class LifecycleNotificationHandler : public Exchange::ILifecycleManagerState::INotification
    {
    public:
        LifecycleNotificationHandler(LifecycleDelegate &parent) : mParent(parent), registered(false) {}
        void OnAppLifecycleStateChanged(const string& appId /* @text appId */,
                                        const string& appInstanceId /* @text appInstanceId */,
                                        const Exchange::ILifecycleManager::LifecycleState oldLifecycleState /* @text oldLifecycleState */,
                                        const Exchange::ILifecycleManager::LifecycleState newLifecycleState /* @text newLifecycleState */,
                                        const string& navigationIntent /* @text navigationIntent */) override
        {
            LOGINFO("OnAppLifecycleStateChanged: appId=%s, appInstanceId=%s, oldState=%d, newState=%d, navigationIntent=%s",
                    appId.c_str(), appInstanceId.c_str(), oldLifecycleState, newLifecycleState, navigationIntent.c_str());
        }

        // Registration management methods
        void SetRegistered(bool state)
        {
            std::lock_guard<std::mutex> lock(registerMutex);
            registered = state;
        }

        bool GetRegistered()
        {
            std::lock_guard<std::mutex> lock(registerMutex);
            return registered;
        }

        BEGIN_INTERFACE_MAP(NotificationHandler)
        INTERFACE_ENTRY(Exchange::ILifecycleManagerState::INotification)
        END_INTERFACE_MAP

    private:
        LifecycleDelegate &mParent;
        bool registered;
        std::mutex registerMutex;
    };

    
    private:
        PluginHost::IShell *mShell;
        Exchange::ILifecycleManagerState *mLifecycleManagerState;
        Core::Sink<LifecycleNotificationHandler> mNotificationHandler;
};


#endif // __LIFECYCLEDELEGATE_H__
