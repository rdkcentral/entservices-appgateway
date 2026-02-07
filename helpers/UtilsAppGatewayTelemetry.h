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

#pragma once

/**
 * @file UtilsAppGatewayTelemetry.h
 * @brief Helper macros and utilities for reporting telemetry to AppGateway
 * 
 * This file provides a standardized way for plugins to report telemetry data
 * to the AppGateway telemetry aggregator via COM-RPC. The AppGateway aggregates
 * data and periodically reports to the T2 telemetry server.
 * 
 * ## Quick Start
 * 
 * 1. Include this header in your plugin:
 *    #include "UtilsAppGatewayTelemetry.h"
 * 
 * 2. Initialize the telemetry client in your plugin's Initialize/Configure:
 *    AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_BADGER)
 * 
 * 3. Report events using the macros with GENERIC MARKERS and plugin/method as values:
 *    - AGW_REPORT_API_ERROR("GetSettings", AGW_ERROR_TIMEOUT)
 *    - AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE)
 *    - AGW_REPORT_API_LATENCY("GetSettings", 123.45)
 * 
 * 4. Cleanup in Deinitialize:
 *    AGW_TELEMETRY_DEINIT()
 * 
 * ## Marker Design
 * 
 * Generic markers are used with plugin/method names included in the data payload:
 * - AGW_MARKER_PLUGIN_API_ERROR: { "plugin": "Badger", "api": "GetSettings", "error": "TIMEOUT" }
 * - AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR: { "plugin": "OttServices", "service": "ThorPermissionService", "error": "CONNECTION_TIMEOUT" }
 * - AGW_MARKER_PLUGIN_API_LATENCY: { "plugin": "Badger", "api": "GetSettings", "latency_ms": 123.45 }
 */

#include <interfaces/IAppGateway.h>
#include <plugins/json/JsonData_Container.h>
#include "UtilsLogging.h"
#include "AppGatewayTelemetryMarkers.h"

namespace WPEFramework {
namespace Plugin {
namespace AppGatewayTelemetryHelper {

    /**
     * @brief Telemetry client that manages connection to AppGateway's IAppGatewayTelemetry
     * 
     * This class provides a RAII-style wrapper for the telemetry interface.
     * It automatically acquires and releases the COM-RPC interface.
     */
    class TelemetryClient
    {
    public:
        TelemetryClient()
            : mService(nullptr)
            , mTelemetry(nullptr)
            , mPluginName()
        {
        }

        ~TelemetryClient()
        {
            Deinitialize();
        }

        /**
         * @brief Initialize the telemetry client
         * @param service The IShell service pointer
         * @param pluginName Name of the plugin (used in telemetry context)
         * @return true if successful, false otherwise
         */
        bool Initialize(PluginHost::IShell* service, const std::string& pluginName)
        {
            if (service == nullptr) {
                LOGERR("TelemetryClient: service is null");
                return false;
            }

            mService = service;
            mPluginName = pluginName;

            // Query for the AppGateway telemetry interface
            mTelemetry = service->QueryInterfaceByCallsign<Exchange::IAppGatewayTelemetry>("AppGateway");
            if (mTelemetry == nullptr) {
                LOGWARN("TelemetryClient: AppGateway telemetry interface not available");
                return false;
            }

            LOGINFO("TelemetryClient: Initialized for plugin '%s'", pluginName.c_str());
            return true;
        }

        /**
         * @brief Deinitialize and release the telemetry interface
         */
        void Deinitialize()
        {
            if (mTelemetry != nullptr) {
                mTelemetry->Release();
                mTelemetry = nullptr;
            }
            mService = nullptr;
            LOGINFO("TelemetryClient: Deinitialized");
        }

        /**
         * @brief Check if the telemetry client is initialized
         * @return true if initialized and interface is available
         */
        bool IsAvailable() const
        {
            return mTelemetry != nullptr;
        }

