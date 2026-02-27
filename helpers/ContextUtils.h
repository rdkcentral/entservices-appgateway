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

#ifndef __CONTEXTUTILS_H__
#define __CONTEXTUTILS_H__
#include <interfaces/IAppGateway.h>
#include <interfaces/IAppNotifications.h>

#include "UtilsCallsign.h"
using namespace WPEFramework;
using namespace std;

#define LEGACY_FIREBOLT_VERSION "0"
#define RDK8_FIREBOLT_VERSION "8"
#define RDK8_SUFFIX ".v8"
#define RDK8_SUFFIX_LENGTH 3
#define ENABLE_DEBUG_FOR_CONNECTION "enableDebugForConnection"
#define DISABLE_DEBUG_FOR_CONNECTION "disableDebugForConnection"

class ContextUtils {
    public:
        // Implement a static method which accepts a Exchange::IAppNotifications::Context object and
        // converts it into Exchange::IAppGateway::Context object
        static Exchange::GatewayContext ConvertNotificationToAppGatewayContext(const Exchange::IAppNotifications::AppNotificationContext& notificationsContext){
            Exchange::GatewayContext appGatewayContext;
            // Perform the conversion logic here
            appGatewayContext.requestId = notificationsContext.requestId;
            appGatewayContext.connectionId = notificationsContext.connectionId;
            appGatewayContext.appId = notificationsContext.appId;
            appGatewayContext.version = notificationsContext.version;
            return appGatewayContext;
        }

        // Implement a static method which accepts a Exchange::IAppGateway::Context object and
        // converts it into Exchange::IAppNotifications::Context object
        static Exchange::IAppNotifications::AppNotificationContext ConvertAppGatewayToNotificationContext(const Exchange::GatewayContext& appGatewayContext, const string& origin){
            Exchange::IAppNotifications::AppNotificationContext notificationsContext;
            // Perform the conversion logic here
            notificationsContext.requestId = appGatewayContext.requestId;
            notificationsContext.connectionId = appGatewayContext.connectionId;
            notificationsContext.appId = appGatewayContext.appId;
            notificationsContext.origin = origin;
            notificationsContext.version = appGatewayContext.version;
            return notificationsContext;
        }

        static bool IsOriginGateway(const string& origin) {
            return origin == APP_GATEWAY_CALLSIGN;
        }

        static bool IsRDK8Compliant(const string& version) {
            return version == RDK8_FIREBOLT_VERSION;
        }

        static string GetEventNameFromContextBasedonVersion(const Exchange::IAppNotifications::AppNotificationContext& context, const string& baseEventName) {
            if (context.version == RDK8_FIREBOLT_VERSION) {
                return GetRDK8VersionedEventName(baseEventName);
            }
            return baseEventName;
        }

        static string GetRDK8VersionedEventName(const string& baseEventName) {
            return baseEventName + RDK8_SUFFIX;
        }

        static string GetBaseEventNameFromVersionedEvent(const string& versionedEventName) {
            if (versionedEventName.size() > RDK8_SUFFIX_LENGTH && versionedEventName.substr(versionedEventName.size() - RDK8_SUFFIX_LENGTH) == RDK8_SUFFIX) {
                return versionedEventName.substr(0, versionedEventName.size() - RDK8_SUFFIX_LENGTH);
            }
            return versionedEventName;
        }
};
#endif
