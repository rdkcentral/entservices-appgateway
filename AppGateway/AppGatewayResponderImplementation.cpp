/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
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
 **/

#include <string>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <plugins/JSONRPC.h>
#include <plugins/IShell.h>
#include "AppGatewayResponderImplementation.h"
#include "UtilsLogging.h"
#include "UtilsConnections.h"
#include "UtilsCallsign.h"
#include <interfaces/IAppNotifications.h>

// App Gateway is only available via local connections,
// so we can use a simple in-memory registry to track connection IDs and their associated app IDs.
#define APPGATEWAY_SOCKET_ADDRESS "127.0.0.1:3473"
#define DEFAULT_CONFIG_PATH "/etc/app-gateway/resolution.base.json"
#define COMMON_GATEWAY_AUTHENTICATOR_CALLSIGN "org.rdk.AppGatewayCommon"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(AppGatewayResponderImplementation, 1, 0, 0);

        AppGatewayResponderImplementation::AppGatewayResponderImplementation()
            : mService(nullptr),
            mWsManager(),
            mAuthenticator(nullptr),
            mResolver(nullptr),
            mConnectionStatusImplLock(),
            mEnhancedLoggingEnabled(false)
        {
            LOGINFO("AppGatewayResponderImplementation constructor");
#ifdef ENABLE_APP_GATEWAY_AUTOMATION
        #ifdef APP_GATEWAY_ENHANCED_LOGGING_INDICATOR
            struct stat buffer;
            mEnhancedLoggingEnabled = (stat(APP_GATEWAY_ENHANCED_LOGGING_INDICATOR, &buffer) == 0);
            LOGINFO("Enhanced logging enabled: %s (indicator: %s)", mEnhancedLoggingEnabled ? "true" : "false", APP_GATEWAY_ENHANCED_LOGGING_INDICATOR);
        #endif
#endif
        }

        AppGatewayResponderImplementation::~AppGatewayResponderImplementation()
        {
            LOGINFO("AppGatewayResponderImplementation destructor");
            
            // Clean up WebSocket handlers first to prevent race conditions during shutdown
            CleanupWebsocket();
            
            // Clear weak self reference to prevent any remaining jobs from accessing this object
            ClearWeakSelf();
            
            if (nullptr != mService)
            {
                mService->Release();
                mService = nullptr;
            }

            if (nullptr != mResolver)
            {
                mResolver->Release();
                mResolver = nullptr;
            }

            if (nullptr != mAuthenticator)
            {
                mAuthenticator->Release();
                mAuthenticator = nullptr;
            }

        }

        uint32_t AppGatewayResponderImplementation::Configure(PluginHost::IShell *shell)
        {
            LOGINFO("Configuring AppGatewayResponderImplementation");
            uint32_t result = Core::ERROR_NONE;
            ASSERT(shell != nullptr);
            mService = shell;
            mService->AddRef();
            
            // Create weak self reference for safe job handling
            CreateWeakSelf();
            
            result = InitializeWebsocket();

            return result;
        }

        uint32_t AppGatewayResponderImplementation::InitializeWebsocket(){
            // Initialize WebSocket server
            WebSocketConnectionManager::Config config(APPGATEWAY_SOCKET_ADDRESS);
            std::string configLine = mService->ConfigLine();
            Core::OptionalType<Core::JSON::Error> error;
            if (config.FromString(configLine, error) == false)
            {
                LOGERR("Failed to parse config line, error: '%s', config line: '%s'.",
                       (error.IsSet() ? error.Value().Message().c_str() : "Unknown"),
                       configLine.c_str());
            }

            LOGINFO("Connector: %s", config.Connector.Value().c_str());
            Core::NodeId source(config.Connector.Value().c_str());
            LOGINFO("Parsed port: %d", source.PortNumber());
            
            AppGatewayResponderImplementation::WeakPtr weakSelf = GetWeakSelf();
            mWsManager.SetMessageHandler(
                [weakSelf](const std::string &method, const std::string &params, const int requestId, const uint32_t connectionId)
                {
                    if (auto sharedSelf = weakSelf.lock()) {
                        Core::IWorkerPool::Instance().Submit(WsMsgJob::Create(weakSelf, method, params, requestId, connectionId));
                    }
                });

            mWsManager.SetAuthHandler(
                [weakSelf](const uint32_t connectionId, const std::string &token) -> bool
                {
                    auto sharedSelf = weakSelf.lock();
                    if (!sharedSelf) {
                        // Object destroyed during shutdown - this is expected
                        return false;
                    }
                    
                    string sessionId = Utils::ResolveQuery(token, "session");
                    if (sessionId.empty())
                    {
                        LOGERR("No session token provided");
                        return false;
                    }

                    if (nullptr == sharedSelf->mAuthenticator) {
                        if (ConfigUtils::useAppManagers()) {
                            sharedSelf->mAuthenticator = sharedSelf->mService->QueryInterfaceByCallsign<Exchange::IAppGatewayAuthenticator>(COMMON_GATEWAY_AUTHENTICATOR_CALLSIGN);
                        } else {
                            sharedSelf->mAuthenticator = sharedSelf->mService->QueryInterfaceByCallsign<Exchange::IAppGatewayAuthenticator>(GATEWAY_AUTHENTICATOR_CALLSIGN);
                        }
                        if (nullptr == sharedSelf->mAuthenticator) {
                            LOGERR("Authenticator Not available");
                            return false;
                        }
                    }

                    string appId;
                    if (Core::ERROR_NONE == sharedSelf->mAuthenticator->Authenticate(sessionId,appId)) {
                        LOGTRACE("APP ID %s", appId.c_str());
                        sharedSelf->mAppIdRegistry.Add(connectionId, appId);
                        sharedSelf->mCompliantJsonRpcRegistry.CheckAndAddCompliantJsonRpc(connectionId, token);
                        #ifdef ENABLE_APP_GATEWAY_AUTOMATION
                        // Check if this is the automation client
                        #ifdef AUTOMATION_APP_ID
                        if (appId == AUTOMATION_APP_ID) {
                            sharedSelf->mWsManager.SetAutomationId(connectionId);
                            LOGINFO("Automation server connected with ID: %d, appId: %s", connectionId, appId.c_str());
                        }
                        #endif
                        #endif
                        
                        Core::IWorkerPool::Instance().Submit(ConnectionStatusNotificationJob::Create(weakSelf, connectionId, appId, true));

                        return true;
                    }

                    return false;
                });

            mWsManager.SetDisconnectHandler(
                [weakSelf](const uint32_t connectionId)
                {
                    auto sharedSelf = weakSelf.lock();
                    if (!sharedSelf) {
                        // Object destroyed during shutdown - this is expected
                        return;
                    }
                    
                    LOGINFO("Connection disconnected: %d", connectionId);
                    string appId;
                    if (!sharedSelf->mAppIdRegistry.Get(connectionId, appId)) {
                        LOGERR("No App ID found for connection %d during disconnect", connectionId);
                    } else {
                        LOGINFO("App ID %s found for connection %d during disconnect", appId.c_str(), connectionId);
                        Core::IWorkerPool::Instance().Submit(ConnectionStatusNotificationJob::Create(weakSelf, connectionId, appId, false));
                    }
                    
                    sharedSelf->mAppIdRegistry.Remove(connectionId);
                    sharedSelf->mCompliantJsonRpcRegistry.CleanupConnectionId(connectionId);
                    Exchange::IAppNotifications* appNotifications = sharedSelf->mService->QueryInterfaceByCallsign<Exchange::IAppNotifications>(APP_NOTIFICATIONS_CALLSIGN);
                    if (appNotifications != nullptr) {
                        if (Core::ERROR_NONE != appNotifications->Cleanup(connectionId, APP_GATEWAY_CALLSIGN)) {
                            LOGERR("AppNotifications Cleanup failed for connectionId: %d", connectionId);
                        }
                        appNotifications->Release();
                        appNotifications = nullptr;
                    }
                }
            );
            mWsManager.Start(source);
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayResponderImplementation::Respond(const Context& context, const string& payload)
        {
            Core::IWorkerPool::Instance().Submit(RespondJob::Create(GetWeakSelf(), context.connectionId, context.requestId, payload));
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayResponderImplementation::Emit(const Context& context /* @in */, 
                const string& method /* @in */, const string& payload /* @in @opaque */) {
            // check if the connection is compliant with JSON RPC
            if (mCompliantJsonRpcRegistry.IsCompliantJsonRpc(context.connectionId)) {
                Core::IWorkerPool::Instance().Submit(EmitJob::Create(GetWeakSelf(), context.connectionId, method, payload));
            }
            else {
                Core::IWorkerPool::Instance().Submit(RespondJob::Create(GetWeakSelf(), context.connectionId, context.requestId, payload));
            }
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayResponderImplementation::Request(const uint32_t connectionId /* @in */, 
                const uint32_t id /* @in */, const string& method /* @in */, const string& params /* @in @opaque */) {
            Core::IWorkerPool::Instance().Submit(RequestJob::Create(GetWeakSelf(), connectionId, id, method, params));
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayResponderImplementation::GetGatewayConnectionContext(const uint32_t connectionId /* @in */,
                const string& contextKey /* @in */, 
                 string& contextValue /* @out */) {
            // TODO add support for jsonrpc compliance in later versions
            return Core::ERROR_NONE;
        }


        void AppGatewayResponderImplementation::DispatchWsMsg(const std::string &method,
                                                     const std::string &params,
                                                     const uint32_t requestId,
                                                     const uint32_t connectionId)
        {
            string appId;

            if (mAppIdRegistry.Get(connectionId, appId)) {

                if (mEnhancedLoggingEnabled) {
                    LOGDBG("%s-->[[a-%d-%d]] method=%s, params=%s",
                           appId.c_str(),connectionId, requestId, method.c_str(), params.c_str());
                }
                // App Id is available
                Context context = {
                    requestId,
                    connectionId,
                    appId
                };

                if (mResolver == nullptr) {
                    mResolver = mService->QueryInterface<Exchange::IAppGatewayResolver>();
                }

                if (mResolver == nullptr) {
                    LOGERR("Resolver interface not available");
                    return;
                }

                string resolution;
                if (Core::ERROR_NONE != mResolver->Resolve(context, APP_GATEWAY_CALLSIGN, method, params, resolution)) {
                    LOGERR("Resolver Failure");
                }
            } else {
                LOGERR("No App ID found for connection %d. Terminate connection", connectionId);
                mWsManager.Close(connectionId);
            }
        }


        Core::hresult AppGatewayResponderImplementation::Register(Exchange::IAppGatewayResponder::INotification *notification)
        {
            ASSERT (nullptr != notification);

            Core::SafeSyncType<Core::CriticalSection> lock(mConnectionStatusImplLock);

            /* Make sure we can't register the same notification callback multiple times */
            if (std::find(mConnectionStatusNotification.begin(), mConnectionStatusNotification.end(), notification) == mConnectionStatusNotification.end())
            {
                LOGINFO("Register notification");
                mConnectionStatusNotification.push_back(notification);
                notification->AddRef();
            }

            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayResponderImplementation::Unregister(Exchange::IAppGatewayResponder::INotification *notification )
        {
            Core::hresult status = Core::ERROR_GENERAL;

            ASSERT (nullptr != notification);

            Core::SafeSyncType<Core::CriticalSection> lock(mConnectionStatusImplLock);

            /* Make sure we can't unregister the same notification callback multiple times */
            auto itr = std::find(mConnectionStatusNotification.begin(), mConnectionStatusNotification.end(), notification);
            if (itr != mConnectionStatusNotification.end())
            {
                (*itr)->Release();
                LOGINFO("Unregister notification");
                mConnectionStatusNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }

            return status;
        }

        void AppGatewayResponderImplementation::OnConnectionStatusChanged(const string& appId, const uint32_t connectionId, const bool connected)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mConnectionStatusImplLock);
            for (auto& notification : mConnectionStatusNotification)
            {
                notification->OnAppConnectionChanged(appId, connectionId, connected);
            }

            #ifdef ENABLE_APP_GATEWAY_AUTOMATION
            // Notify automation server of connection status change
            mWsManager.UpdateConnection(connectionId, appId, connected);
            #endif
        }

        void AppGatewayResponderImplementation::CleanupWebsocket()
        {
            LOGINFO("Cleaning up WebSocket to prevent race conditions during shutdown");

            // First, replace handlers with thread-safe no-op implementations
            // This ensures that any pending callbacks won't access the object being destroyed
            mWsManager.SetMessageHandler([](const std::string&, const std::string&, const int, const uint32_t) {
                // No-op handler - safe during shutdown
            });

            mWsManager.SetAuthHandler([](const uint32_t, const std::string&) -> bool {
                // No-op handler - reject all authentication attempts during shutdown
                return false;
            });
            
            mWsManager.SetDisconnectHandler([](const uint32_t) {
                // No-op handler - safe during shutdown  
            });

            // Give a brief moment for any in-flight callbacks to complete with the new handlers
            // This reduces the race condition window, though the WebSocketConnectionManager
            // destructor will ultimately handle the final cleanup synchronously
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            LOGINFO("WebSocket cleanup completed - handlers replaced and brief stabilization period completed");
        }

        void AppGatewayResponderImplementation::CreateWeakSelf()
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mWeakSelfLock);
            
            if (!mWeakSelfHolder) {
                // Create a shared_ptr that manages the object lifetime via COM reference counting
                // This avoids circular reference by not storing a strong reference to self
                SharedPtr tempShared(this, [](AppGatewayResponderImplementation*){
                    // No-op deleter since COM manages the lifetime
                });
                
                // Store only the weak_ptr in a holder that jobs can access
                mWeakSelfHolder = std::make_shared<AppGatewayResponderImplementation::WeakPtr>(tempShared);
            }
        }

        AppGatewayResponderImplementation::WeakPtr AppGatewayResponderImplementation::GetWeakSelf() const
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mWeakSelfLock);
            
            if (mWeakSelfHolder) {
                return *mWeakSelfHolder;
            }
            
            return AppGatewayResponderImplementation::WeakPtr();
        }

        void AppGatewayResponderImplementation::ClearWeakSelf()
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mWeakSelfLock);
            
            if (mWeakSelfHolder) {
                // Reset the weak_ptr to expired state
                mWeakSelfHolder->reset();
                mWeakSelfHolder.reset();
            }
        }

    } // namespace Plugin
} // namespace WPEFramework