        /**
         * @brief Record a telemetry event
         * @param eventName Event name (used as T2 marker)
         * @param eventData JSON string with event details
         * @return Core::hresult
         */
        Core::hresult RecordEvent(const std::string& eventName, const std::string& eventData)
        {
            if (!IsAvailable()) {
                return Core::ERROR_UNAVAILABLE;
            }

            Exchange::GatewayContext context;
            context.requestId = 0;
            context.connectionId = 0;
            context.appId = mPluginName.c_str();

            return mTelemetry->RecordTelemetryEvent(context, eventName, eventData);
        }

        /**
         * @brief Record a telemetry metric
         * @param metricName Metric name (used as T2 marker)
         * @param value Numeric value
         * @param unit Unit of measurement
         * @return Core::hresult
         */
        Core::hresult RecordMetric(const std::string& metricName, double value, const std::string& unit)
        {
            if (!IsAvailable()) {
                return Core::ERROR_UNAVAILABLE;
            }

            Exchange::GatewayContext context;
            context.requestId = 0;
            context.connectionId = 0;
            context.appId = mPluginName.c_str();

            return mTelemetry->RecordTelemetryMetric(context, metricName, value, unit);
        }

        /**
         * @brief Record an API error using generic marker
         * @param apiName Name of the API that failed
         * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h
         * @return Core::hresult
         */
        Core::hresult RecordApiError(const std::string& apiName, const std::string& errorCode)
        {
            JsonObject data;
            data["plugin"] = mPluginName;
            data["api"] = apiName;
            data["error"] = errorCode;

            std::string eventData;
            data.ToString(eventData);
            
            LOGTRACE("TelemetryClient: Recording API error - plugin=%s, api=%s, error=%s",
                     mPluginName.c_str(), apiName.c_str(), errorCode.c_str());

            return RecordEvent(AGW_MARKER_PLUGIN_API_ERROR, eventData);
        }

        /**
         * @brief Record an external service error using generic marker
         * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h
         * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h
         * @return Core::hresult
         */
        Core::hresult RecordExternalServiceError(const std::string& serviceName, 
                                                  const std::string& errorCode)
        {
            JsonObject data;
            data["plugin"] = mPluginName;
            data["service"] = serviceName;
            data["error"] = errorCode;

            std::string eventData;
            data.ToString(eventData);

            LOGINFO("TelemetryClient: Recording external service error - plugin=%s, service=%s, error=%s",
                    mPluginName.c_str(), serviceName.c_str(), errorCode.c_str());

            return RecordEvent(AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR, eventData);
        }

        /**
         * @brief Record an API latency metric using generic marker
         * @param apiName Name of the API
         * @param latencyMs Latency in milliseconds
         * @return Core::hresult
         */
        Core::hresult RecordApiLatency(const std::string& apiName, double latencyMs)
        {
            JsonObject data;
            data["plugin"] = mPluginName;
            data["api"] = apiName;
            data["latency_ms"] = latencyMs;

            std::string eventData;
            data.ToString(eventData);

            LOGTRACE("TelemetryClient: Recording API latency - plugin=%s, api=%s, latency=%.2fms",
                     mPluginName.c_str(), apiName.c_str(), latencyMs);

            return RecordEvent(AGW_MARKER_PLUGIN_API_LATENCY, eventData);
        }

        /**
         * @brief Record an external service latency metric using generic marker
         * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h
         * @param latencyMs Latency in milliseconds
         * @return Core::hresult
         */
        Core::hresult RecordServiceLatency(const std::string& serviceName, double latencyMs)
        {
            JsonObject data;
            data["plugin"] = mPluginName;
            data["service"] = serviceName;
            data["latency_ms"] = latencyMs;

            std::string eventData;
            data.ToString(eventData);

            LOGTRACE("TelemetryClient: Recording service latency - plugin=%s, service=%s, latency=%.2fms",
                     mPluginName.c_str(), serviceName.c_str(), latencyMs);

            return RecordEvent(AGW_MARKER_PLUGIN_SERVICE_LATENCY, eventData);
        }

        /**
         * @brief Get the plugin name
         * @return The plugin name
         */
        const std::string& GetPluginName() const
        {
            return mPluginName;
        }

    private:
        PluginHost::IShell* mService;
        Exchange::IAppGatewayTelemetry* mTelemetry;
        std::string mPluginName;
    };

