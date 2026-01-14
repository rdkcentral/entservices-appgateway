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
    "lifecycle.onbackground",
    "lifecycle.onforeground",
    "lifecycle.oninactive",
    "lifecycle.onsuspended",
    "lifecycle.onunloading",
    "lifecycle2.onstatechanged"
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

    // Handle Lifecycle update for a given appInstanceId by accepting the previous and current lifecycle state
    void HandleLifeycleUpdate(const string& appInstanceId,  const Exchange::ILifecycleManager::LifecycleState oldLifecycleState, const Exchange::ILifecycleManager::LifecycleState newLifecycleState)
    {
        // update lifecycle state registry
        mLifecycleStateRegistry.UpdateLifecycleState(appInstanceId, newLifecycleState);

        // get appId from appInstanceId
        string appId = mAppIdInstanceIdMap.GetAppId(appInstanceId);

        Dispatch("Lifecycle2.onStateChanged", mLifecycleStateRegistry.GetLifecycle2StateJson(appInstanceId), appId);

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

            // add navigation intent to registry
            mParent.mNavigationIntentRegistry.AddNavigationIntent(appInstanceId, navigationIntent);

            // if new Lifecycle state is INITIALIZING then add to app instance map
            if (newLifecycleState == Exchange::ILifecycleManager::INITIALIZING) {
                mParent.mAppIdInstanceIdMap.AddAppInstanceId(appId, appInstanceId);
                // also add to lifecycle state registry
                mParent.mLifecycleStateRegistry.AddLifecycleState(appInstanceId, oldLifecycleState, newLifecycleState);   
            } 

            // handle lifecycle update
            mParent.HandleLifeycleUpdate(appInstanceId, oldLifecycleState, newLifecycleState);            
            
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

    // create a class to store app Id and app instance id map
    class AppIdInstanceIdMap {
        public:
            void AddAppInstanceId(const string& appId, const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(mapMutex);
                appIdInstanceIdMap[appId] = appInstanceId;
            }

            string GetAppInstanceId(const string& appId) {
                std::lock_guard<std::mutex> lock(mapMutex);
                if (appIdInstanceIdMap.find(appId) != appIdInstanceIdMap.end()) {
                    return appIdInstanceIdMap[appId];
                }
                return "";
            }

            // reverse lookup app instance id to app id
            string GetAppId(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(mapMutex);
                for (const auto& pair : appIdInstanceIdMap) {
                    if (pair.second == appInstanceId) {
                        return pair.first;
                    }
                }
                return "";
            }

            void RemoveAppInstanceId(const string& appId) {
                std::lock_guard<std::mutex> lock(mapMutex);
                appIdInstanceIdMap.erase(appId);
            }
        private:
            std::map<string, string> appIdInstanceIdMap;
            std::mutex mapMutex;
    };

    // struct to contain previous and current const Exchange::ILifecycleManager::LifecycleState
    struct LifecycleStateInfo {
        Exchange::ILifecycleManager::LifecycleState previousState;
        Exchange::ILifecycleManager::LifecycleState currentState;
    };

    // Create a class to act as Registry to contain map of appInstanceId to LifecycleStateInfo
    class LifecycleStateRegistry {
        public:
            // new method to accept previous state and current state for a given appInstanceId
            void AddLifecycleState(const string& appInstanceId, Exchange::ILifecycleManager::LifecycleState previousState, Exchange::ILifecycleManager::LifecycleState currentState) {
                std::lock_guard<std::mutex> lock(registryMutex);
                lifecycleStateMap[appInstanceId] = {previousState, currentState};
            }

            void UpdateLifecycleState(const string& appInstanceId, Exchange::ILifecycleManager::LifecycleState newState) {
                std::lock_guard<std::mutex> lock(registryMutex);
                LifecycleStateInfo& stateInfo = lifecycleStateMap[appInstanceId];
                stateInfo.previousState = stateInfo.currentState;
                stateInfo.currentState = newState;
            }

            LifecycleStateInfo GetLifecycleStateInfo(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (lifecycleStateMap.find(appInstanceId) != lifecycleStateMap.end()) {
                    return lifecycleStateMap[appInstanceId];
                }
                return {Exchange::ILifecycleManager::UNLOADED, Exchange::ILifecycleManager::UNLOADED};
            }

            void RemoveLifecycleStateInfo(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(registryMutex);
                lifecycleStateMap.erase(appInstanceId);
            }

            // Get json payload of current and previous state for a given appInstanceId
            string GetLifecycle1StateJson(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (lifecycleStateMap.find(appInstanceId) != lifecycleStateMap.end()) {
                    LifecycleStateInfo& stateInfo = lifecycleStateMap[appInstanceId];
                    string jsonPayload = "{ \"previous\": \"" + LifecycleStateToString(stateInfo.previousState) +
                                         "\", \"state\": \"" + LifecycleStateToString(stateInfo.currentState) + "\" }";
                    return jsonPayload;
                }
                return "{}";
            }

            string GetLifecycle2StateJson(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (lifecycleStateMap.find(appInstanceId) != lifecycleStateMap.end()) {
                    LifecycleStateInfo& stateInfo = lifecycleStateMap[appInstanceId];
                    string jsonPayload = "{ \"oldState\": \"" + LifecycleStateToString(stateInfo.previousState) +
                                         "\", \"newState\": \"" + LifecycleStateToString(stateInfo.currentState) + "\" }";
                    return jsonPayload;
                }
                return "{}";
            }
            
        private:
            std::map<string, LifecycleStateInfo> lifecycleStateMap;
            std::mutex registryMutex;
    };

    // create a class as registry to store appInstance Id and the intent string
    class NavigationIntentRegistry {
        public:
            void AddNavigationIntent(const string& appInstanceId, const string& navigationIntent) {
                std::lock_guard<std::mutex> lock(intentMutex);
                navigationIntentMap[appInstanceId] = navigationIntent;
            }

            string GetNavigationIntent(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(intentMutex);
                if (navigationIntentMap.find(appInstanceId) != navigationIntentMap.end()) {
                    return navigationIntentMap[appInstanceId];
                }
                return "";
            }

            void RemoveNavigationIntent(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(intentMutex);
                navigationIntentMap.erase(appInstanceId);
            }
        private:
            std::map<string, string> navigationIntentMap;
            std::mutex intentMutex;
    };


    // create a utility function to convert LifecycleState to string
    static string LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState state) {
        switch (state) {
            case Exchange::ILifecycleManager::UNLOADED:
                return "UNLOADED";
            case Exchange::ILifecycleManager::LOADING:
                return "LOADING";
            case Exchange::ILifecycleManager::INITIALIZING:
                return "INITIALIZING";
            case Exchange::ILifecycleManager::PAUSED:
                return "PAUSED";
            case Exchange::ILifecycleManager::ACTIVE:
                return "ACTIVE";
            case Exchange::ILifecycleManager::SUSPENDED:
                return "SUSPENDED";
            case Exchange::ILifecycleManager::HIBERNATED:
                return "HIBERNATED";
            case Exchange::ILifecycleManager::TERMINATING:
                return "TERMINATING";
            default:
                return "UNKNOWN";
        }
    }

    
    private:
        PluginHost::IShell *mShell;
        Exchange::ILifecycleManagerState *mLifecycleManagerState;
        Core::Sink<LifecycleNotificationHandler> mNotificationHandler;
        // add all registries
        AppIdInstanceIdMap mAppIdInstanceIdMap;
        LifecycleStateRegistry mLifecycleStateRegistry;
        NavigationIntentRegistry mNavigationIntentRegistry;
};


#endif // __LIFECYCLEDELEGATE_H__
