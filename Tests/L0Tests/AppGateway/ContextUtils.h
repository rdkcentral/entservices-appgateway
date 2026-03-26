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

#include "StringUtils.h"
#include "UtilsCallsign.h"

#ifndef LEGACY_FIREBOLT_VERSION
#define LEGACY_FIREBOLT_VERSION "0"
#endif
#ifndef RDK8_FIREBOLT_VERSION
#define RDK8_FIREBOLT_VERSION "8"
#endif
#ifndef RDK8_SUFFIX
#define RDK8_SUFFIX ".v8"
#endif
#ifndef RDK8_SUFFIX_LENGTH
#define RDK8_SUFFIX_LENGTH 3
#endif

class ContextUtils {
public:
    // Convert Exchange::IAppNotifications::AppNotificationContext -> Exchange::GatewayContext
    static WPEFramework::Exchange::GatewayContext ConvertNotificationToAppGatewayContext(const WPEFramework::Exchange::IAppNotifications::AppNotificationContext& notificationsContext)
    {
        WPEFramework::Exchange::GatewayContext appGatewayContext;
        appGatewayContext.requestId = notificationsContext.requestId;
        appGatewayContext.connectionId = notificationsContext.connectionId;
        appGatewayContext.appId = notificationsContext.appId;
        appGatewayContext.version = notificationsContext.version;
        return appGatewayContext;
    }

    // Convert Exchange::GatewayContext -> Exchange::IAppNotifications::AppNotificationContext
    static WPEFramework::Exchange::IAppNotifications::AppNotificationContext ConvertAppGatewayToNotificationContext(const WPEFramework::Exchange::GatewayContext& appGatewayContext, const std::string& origin)
    {
        WPEFramework::Exchange::IAppNotifications::AppNotificationContext notificationsContext;
        notificationsContext.requestId = appGatewayContext.requestId;
        notificationsContext.connectionId = appGatewayContext.connectionId;
        notificationsContext.appId = appGatewayContext.appId;
        notificationsContext.origin = origin;
        notificationsContext.version = appGatewayContext.version;
        return notificationsContext;
    }

    // Convert Exchange::GatewayContext -> Exchange::IApp2AppProvider::Context
    // static WPEFramework::Exchange::IApp2AppProvider::Context ConvertAppGatewayToProviderContext(const WPEFramework::Exchange::GatewayContext& appGatewayContext, const std::string& origin)
    // {
    //     WPEFramework::Exchange::IApp2AppProvider::Context providerContext;
    //     providerContext.requestId = appGatewayContext.requestId;
    //     providerContext.connectionId = appGatewayContext.connectionId;
    //     providerContext.appId = appGatewayContext.appId;
    //     providerContext.origin = origin;
    //     return providerContext;
    // }

    // // Convert Exchange::IApp2AppProvider::Context -> Exchange::GatewayContext
    // static WPEFramework::Exchange::GatewayContext ConvertProviderToAppGatewayContext(const WPEFramework::Exchange::IApp2AppProvider::Context& providerContext)
    // {
    //     WPEFramework::Exchange::GatewayContext appGatewayContext;
    //     appGatewayContext.requestId = static_cast<uint32_t>(providerContext.requestId);
    //     appGatewayContext.connectionId = providerContext.connectionId;
    //     appGatewayContext.appId = providerContext.appId;
    //     return appGatewayContext;
    // }

    static bool IsOriginGateway(const std::string& origin)
    {
        return StringUtils::rfindInsensitive(origin, APP_GATEWAY_CALLSIGN);
    }

    static bool IsRDK8Compliant(const std::string& version)
    {
        return RDK8_FIREBOLT_VERSION == version;
    }

    static std::string GetEventNameFromContextBasedonVersion(const std::string& version, const std::string& baseEventName)
    {
        if (RDK8_FIREBOLT_VERSION == version) {
            return GetRDK8VersionedEventName(baseEventName);
        }
        return baseEventName;
    }

    static std::string GetRDK8VersionedEventName(const std::string& baseEventName)
    {
        return baseEventName + RDK8_SUFFIX;
    }

    static std::string GetBaseEventNameFromVersionedEvent(const std::string& versionedEventName)
    {
        if (versionedEventName.size() > RDK8_SUFFIX_LENGTH &&
            RDK8_SUFFIX == versionedEventName.substr(versionedEventName.size() - RDK8_SUFFIX_LENGTH)) {
            return versionedEventName.substr(0, versionedEventName.size() - RDK8_SUFFIX_LENGTH);
        }
        return versionedEventName;
    }
};

#endif