    /**
     * @brief Global telemetry client instance (thread-local for safety)
     * 
     * Each plugin should have its own instance. Use the macros to access.
     */
    inline TelemetryClient& GetTelemetryClient()
    {
        static TelemetryClient instance;
        return instance;
    }

} // namespace AppGatewayTelemetryHelper
} // namespace Plugin
} // namespace WPEFramework


// ============================================================================
// CONVENIENCE MACROS FOR TELEMETRY REPORTING
// ============================================================================

/**
 * @brief Initialize the AppGateway telemetry client
 * @param service PluginHost::IShell* pointer
 * @param pluginName Plugin name constant from AppGatewayTelemetryMarkers.h (e.g., AGW_PLUGIN_BADGER)
 * 
 * Example:
 *   AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_BADGER)
 *   AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_OTTSERVICES)
 */
#define AGW_TELEMETRY_INIT(service, pluginName) \
    do { \
        WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient().Initialize(service, pluginName); \
    } while(0)

/**
 * @brief Deinitialize the AppGateway telemetry client
 * 
 * Example:
 *   AGW_TELEMETRY_DEINIT()
 */
#define AGW_TELEMETRY_DEINIT() \
    do { \
        WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient().Deinitialize(); \
    } while(0)

/**
 * @brief Check if telemetry is available
 * @return bool - true if telemetry is available
 * 
 * Example:
 *   if (AGW_TELEMETRY_AVAILABLE()) { ... }
 */
#define AGW_TELEMETRY_AVAILABLE() \
    WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient().IsAvailable()

/**
 * @brief Report an API error to AppGateway telemetry
 * @param apiName Name of the API that failed
 * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h (e.g., AGW_ERROR_TIMEOUT)
 * 
 * Uses generic marker AGW_MARKER_PLUGIN_API_ERROR with plugin name from initialization.
 * 
 * Example:
 *   AGW_REPORT_API_ERROR("GetSettings", AGW_ERROR_TIMEOUT)
 *   AGW_REPORT_API_ERROR("GetAppPermissions", AGW_ERROR_PERMISSION_DENIED)
 */
#define AGW_REPORT_API_ERROR(apiName, errorCode) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordApiError(apiName, errorCode); \
        } \
    } while(0)

/**
 * @brief Report an external service error to AppGateway telemetry
 * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h (e.g., AGW_SERVICE_OTT_SERVICES)
 * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h (e.g., AGW_ERROR_INTERFACE_UNAVAILABLE)
 * 
 * Uses generic marker AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR with plugin name from initialization.
 * 
 * Example:
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE)
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_CONNECTION_TIMEOUT)
 */
#define AGW_REPORT_EXTERNAL_SERVICE_ERROR(serviceName, errorCode) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordExternalServiceError(serviceName, errorCode); \
        } \
    } while(0)

/**
 * @brief Report an API latency metric to AppGateway telemetry
 * @param apiName Name of the API
 * @param latencyMs Latency in milliseconds
 * 
 * Uses generic marker AGW_MARKER_PLUGIN_API_LATENCY with plugin name from initialization.
 * 
 * Example:
 *   AGW_REPORT_API_LATENCY("GetSettings", 150.5)
 *   AGW_REPORT_API_LATENCY("GetAppPermissions", 250.0)
 */
#define AGW_REPORT_API_LATENCY(apiName, latencyMs) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordApiLatency(apiName, latencyMs); \
        } \
    } while(0)

/**
 * @brief Report an external service latency metric to AppGateway telemetry
 * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h
 * @param latencyMs Latency in milliseconds
 * 
 * Uses generic marker AGW_MARKER_PLUGIN_SERVICE_LATENCY with plugin name from initialization.
 * 
 * Example:
 *   AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_THOR_PERMISSION, 350.0)
 *   AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_TOKEN, 200.0)
 */
#define AGW_REPORT_SERVICE_LATENCY(serviceName, latencyMs) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordServiceLatency(serviceName, latencyMs); \
        } \
    } while(0)

/**
 * @brief Report a custom numeric metric to AppGateway telemetry
 * @param metricName Custom metric name
 * @param value Numeric value
 * @param unit Predefined unit from AppGatewayTelemetryMarkers.h (e.g., AGW_UNIT_MILLISECONDS)
 * 
 * Example:
 *   AGW_REPORT_METRIC("agw_CustomMetric", 150.5, AGW_UNIT_MILLISECONDS)
 */
