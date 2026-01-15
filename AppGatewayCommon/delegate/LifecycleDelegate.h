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
#include <interfaces/IRDKWindowManager.h>
#include "UtilsLogging.h"
using namespace WPEFramework;

#define LIFECYCLE_MANAGER_CALLSIGN "org.rdk.LifecycleManager"
#define WINDOW_MANAGER_CALLSIGN "org.rdk.WindowManager"

// Valid lifecycle events that can be subscribed to
static const std::set<string> VALID_LIFECYCLE_EVENT = {
    "lifecycle.onbackground",
    "lifecycle.onforeground",
    "lifecycle.oninactive",
    "lifecycle.onsuspended",
    "lifecycle.onunloading",
    "lifecycle2.onstatechanged",
    "discovery.onnavigateto",
    "presentation.onfocusedChanged"
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

    // Dispatch last known intent for a given appId
    void DispatchLastKnownIntent(const string& appId)
    {
        string appInstanceId = mAppIdInstanceIdMap.GetAppInstanceId(appId);
        if (!appInstanceId.empty()) {
            string navigationIntent = mNavigationIntentRegistry.GetNavigationIntent(appInstanceId);
            if (!navigationIntent.empty()) {
                Dispatch("Discovery.onNavigateTo", navigationIntent, appId);
            }
        }
    }

    // Get AppId from Instance Id
    Core::hresult Authenticate(const string& appInstanceId, string& appId)
    {
        appId = mAppIdInstanceIdMap.GetAppId(appInstanceId);
        if (appId.empty()) {
            return Core::ERROR_GENERAL;
        } else {
            return Core::ERROR_NONE;
        }
    }

    // Get AppInstanceId from AppId
    Core::hresult GetSessionId(const string& appId, string& appInstanceId)
    {
        appInstanceId = mAppIdInstanceIdMap.GetAppInstanceId(appId);
        if (appInstanceId.empty()) {
            return Core::ERROR_GENERAL;
        } else {
            return Core::ERROR_NONE;
        }
    }

    Core::hresult LifecycleClose(const Exchange::GatewayContext& context , const string& payload /*@opaque */, string& result /*@out @opaque */){
        result = "null";
        if (mLifecycleManagerState != nullptr) {
            JsonObject params;
            if (params.FromString(payload))
            {
                string reason = params.Get("reason").String();
                // reason == userExit maps to USER_EXIT enum
                if (reason == "userExit") {
                    return mLifecycleManagerState->CloseApp(context.appId, Exchange::ILifecycleManagerState::USER_EXIT);
                } else {
                    return mLifecycleManagerState->CloseApp(context.appId, Exchange::ILifecycleManagerState::ERROR);
                }
            }
        }
        return Core::ERROR_NONE;
    }

    Core::hresult Lifecycle2Close(const Exchange::GatewayContext& context , const string& payload /*@opaque */, string& result /*@out @opaque */){
        result = "null";
        if (mLifecycleManagerState != nullptr) {
            JsonObject params;
            if (params.FromString(payload))
            {
                string reason = params.Get("type").String();
                // reason == userExit maps to USER_EXIT enum
                if (reason == "deactivate") {
                    return mLifecycleManagerState->CloseApp(context.appId, Exchange::ILifecycleManagerState::USER_EXIT);
                } else if (reason == "unload") {
                    return mLifecycleManagerState->CloseApp(context.appId, Exchange::ILifecycleManagerState::ERROR);
                } else if (reason == "killReload") {
                    return mLifecycleManagerState->CloseApp(context.appId, Exchange::ILifecycleManagerState::KILL_AND_RUN);
                } else if (reason == "killReactivate") {
                    return mLifecycleManagerState->CloseApp(context.appId, Exchange::ILifecycleManagerState::KILL_AND_ACTIVATE);
                }
            }
        }
        return Core::ERROR_NONE;
    }

    Core::hresult Lifecycle2State(const Exchange::GatewayContext& context , const string& payload /*@opaque */, string& result /*@out @opaque */){
        // get appInstance Id from context.appId
        string appInstanceId = mAppIdInstanceIdMap.GetAppInstanceId(context.appId);
        // Get current LifecycleState for given appInstanceId
        LifecycleStateInfo stateInfo = mLifecycleStateRegistry.GetLifecycleStateInfo(appInstanceId);

        result = LifecycleStateToString(stateInfo.currentState);
        return Core::ERROR_NONE;
    }

    Core::hresult LifecycleState(const Exchange::GatewayContext& context , const string& payload /*@opaque */, string& result /*@out @opaque */){
         // get appInstance Id from context.appId
        string appInstanceId = mAppIdInstanceIdMap.GetAppInstanceId(context.appId);
        // Get current LifecycleState for given appInstanceId
        LifecycleStateInfo stateInfo = mLifecycleStateRegistry.GetLifecycleStateInfo(appInstanceId);

        result = Lifecycle2StateToLifecycle1String(stateInfo.currentState);
        
        return Core::ERROR_NONE;
    }

    Core::hresult LifecycleReady(const Exchange::GatewayContext& context , const string& payload /*@opaque */, string& result /*@out @opaque */){
        result = "null";
        if (mLifecycleManagerState != nullptr) {
            return mLifecycleManagerState->AppReady(context.appId);            
        }
        return Core::ERROR_NONE;
    }

    Core::hresult LifecycleFinished(const Exchange::GatewayContext& context , const string& payload /*@opaque */, string& result /*@out @opaque */){
        return Core::ERROR_NONE;
    }

    private:
    class LifecycleNotificationHandler : public Exchange::ILifecycleManagerState::INotification
    {
    public:
        LifecycleNotificationHandler(LifecycleDelegate &parent) : mParent(parent) {}
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

        BEGIN_INTERFACE_MAP(NotificationHandler)
        INTERFACE_ENTRY(Exchange::ILifecycleManagerState::INotification)
        END_INTERFACE_MAP

    private:
        LifecycleDelegate &mParent;
    };

    class WindowManagerNotificationHandler : public Exchange::IRDKWindowManager::INotification
    {
    public:
        WindowManagerNotificationHandler(LifecycleDelegate &parent) : mParent(parent) {}

        void OnFocus(const std::string& appInstanceId){
            mParent.mFocusedAppRegistry.SetFocusedAppInstanceId(appInstanceId);
            mParent.Dispatch("Presentation.onFocusedChanged", mParent.mFocusedAppRegistry.GetFocusedEventData(appInstanceId), mParent.mAppIdInstanceIdMap.GetAppId(appInstanceId));
            mParent.HandleAppFocusForLifecycle1(appInstanceId);
        }

        void OnBlur(const std::string& appInstanceId){
            mParent.mFocusedAppRegistry.ClearFocusedAppInstanceId();
            mParent.Dispatch("Presentation.onFocusedChanged", mParent.mFocusedAppRegistry.GetFocusedEventData(appInstanceId), mParent.mAppIdInstanceIdMap.GetAppId(appInstanceId));
            mParent.HandleAppBlurForLifecycle1(appInstanceId);
        }

        // Implement notification methods if needed
        BEGIN_INTERFACE_MAP(WindowManagerNotificationHandler)
        INTERFACE_ENTRY(Exchange::IRDKWindowManager::INotification)
        END_INTERFACE_MAP

    private:
        LifecycleDelegate &mParent;
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

            // is current app lifecycle state is ACTIVE
            bool IsAppLifecycleActive(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (lifecycleStateMap.find(appInstanceId) != lifecycleStateMap.end()) {
                    return lifecycleStateMap[appInstanceId].currentState == Exchange::ILifecycleManager::ACTIVE;
                }
                return false;
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
                    string jsonPayload = "{ \"previous\": \"" + Lifecycle2StateToLifecycle1String(stateInfo.previousState) +
                                         "\", \"state\": \"" + Lifecycle2StateToLifecycle1String(stateInfo.currentState) + "\" }";
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

    // create a class which stores the last app instance id which has focus
    // focus can be cleared by clearing the app instance id when no apps are in focus
    class FocusedAppRegistry {
        public:
            void SetFocusedAppInstanceId(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(focusMutex);
                focusedAppInstanceId = appInstanceId;
            }

            // check if given instanceId has focus
            bool IsAppInstanceIdFocused(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(focusMutex);
                return focusedAppInstanceId == appInstanceId;
            }

            string GetFocusedAppInstanceId() {
                std::lock_guard<std::mutex> lock(focusMutex);
                return focusedAppInstanceId;
            }

            void ClearFocusedAppInstanceId() {
                std::lock_guard<std::mutex> lock(focusMutex);
                focusedAppInstanceId.clear();
            }

            // return a focus json string for a given app instanceId {"value": true or false}
            string GetFocusedEventData(const string& appInstanceId) {
                std::lock_guard<std::mutex> lock(focusMutex);
                bool isFocused = (focusedAppInstanceId == appInstanceId);
                // jsonPayload is boolean for value
                string jsonPayload = "{ \"value\": " + string(isFocused ? "true" : "false") + " }";
                return jsonPayload;
            }
        private:
            string focusedAppInstanceId;
            std::mutex focusMutex;
    };


    // create a utility function to convert LifecycleState to string
    static string LifecycleStateToString(Exchange::ILifecycleManager::LifecycleState state) {
        switch (state) {
            case Exchange::ILifecycleManager::UNLOADED:
                return "unloaded";
            case Exchange::ILifecycleManager::LOADING:
                return "loading";
            case Exchange::ILifecycleManager::INITIALIZING:
                return "initializing";
            case Exchange::ILifecycleManager::PAUSED:
                return "paused";
            case Exchange::ILifecycleManager::ACTIVE:
                return "active";
            case Exchange::ILifecycleManager::SUSPENDED:
                return "suspended";
            case Exchange::ILifecycleManager::HIBERNATED:
                return "hibernated";
            case Exchange::ILifecycleManager::TERMINATING:
                return "terminating";
            default:
                return "";
        }
    }

    static string Lifecycle2StateToLifecycle1String(Exchange::ILifecycleManager::LifecycleState state) {
        switch (state) {
            case Exchange::ILifecycleManager::UNLOADED:
            case Exchange::ILifecycleManager::TERMINATING:
                return "unloading";
            case Exchange::ILifecycleManager::LOADING:
            case Exchange::ILifecycleManager::INITIALIZING:
                return "initializing";
            case Exchange::ILifecycleManager::PAUSED:
                return "inactive";
            case Exchange::ILifecycleManager::ACTIVE:
                return "foreground";
            case Exchange::ILifecycleManager::HIBERNATED:
            case Exchange::ILifecycleManager::SUSPENDED:
                return "suspended";
            default:
                return "";
        }
    }

    // Create method to support lifecycle 1 updates which accepts the instanceId along with current
    // old lifecycle states
    void HandleLifecycle1Update(const string& appInstanceId,  const Exchange::ILifecycleManager::LifecycleState oldLifecycleState, const Exchange::ILifecycleManager::LifecycleState newLifecycleState)
    {
        switch (newLifecycleState) {
            
            case Exchange::ILifecycleManager::PAUSED:
                Dispatch("Lifecycle.onInactive", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
                break;
            case Exchange::ILifecycleManager::SUSPENDED:
            case Exchange::ILifecycleManager::HIBERNATED:
                Dispatch("Lifecycle.onSuspended", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
                break;
            case Exchange::ILifecycleManager::UNLOADED:
            case Exchange::ILifecycleManager::TERMINATING:
                Dispatch("Lifecycle.onUnloading", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
                break;
            case Exchange::ILifecycleManager::ACTIVE:
                // if app is in focus send foreground
                 if(mFocusedAppRegistry.IsAppInstanceIdFocused(appInstanceId)) {
                    Dispatch("Lifecycle.onForeground", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
                 } else {
                    Dispatch("Lifecycle.onBackground", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
                 }
                break;
            default:
                // No action for other states
                break;
        }
    }

    // create a function to handle focus for a given app instance id
    void HandleAppFocusForLifecycle1(const string& appInstanceId) {
        // get if current app lifecycle is active
        if (mLifecycleStateRegistry.IsAppLifecycleActive(appInstanceId)) {
            mFocusedAppRegistry.SetFocusedAppInstanceId(appInstanceId);
            Dispatch("Lifecycle.onForeground", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
        }
    }

    // create a function to handle blur for a given app instance id
    void HandleAppBlurForLifecycle1(const string& appInstanceId) {
        // get if current app lifecycle is active
        if (mLifecycleStateRegistry.IsAppLifecycleActive(appInstanceId)) {
            mFocusedAppRegistry.ClearFocusedAppInstanceId();
            Dispatch("Lifecycle.onBackground", mLifecycleStateRegistry.GetLifecycle1StateJson(appInstanceId), mAppIdInstanceIdMap.GetAppId(appInstanceId));
        }
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

    Exchange::IRDKWindowManager* GetWindowManagerInterface()
    {
        if (mWindowManager == nullptr && mShell != nullptr)
        {
            mWindowManager = mShell->QueryInterfaceByCallsign<Exchange::IRDKWindowManager>(WINDOW_MANAGER_CALLSIGN);
            if (mWindowManager == nullptr)
            {
                LOGERR("Failed to get RDKWindowManager COM interface");
            }
        }
        return mWindowManager;
    }

    // Handle Lifecycle update for a given appInstanceId by accepting the previous and current lifecycle state
    void HandleLifeycleUpdate(const string& appInstanceId,  const Exchange::ILifecycleManager::LifecycleState oldLifecycleState, const Exchange::ILifecycleManager::LifecycleState newLifecycleState)
    {
        // update lifecycle state registry
        mLifecycleStateRegistry.UpdateLifecycleState(appInstanceId, newLifecycleState);

        // get appId from appInstanceId
        string appId = mAppIdInstanceIdMap.GetAppId(appInstanceId);

        Dispatch("Lifecycle2.onStateChanged", mLifecycleStateRegistry.GetLifecycle2StateJson(appInstanceId), appId);

        // if new lifecycleState is ACTIVE trigger last known intent
        if (newLifecycleState == Exchange::ILifecycleManager::ACTIVE) {
            DispatchLastKnownIntent(appId);
        }

        HandleLifecycle1Update(appInstanceId, oldLifecycleState, newLifecycleState);
    }

    
    private:
        PluginHost::IShell *mShell;
        Exchange::ILifecycleManagerState *mLifecycleManagerState;
        Exchange::IRDKWindowManager *mWindowManager;
        Core::Sink<LifecycleNotificationHandler> mNotificationHandler;
        // add all registries
        AppIdInstanceIdMap mAppIdInstanceIdMap;
        LifecycleStateRegistry mLifecycleStateRegistry;
        NavigationIntentRegistry mNavigationIntentRegistry;
        FocusedAppRegistry mFocusedAppRegistry;
};


#endif // __LIFECYCLEDELEGATE_H__
