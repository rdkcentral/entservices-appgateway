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

#include "Module.h"
#include "WsManager.h"
#include <interfaces/IAppGateway.h>
#include <interfaces/IConfiguration.h>
#include "ContextUtils.h"
#include <com/com.h>
#include <core/core.h>
#include <map>
#include <mutex>
#include "UtilsAppGatewayTelemetry.h"
#include <unordered_set>
#include <sstream>
#include <unordered_map>


namespace WPEFramework {
namespace Plugin {
    using Context = Exchange::GatewayContext;
    class AppGatewayResponderImplementation : public Exchange::IConfiguration, public Exchange::IAppGatewayResponder, public Exchange::IAppGatewayAppSessionGuard
    {

    public:
        AppGatewayResponderImplementation();
        ~AppGatewayResponderImplementation() override;

        // We do not allow this plugin to be copied !!
        AppGatewayResponderImplementation(const AppGatewayResponderImplementation&) = delete;
        AppGatewayResponderImplementation& operator=(const AppGatewayResponderImplementation&) = delete;

        BEGIN_INTERFACE_MAP(AppGatewayResponderImplementation)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        INTERFACE_ENTRY(Exchange::IAppGatewayResponder)
        INTERFACE_ENTRY(Exchange::IAppGatewayAppSessionGuard)
        END_INTERFACE_MAP

    public:
        Core::hresult Respond(const Context& context, const string& payload) override;
         Core::hresult Emit(const Context& context /* @in */, 
                const string& method /* @in */, const string& payload /* @in @opaque */) override;
        Core::hresult Request(const uint32_t connectionId /* @in */, 
                const uint32_t id /* @in */, const string& method /* @in */, const string& params /* @in @opaque */) override;
        Core::hresult GetGatewayConnectionContext(const uint32_t connectionId /* @in */,
                const string& contextKey /* @in */, 
                 string& contextValue /* @out */) override;
        Core::hresult RecordGatewayConnectionContext(const uint32_t connectionId ,
                const string& contextKey ,
                const string& contextValue) override;
        
        virtual Core::hresult Register(Exchange::IAppGatewayResponder::INotification *notification) override;
        virtual Core::hresult Unregister(Exchange::IAppGatewayResponder::INotification *notification) override;

        virtual void OnConnectionStatusChanged(const string& appId, const uint32_t connectionId, const bool connected);

        // IAppGatewayAppSessionGuard interface
        Core::hresult SuspendTraffic(const string& appId) override;
        Core::hresult ResumeTraffic(const string& appId) override;

        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* service) override;

    private:
class AppIdRegistry{
        public:
            void Add(const uint32_t connectionId, const std::string& appId) {
                std::lock_guard<std::mutex> lock(mAppIdMutex);
                mAppIdMap[connectionId] = appId;
            }

            void Remove(const uint32_t connectionId) {
                std::lock_guard<std::mutex> lock(mAppIdMutex);
                mAppIdMap.erase(connectionId);
            }

            bool Get(const uint32_t connectionId, string& appId) {
                std::lock_guard<std::mutex> lock(mAppIdMutex);
                auto it = mAppIdMap.find(connectionId);
                if (it != mAppIdMap.end()) {
                    appId = it->second;
                    return true;
                }
                return false;
            }


        private:
            std::unordered_map<uint32_t,string> mAppIdMap;
            std::mutex mAppIdMutex;
        };

        // Create new Registry for Debug Disabled Connections
        class DebugDisabledConnectionsRegistry {
        public:
            void Add(const uint32_t connectionId) {
                std::lock_guard<std::mutex> lock(mDebugDisabledMutex);
                mDebugDisabledConnections[connectionId] = true;
            }

            void Remove(const uint32_t connectionId) {
                std::lock_guard<std::mutex> lock(mDebugDisabledMutex);
                mDebugDisabledConnections.erase(connectionId);
            }

            bool IsDebugDisabled(const uint32_t connectionId) {
                std::lock_guard<std::mutex> lock(mDebugDisabledMutex);
                return mDebugDisabledConnections.find(connectionId) != mDebugDisabledConnections.end();
            }

            private:
            std::unordered_map<uint32_t,bool> mDebugDisabledConnections;
            std::mutex mDebugDisabledMutex;
        };