#define AGW_REPORT_METRIC(metricName, value, unit) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordMetric(metricName, value, unit); \
        } \
    } while(0)

/**
 * @brief Report a custom telemetry event to AppGateway
 * @param eventName Event name (becomes T2 marker)
 * @param eventData JSON string with event data
 * 
 * Example:
 *   AGW_REPORT_EVENT("agw_UserLogin", "{\"userId\":\"123\"}")
 */
#define AGW_REPORT_EVENT(eventName, eventData) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordEvent(eventName, eventData); \
        } \
    } while(0)

/**
 * @brief Report a successful API call with timing information
 * @param apiName Name of the API
 * @param durationMs Duration of the call in milliseconds
 * 
 * Example:
 *   AGW_REPORT_API_SUCCESS("GetSettings", 45)
 */
#define AGW_REPORT_API_SUCCESS(apiName, durationMs) \
    do { \
        auto& client = WPEFramework::Plugin::AppGatewayTelemetryHelper::GetTelemetryClient(); \
        if (client.IsAvailable()) { \
            std::string metricName = std::string("agw_") + client.GetPluginName() + "ApiLatency"; \
            client.RecordMetric(metricName, static_cast<double>(durationMs), "ms"); \
        } \
    } while(0)

// ============================================================================
// SCOPED TIMER FOR AUTOMATIC LATENCY TRACKING
// ============================================================================

namespace WPEFramework {
namespace Plugin {
namespace AppGatewayTelemetryHelper {

    /**
     * @brief RAII timer for automatic API latency tracking
     * 
     * Usage:
     *   {
     *       ScopedApiTimer timer("GetSettings");
     *       // ... do API work ...
     *       if (failed) timer.SetFailed("TIMEOUT");
     *   } // Timer automatically reports on destruction
     */
    class ScopedApiTimer
    {
    public:
        ScopedApiTimer(const std::string& apiName)
            : mApiName(apiName)
            , mFailed(false)
            , mErrorDetails()
            , mStartTime(std::chrono::steady_clock::now())
        {
        }

        ~ScopedApiTimer()
        {
            auto endTime = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - mStartTime).count();

            auto& client = GetTelemetryClient();
            if (client.IsAvailable()) {
                if (mFailed) {
                    client.RecordApiError(mApiName, mErrorDetails);
                    std::string metricName = "agw_" + client.GetPluginName() + "FailedApiLatency";
                    client.RecordMetric(metricName, static_cast<double>(durationMs), "ms");
                } else {
                    std::string metricName = "agw_" + client.GetPluginName() + "ApiLatency";
                    client.RecordMetric(metricName, static_cast<double>(durationMs), "ms");
                }
            }
        }

        void SetFailed(const std::string& errorDetails)
        {
            mFailed = true;
            mErrorDetails = errorDetails;
        }

        void SetSuccess()
        {
            mFailed = false;
        }

    private:
        std::string mApiName;
        bool mFailed;
        std::string mErrorDetails;
        std::chrono::steady_clock::time_point mStartTime;
    };

} // namespace AppGatewayTelemetryHelper
} // namespace Plugin
} // namespace WPEFramework

/**
 * @brief Create a scoped timer for automatic API latency tracking
 * @param varName Variable name for the timer
 * @param apiName Name of the API being timed
 * 
 * Example:
 *   Core::hresult MyPlugin::SomeMethod()
 *   {
 *       AGW_SCOPED_API_TIMER(timer, "SomeMethod");
 *       
 *       auto result = DoWork();
 *       if (result != Core::ERROR_NONE) {
 *           timer.SetFailed("WORK_FAILED");
 *           return result;
 *       }
 *       
 *       return Core::ERROR_NONE;
 *   } // Timer automatically reports success/failure with timing
 */
#define AGW_SCOPED_API_TIMER(varName, apiName) \
    WPEFramework::Plugin::AppGatewayTelemetryHelper::ScopedApiTimer varName(apiName)