        // Create new Registry for JSON RPC compliant connections
        class CompliantJsonRpcRegistry {
        public:
        // Token substring that indicates a connection is compliant with JSON RPC (RPC version 2).
        // When this substring is present in the authentication token, the connection is treated
        // as JSON RPC compliant and added to the compliant connections registry.
        static constexpr const char* kCompliantJsonRpcFeatureFlag = "RPCv2=true";

        void CheckAndAddCompliantJsonRpc(const uint32_t connectionId, const string& token) {
            // Split all query parameters based on delimiter '&'
            std::stringstream ss(token);
            std::string param;
            while (std::getline(ss, param, '&')) {
                // If any parameter exactly matches the feature flag, add to compliant list
                if (param == kCompliantJsonRpcFeatureFlag) {
                    std::lock_guard<std::mutex> lock(mCompliantJsonRpcMutex);
                    mCompliantJsonRpcConnections.insert(connectionId);
                    break;
                }
            }
        }

        bool IsCompliantJsonRpc(const uint32_t connectionId) {
            std::lock_guard<std::mutex> lock(mCompliantJsonRpcMutex);
            return (mCompliantJsonRpcConnections.find(connectionId) != mCompliantJsonRpcConnections.end());
        }

        void CleanupConnectionId(const uint32_t connectionId) {
            std::lock_guard<std::mutex> lock(mCompliantJsonRpcMutex);
            auto it = mCompliantJsonRpcConnections.find(connectionId);
            if (it != mCompliantJsonRpcConnections.end()) {
                mCompliantJsonRpcConnections.erase(it);
            }
        }

        private:
            // unordered set of connection IDs which are compliant with JSON RPC
            std::unordered_set<uint32_t> mCompliantJsonRpcConnections;
            std::mutex mCompliantJsonRpcMutex;
        };

        void DispatchWsMsg(const std::string& method,
            const std::string& params,
            const uint32_t requestId,
            const uint32_t connectionId);

        void ReturnMessageInSocket(const uint32_t connectionId, const int requestId, const string payload);

        bool DispatchResponseToConnectionIfNotPaused(const uint32_t connectionId, const uint32_t requestId, const string& result);
        bool DispatchNotificationToConnectionIfNotPaused(const uint32_t connectionId, const string& designator, const string& payload);
        bool SendRequestToConnectionIfNotPaused(const uint32_t connectionId, const string& designator, const uint32_t requestId, const string& params);

        // Thread-safe registry tracking appIds whose WebSocket traffic is currently paused
        // (i.e., the application is in the HIBERNATED lifecycle state).
        class PausedAppsRegistry {
        public:
            void Pause(const string& appId) {
                std::lock_guard<std::mutex> lock(mMutex);
                mPausedApps.insert(appId);
            }

            void Resume(const string& appId) {
                std::lock_guard<std::mutex> lock(mMutex);
                mPausedApps.erase(appId);
            }

            bool IsPaused(const string& appId) const {
                std::lock_guard<std::mutex> lock(mMutex);
                return mPausedApps.find(appId) != mPausedApps.end();
            }

        private:
            mutable std::mutex mMutex;
            std::unordered_set<string> mPausedApps;
        };

        PluginHost::IShell* mService;
        WebSocketConnectionManager mWsManager;
        mutable Core::CriticalSection mAuthenticatorLock;
        mutable Core::CriticalSection mResolverLock;
        Exchange::IAppGatewayAuthenticator *mAuthenticator; // Shared pointer to Authenticator
        Exchange::IAppGatewayResolver *mResolver; // Shared pointer to InternalGatewayResolver
        AppIdRegistry mAppIdRegistry;
        uint32_t InitializeWebsocket();
        mutable Core::CriticalSection mConnectionStatusImplLock;
        std::list<Exchange::IAppGatewayResponder::INotification*> mConnectionStatusNotification;
        bool mEnhancedLoggingEnabled;
        CompliantJsonRpcRegistry mCompliantJsonRpcRegistry;
        DebugDisabledConnectionsRegistry mDebugDisabledConnectionsRegistry;
        PausedAppsRegistry mPausedAppsRegistry;
    };
} // namespace Plugin
} // namespace WPEFramework
