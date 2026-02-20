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
 * 2. Define the telemetry client in your plugin's .cpp file (top-level, before namespace):
 *    AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_BADGER)
 * 
 * 3. Initialize the telemetry client in your plugin's Initialize/Configure:
 *    AGW_TELEMETRY_INIT(mService)
 * 
 *    (Optional) Record plugin bootstrap time using RAII:
 *    AGW_RECORD_BOOTSTRAP_TIME();  // Timer starts here
 *    // ... plugin initialization code ...
 *    // Timer automatically records on scope exit
 * 
 * 4. Report events using the macros (all require context parameter):
 *    - AGW_REPORT_API_ERROR(context, "GetSettings", AGW_ERROR_TIMEOUT)
 *    - AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE)
 *    - AGW_REPORT_API_LATENCY(context, "GetSettings", 123.45)
 *    - AGW_SCOPED_API_TIMER(timer, context, "GetSettings")  // RECOMMENDED for API methods
 * 
 * 5. Cleanup in Deinitialize:
 *    AGW_TELEMETRY_DEINIT()
 * 
 * ## Marker Design
 * 
 * Generic markers are used with plugin/method names included in the data payload:
 * - AGW_MARKER_PLUGIN_API_ERROR: { "plugin": "Badger", "api": "GetSettings", "error": "TIMEOUT" }
 * - AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR: { "plugin": "OttServices", "service": "ThorPermissionService", "error": "CONNECTION_TIMEOUT" }
 * - Latency metrics use composite names: AppGwBadger_GetSettings_Latency_split
 * - AGW_MARKER_PLUGIN_API_LATENCY: { "plugin": "Badger", "api": "GetSettings", "latency_ms": 123.45 }
 */

#include <interfaces/IAppGateway.h>
#include <chrono>
#include "UtilsLogging.h"
#include "UtilsCallsign.h"
#include "AppGatewayTelemetryMarkers.h"

// Ensure gDefaultLogLevel from utils.h (via UtilsCallsign.h) is considered used
// by checking the log level at static initialization time
namespace {
    static inline bool __CheckLogLevelInitialized() {
        return (gDefaultLogLevel >= FATAL_LEVEL);
    }
    static const bool __logLevelChecked [[maybe_unused]] = __CheckLogLevelInitialized();
}

namespace WPEFramework {
namespace Plugin {
namespace AppGatewayTelemetryHelper {

    /**
     * @brief Telemetry client that manages connection to AppGateway's IAppGatewayTelemetry
     * 
     * This class provides a RAII-style wrapper for the telemetry interface.
     * It automatically acquires and releases the COM-RPC interface.
     * 
     * ## Events vs Metrics
     * 
     * **Events** (RecordEvent/RecordTelemetryEvent):
     * - For individual occurrences that happen at specific points in time
     * - Contains JSON payload with context about what happened
     * - Each event is sent to T2 immediately or queued individually
     * - Use for: errors, state changes, user actions
     * - Example: API error, service failure, user login
     * 
     * **Metrics** (RecordMetric/RecordTelemetryMetric):
     * - For numeric values that should be aggregated over time
     * - Aggregation includes: sum, count, min, max, average
     * - Aggregated values sent to T2 periodically (e.g., hourly)
     * - Use for: latencies, counters, measurements
     * - Example: API latency, service latency, request count
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
            mTelemetry = service->QueryInterfaceByCallsign<Exchange::IAppGatewayTelemetry>(APP_GATEWAY_CALLSIGN);
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
         * @param context Gateway context with request/connection/app info
         * @param eventName Event name (used as T2 marker)
         * @param eventData JSON string with event details
         * @return Core::hresult
         */
        Core::hresult RecordEvent(const Exchange::GatewayContext& context,
                                  const std::string& eventName, 
                                  const std::string& eventData)
        {
            if (!IsAvailable()) {
                return Core::ERROR_UNAVAILABLE;
            }

            return mTelemetry->RecordTelemetryEvent(context, eventName, eventData);
        }

        /**
         * @brief Record a telemetry metric
         * @param context Gateway context with request/connection/app info
         * @param metricName Metric name (used as T2 marker)
         * @param value Numeric value
         * @param unit Unit of measurement
         * @return Core::hresult
         */
        Core::hresult RecordMetric(const Exchange::GatewayContext& context,
                                   const std::string& metricName, 
                                   double value, 
                                   const std::string& unit)
        {
            if (!IsAvailable()) {
                return Core::ERROR_UNAVAILABLE;
            }

            return mTelemetry->RecordTelemetryMetric(context, metricName, value, unit);
        }

        /**
         * @brief Record an API error event (individual occurrence)
         * @param context Gateway context with request/connection/app info
         * @param apiName Name of the API that failed
         * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h
         * @return Core::hresult
         * 
         * This records an EVENT - each error occurrence is sent individually to T2.
         * Use this to track WHAT errors happened, not how many.
         */
        Core::hresult RecordApiError(const Exchange::GatewayContext& context,
                                     const std::string& apiName, 
                                     const std::string& errorCode)
        {
            JsonObject data;
            data["plugin"] = mPluginName;
            data["api"] = apiName;
            data["error"] = errorCode;

            std::string eventData;
            data.ToString(eventData);
            
            LOGTRACE("TelemetryClient: Recording API error - plugin=%s, api=%s, error=%s",
                     mPluginName.c_str(), apiName.c_str(), errorCode.c_str());

            return RecordEvent(context, AGW_MARKER_PLUGIN_API_ERROR, eventData);
        }

        /**
         * @brief Record an external service error event (individual occurrence)
         * @param context Gateway context with request/connection/app info
         * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h
         * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h
         * @return Core::hresult
         * 
         * This records an EVENT - each error occurrence is sent individually to T2.
         * Use this to track WHAT service errors happened, not how many.
         */
        Core::hresult RecordExternalServiceError(const Exchange::GatewayContext& context,
                                                  const std::string& serviceName, 
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

            return RecordEvent(context, AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR, eventData);
        }

        /**
         * @brief Record an API latency metric (aggregated value)
         * @param context Gateway context with request/connection/app info
         * @param apiName Name of the API
         * @param latencyMs Latency in milliseconds
         * @return Core::hresult
         * 
         * This records a METRIC - values are aggregated (sum/count/min/max/avg)
         * and sent to T2 periodically using common marker AGW_METRIC_API_LATENCY.
         * 
         * Generates a tagged metric name with explicit structure:
         *   "AppGw_PluginName_" + <PluginName> + "_ApiName_" + <ApiName> + "_ApiLatency_split"
         * Example: "AppGw_PluginName_Badger_ApiName_GetSettings_ApiLatency_split"
         * 
         * The explicit tags (PluginName_, ApiName_) make the metric unambiguous and
         * allow precise parsing to extract plugin/API names for aggregation.
         */
        Core::hresult RecordApiLatency(const Exchange::GatewayContext& context,
                                       const std::string& apiName, 
                                       double latencyMs)
        {
            std::string metricName = "AppGw_PluginName_" + mPluginName + "_ApiName_" + apiName + "_ApiLatency_split";
            
            LOGTRACE("TelemetryClient: Recording API latency - plugin=%s, api=%s, latency=%.2fms, metric=%s",
                     mPluginName.c_str(), apiName.c_str(), latencyMs, metricName.c_str());

            return RecordMetric(context, metricName, latencyMs, AGW_UNIT_MILLISECONDS);
        }

        /**
         * @brief Record an external service latency metric (aggregated value)
         * @param context Gateway context with request/connection/app info
         * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h
         * @param latencyMs Latency in milliseconds
         * @return Core::hresult
         * 
         * This records a METRIC - values are aggregated (sum/count/min/max/avg)
         * and sent to T2 periodically using common marker AGW_METRIC_SERVICE_LATENCY.
         * 
         * Generates a tagged metric name with explicit structure:
         *   "AppGw_PluginName_" + <PluginName> + "_ServiceName_" + <ServiceName> + "_ServiceLatency_split"
         * Example: "AppGw_PluginName_OttServices_ServiceName_ThorPermissionService_ServiceLatency_split"
         * 
         * The explicit tags (PluginName_, ServiceName_) make the metric unambiguous and
         * allow precise parsing to extract plugin/service names for aggregation.
         */
        Core::hresult RecordServiceLatency(const Exchange::GatewayContext& context,
                                           const std::string& serviceName, 
                                           double latencyMs)
        {
            std::string metricName = "AppGw_PluginName_" + mPluginName + "_ServiceName_" + serviceName + "_ServiceLatency_split";
            
            LOGTRACE("TelemetryClient: Recording service latency - plugin=%s, service=%s, latency=%.2fms, metric=%s",
                     mPluginName.c_str(), serviceName.c_str(), latencyMs, metricName.c_str());

            return RecordMetric(context, metricName, latencyMs, AGW_UNIT_MILLISECONDS);
        }

        /**
         * @brief Record plugin bootstrap time
         * @param durationMs Bootstrap duration in milliseconds
         * @return Core::hresult
         * 
         * Reports the time taken for this plugin to initialize.
         * Uses standard bootstrap metric marker. AppGatewayTelemetry aggregates
         * all plugin bootstrap times and increments the plugin counter automatically.
         */
        Core::hresult RecordBootstrapTime(uint64_t durationMs)
        {
            Exchange::GatewayContext context;
            context.requestId = 0;
            context.connectionId = 0;
            context.appId = mPluginName;  // Plugin identity in context

            LOGINFO("TelemetryClient: Recording bootstrap time - plugin=%s, duration=%llums",
                    mPluginName.c_str(), (unsigned long long)durationMs);

            // Use standard bootstrap metric - AppGatewayTelemetry will handle cumulative tracking
            return RecordMetric(context, AGW_METRIC_BOOTSTRAP_DURATION, 
                              static_cast<double>(durationMs), AGW_UNIT_MILLISECONDS);
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

} // namespace AppGatewayTelemetryHelper
} // namespace Plugin
} // namespace WPEFramework

//=============================================================================
// RAII HELPER CLASSES FOR AUTOMATIC TIMING
//=============================================================================

namespace WPEFramework {
namespace Plugin {
namespace AppGatewayTelemetryHelper {

    /**
     * @brief RAII timer for automatic bootstrap time tracking
     * 
     * Takes a TelemetryClient pointer to report bootstrap time via COM-RPC.
     * Timer starts on construction and reports on destruction.
     * 
     * @note This class is used by AGW_RECORD_BOOTSTRAP_TIME() macro.
     *       Direct instantiation is not recommended.
     */
    class ScopedBootstrapTimer
    {
    public:
        ScopedBootstrapTimer(TelemetryClient* client)
            : mClient(client)
            , mStartTime(std::chrono::steady_clock::now())
        {
        }

        ~ScopedBootstrapTimer()
        {
            if (!mClient || !mClient->IsAvailable()) {
                return;
            }

            auto endTime = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - mStartTime).count();
            
            // Report bootstrap time via TelemetryClient (COM-RPC to AppGateway)
            mClient->RecordBootstrapTime(static_cast<uint64_t>(durationMs));
        }

    private:
        TelemetryClient* mClient;
        std::chrono::steady_clock::time_point mStartTime;
    };

    /**
     * @brief RAII timer for automatic API latency and error tracking
     * 
     * Times an API call from construction to destruction, automatically reporting:
     * - Success latency metric (if SetFailed() not called)
     * - Error event + error latency metric (if SetFailed() called)
     * 
     * @note This class is used by AGW_SCOPED_API_TIMER() macro.
     *       Direct instantiation is not recommended.
     * 
     * Usage:
     *   {
     *       ScopedApiTimer timer(&GetLocalTelemetryClient(), context, "GetSettings");
     *       // ... do API work ...
     *       if (failed) timer.SetFailed("TIMEOUT");
     *   } // Timer automatically reports on destruction
     */
    class ScopedApiTimer
    {
    public:
        ScopedApiTimer(TelemetryClient* client, const Exchange::GatewayContext& context, const std::string& apiName)
            : mClient(client)
            , mContext(context)
            , mApiName(apiName)
            , mFailed(false)
            , mErrorDetails()
            , mStartTime(std::chrono::steady_clock::now())
        {
        }

        ~ScopedApiTimer()
        {
            if (!mClient || !mClient->IsAvailable()) {
                return;
            }

            auto endTime = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - mStartTime).count();
            
            if (mFailed) {
                mClient->RecordApiError(mContext, mApiName, mErrorDetails);
                std::string metricName = "AppGw_PluginName_" + mClient->GetPluginName() +
                                         "_MethodName_" + mApiName + "_Error_split";
                mClient->RecordMetric(mContext, metricName, static_cast<double>(durationMs), AGW_UNIT_MILLISECONDS);
            } else {
                std::string metricName = "AppGw_PluginName_" + mClient->GetPluginName() +
                                         "_MethodName_" + mApiName + "_Success_split";
                mClient->RecordMetric(mContext, metricName, static_cast<double>(durationMs), AGW_UNIT_MILLISECONDS);
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
        TelemetryClient* mClient;
        Exchange::GatewayContext mContext;
        std::string mApiName;
        bool mFailed;
        std::string mErrorDetails;
        std::chrono::steady_clock::time_point mStartTime;
    };

} // namespace AppGatewayTelemetryHelper
} // namespace Plugin
} // namespace WPEFramework

//=============================================================================
// TELEMETRY REPORTING MACROS
//=============================================================================

/**
 * This section provides convenience macros for reporting telemetry from plugins.
 * Macros are organized into categories by their purpose and usage pattern.
 * 
 * ## Events vs Metrics
 * 
 * **Events** (via RecordTelemetryEvent):
 * - Individual occurrences reported immediately
 * - Contains JSON payload with contextual information
 * - Not aggregated - each event sent separately to T2
 * - Use for: errors, state changes, significant occurrences
 * - Example: API error with error code, service failure with details
 * 
 * **Metrics** (via RecordTelemetryMetric):
 * - Numeric values aggregated over time (sum, count, min, max, avg)
 * - Reported periodically to T2 (e.g., hourly summary)
 * - Use for: latencies, counters, measurements
 * - Example: API latency, call counts, success rates
 * 
 * ## Macro Categories
 * 
 * 1. **Framework/Initialization**: Setup and teardown of telemetry client
 * 2. **Bootstrap Time Tracking**: Measure plugin initialization time
 * 3. **Error Reporting (Events)**: Report API and service errors
 * 4. **Latency Tracking (Metrics)**: Report timing measurements
 * 5. **Generic Telemetry**: Low-level event/metric reporting
 */

//=============================================================================
// 1. FRAMEWORK/INITIALIZATION MACROS
//=============================================================================

/**
 * @brief Define a telemetry client instance for this plugin
 * @param pluginName Plugin name constant from AppGatewayTelemetryMarkers.h (e.g., AGW_PLUGIN_BADGER)
 * 
 * This macro MUST be called once in each plugin's implementation file (.cpp) to create
 * a plugin-specific telemetry client instance. Each plugin gets its own separate instance
 * to avoid conflicts when multiple plugins report telemetry simultaneously.
 * 
 * **IMPORTANT**: Place this macro at the top of your plugin's .cpp file, outside any class/function.
 * 
 * Example in Badger.cpp:
 *   #include "UtilsAppGatewayTelemetry.h"
 *   
 *   AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_BADGER)
 *   
 *   namespace WPEFramework {
 *   namespace Plugin {
 *       // ... rest of implementation
 *   }}
 * 
 * Example in OttServices.cpp:
 *   #include "UtilsAppGatewayTelemetry.h"
 *   
 *   AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_OTTSERVICES)
 *   
 *   namespace WPEFramework {
 *   namespace Plugin {
 *       // ... rest of implementation
 *   }}
 */
#define AGW_DEFINE_TELEMETRY_CLIENT(pluginName) \
    namespace { \
        WPEFramework::Plugin::AppGatewayTelemetryHelper::TelemetryClient& GetLocalTelemetryClient() { \
            static WPEFramework::Plugin::AppGatewayTelemetryHelper::TelemetryClient instance; \
            return instance; \
        } \
        const char* GetLocalPluginName() { \
            return pluginName; \
        } \
    }


/**
 * @brief Initialize the AppGateway telemetry client
 * @param service PluginHost::IShell* pointer
 * 
 * Call this in your plugin's Initialize() method to connect to AppGateway's telemetry interface.
 * The plugin name was already specified in AGW_DEFINE_TELEMETRY_CLIENT macro.
 * 
 * Example:
 *   const string MyPlugin::Initialize(PluginHost::IShell* service) {
 *       AGW_TELEMETRY_INIT(service);
 *       // ... rest of initialization ...
 *   }
 */
#define AGW_TELEMETRY_INIT(service) \
    do { \
        GetLocalTelemetryClient().Initialize(service, GetLocalPluginName()); \
    } while(0)

/**
 * @brief Deinitialize the AppGateway telemetry client
 * 
 * Call this in your plugin's Deinitialize() method to release the telemetry interface.
 * 
 * Example:
 *   void MyPlugin::Deinitialize(PluginHost::IShell* service) {
 *       AGW_TELEMETRY_DEINIT();
 *   }
 */
#define AGW_TELEMETRY_DEINIT() \
    do { \
        GetLocalTelemetryClient().Deinitialize(); \
    } while(0)

/**
 * @brief Check if telemetry client is available and ready to use
 * @return bool - true if telemetry is available
 * 
 * Use this to check telemetry availability before manual reporting.
 * Not needed for AGW_REPORT_* macros (they check internally).
 * 
 * Example:
 *   if (AGW_TELEMETRY_AVAILABLE()) {
 *       // Telemetry is ready
 *   }
 */
#define AGW_TELEMETRY_AVAILABLE() \
    GetLocalTelemetryClient().IsAvailable()

//=============================================================================
// 2. BOOTSTRAP TIME TRACKING MACROS
//=============================================================================

/**
 * @brief Record plugin bootstrap time using RAII (RECOMMENDED)
 * 
 * Creates a scoped timer that automatically measures bootstrap time from
 * invocation until the end of the current scope (typically end of Initialize).
 * Reports the bootstrap time via TelemetryClient to AppGateway.
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryMetric internally
 * - Reports to standard marker: AGW_METRIC_BOOTSTRAP_DURATION
 * - AppGateway aggregates all plugin bootstrap times cumulatively
 * - AppGateway tracks total plugins loaded and total bootstrap time
 * 
 * Example:
 *   const string MyPlugin::Initialize(PluginHost::IShell* service) {
 *       AGW_RECORD_BOOTSTRAP_TIME();  // Timer starts here
 *       
 *       // ... plugin initialization code ...
 *       AGW_TELEMETRY_INIT(service);  // Initialize telemetry
 *       
 *       return EMPTY_STRING;
 *   } // Timer automatically records on scope exit
 */
#define AGW_RECORD_BOOTSTRAP_TIME() \
    WPEFramework::Plugin::AppGatewayTelemetryHelper::ScopedBootstrapTimer __bootstrapTimer(&GetLocalTelemetryClient())

//=============================================================================
// 3. ERROR REPORTING MACROS (Events via RecordTelemetryEvent)
//=============================================================================

/**
 * @brief Report an API error event to AppGateway telemetry
 * @param context Gateway context with request/connection/app info
 * @param apiName Name of the API that failed
 * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h (e.g., AGW_ERROR_TIMEOUT)
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryEvent internally (individual occurrence)
 * - Marker: AGW_MARKER_PLUGIN_API_ERROR
 * - Payload: {"plugin": "<name>", "api": "<apiName>", "error": "<errorCode>"}
 * - Each error reported individually to T2 (not aggregated)
 * 
 * **When to Use**:
 * - Use for tracking WHAT errors occurred (forensics)
 * - For error counting, use AGW_SCOPED_API_TIMER instead (aggregates metrics)
 * 
 * Example:
 *   AGW_REPORT_API_ERROR(context, "GetSettings", AGW_ERROR_TIMEOUT)
 *   AGW_REPORT_API_ERROR(context, "GetAppPermissions", AGW_ERROR_PERMISSION_DENIED)
 */
#define AGW_REPORT_API_ERROR(context, apiName, errorCode) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordApiError(context, apiName, errorCode); \
        } \
    } while(0)

/**
 * @brief Report an external service error to AppGateway telemetry
 * @param context Gateway context with request/connection/app info
 * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h (e.g., AGW_SERVICE_OTT_SERVICES)
 * @param errorCode Predefined error code from AppGatewayTelemetryMarkers.h (e.g., AGW_ERROR_INTERFACE_UNAVAILABLE)
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryEvent internally (individual occurrence)
 * - Marker: AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR
 * - Payload: {"plugin": "<name>", "service": "<serviceName>", "error": "<errorCode>"}
 * - Each error reported individually to T2 (not aggregated)
 * 
 * **When to Use**:
 * - Use for tracking WHAT service errors occurred (forensics)
 * - For error counting, use custom metrics or aggregate manually
 * 
 * Example:
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE)
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_CONNECTION_TIMEOUT)
 */
#define AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, serviceName, errorCode) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordExternalServiceError(context, serviceName, errorCode); \
        } \
    } while(0)

//=============================================================================
// 4. LATENCY TRACKING MACROS (Metrics via RecordTelemetryMetric)
//=============================================================================

/**
 * @brief Automatic API timing with RAII (RECOMMENDED for API methods)
 * @param varName Variable name for the timer
 * @param context Gateway context with request/connection/app info
 * @param apiName Name of the API being timed
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryMetric internally (aggregated values)
 * - On success: Records metric "AppGw_PluginName_<Plugin>_MethodName_<API>_Success_split"
 * - On failure: Records event (RecordTelemetryEvent) + metric with "_Error_split" suffix
 * - Metrics aggregated by AppGateway over time (sum, count, min, max, avg)
 * 
 * **When to Use**:
 * - RECOMMENDED for all API method implementations
 * - Automatically tracks success/error rates and latencies
 * - Call SetFailed(errorCode) to mark as error, otherwise assumes success
 * 
 * Example:
 *   Core::hresult MyPlugin::SomeMethod(const Exchange::GatewayContext& context)
 *   {
 *       AGW_SCOPED_API_TIMER(timer, context, "SomeMethod");
 *       
 *       auto result = DoWork();
 *       if (result != Core::ERROR_NONE) {
 *           timer.SetFailed(AGW_ERROR_TIMEOUT);
 *           return result;
 *       }
 *       
 *       return Core::ERROR_NONE;
 *   } // Timer automatically reports success/failure with timing
 */
#define AGW_SCOPED_API_TIMER(varName, context, apiName) \
    WPEFramework::Plugin::AppGatewayTelemetryHelper::ScopedApiTimer varName(&GetLocalTelemetryClient(), context, apiName)

/**
 * @brief Report an API latency metric to AppGateway telemetry (manual)
 * @param context Gateway context with request/connection/app info
 * @param apiName Name of the API
 * @param latencyMs Latency in milliseconds
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryMetric internally (aggregated values)
 * - Metric name: "AppGw_PluginName_<Plugin>_ApiName_<API>_ApiLatency_split"
 * - AppGateway aggregates over time (sum, count, min, max, avg)
 * - Reported periodically to T2 (e.g., hourly)
 * 
 * **When to Use**:
 * - Manual latency reporting when not using AGW_SCOPED_API_TIMER
 * - Use AGW_SCOPED_API_TIMER instead for automatic timing (RECOMMENDED)
 * 
 * Example:
 *   auto start = std::chrono::steady_clock::now();
 *   DoWork();
 *   auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
 *       std::chrono::steady_clock::now() - start).count();
 *   AGW_REPORT_API_LATENCY(context, "GetSettings", durationMs);
 */
#define AGW_REPORT_API_LATENCY(context, apiName, latencyMs) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordApiLatency(context, apiName, latencyMs); \
        } \
    } while(0)

/**
 * @brief Report an external service latency metric to AppGateway telemetry
 * @param context Gateway context with request/connection/app info
 * @param serviceName Predefined service name from AppGatewayTelemetryMarkers.h
 * @param latencyMs Latency in milliseconds
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryMetric internally (aggregated values)
 * - Metric name: "AppGw_PluginName_<Plugin>_ServiceName_<Service>_ServiceLatency_split"
 * - AppGateway aggregates over time (sum, count, min, max, avg)
 * - Reported periodically to T2 (e.g., hourly)
 * 
 * **When to Use**:
 * - Track latency of external service calls (gRPC, COM-RPC, HTTP)
 * - Helps identify slow external dependencies
 * 
 * Example:
 *   auto start = std::chrono::steady_clock::now();
 *   auto result = thorPermissionClient->CheckPermission(...);
 *   auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
 *       std::chrono::steady_clock::now() - start).count();
 *   AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_THOR_PERMISSION, durationMs);
 */
#define AGW_REPORT_SERVICE_LATENCY(context, serviceName, latencyMs) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordServiceLatency(context, serviceName, latencyMs); \
        } \
    } while(0)

//=============================================================================
// 5. GENERIC TELEMETRY REPORTING MACROS (Low-level Interface)
//=============================================================================

/**
 * @brief Report a custom numeric metric to AppGateway telemetry
 * @param context Gateway context with request/connection/app info
 * @param metricName Custom metric name
 * @param value Numeric value
 * @param unit Predefined unit from AppGatewayTelemetryMarkers.h (e.g., AGW_UNIT_MILLISECONDS)
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryMetric internally (aggregated values)
 * - Direct low-level metric reporting
 * - AppGateway aggregates over time (sum, count, min, max, avg)
 * 
 * **When to Use**:
 * - Custom counters (e.g., connection count, cache hits)
 * - Custom measurements not covered by standard macros
 * - Prefer specific macros (AGW_REPORT_API_LATENCY, etc.) when available
 * 
 * Example:
 *   static uint32_t cacheHitCount = 0;
 *   cacheHitCount++;
 *   AGW_REPORT_METRIC(context, "AppGwCacheHits", 
 *                     static_cast<double>(cacheHitCount), AGW_UNIT_COUNT);
 */
#define AGW_REPORT_METRIC(context, metricName, value, unit) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordMetric(context, metricName, value, unit); \
        } \
    } while(0)

/**
 * @brief Report a custom telemetry event to AppGateway
 * @param context Gateway context with request/connection/app info
 * @param eventName Event name (becomes T2 marker)
 * @param eventData JSON string with event data
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryEvent internally (individual occurrence)
 * - Direct low-level event reporting
 * - Each event sent individually to T2 (not aggregated)
 * 
 * **When to Use**:
 * - Custom events not covered by standard macros
 * - State changes, user actions, significant occurrences
 * - Prefer specific macros (AGW_REPORT_API_ERROR, etc.) when available
 * 
 * Example:
 *   JsonObject data;
 *   data["userId"] = "12345";
 *   data["action"] = "login";
 *   std::string eventData;
 *   data.ToString(eventData);
 *   AGW_REPORT_EVENT(context, "AppGwUserLogin_split", eventData);
 */
#define AGW_REPORT_EVENT(context, eventName, eventData) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            client.RecordEvent(context, eventName, eventData); \
        } \
    } while(0)

/**
 * @brief Report a successful API call with timing information (DEPRECATED)
 * @param context Gateway context with request/connection/app info
 * @param apiName Name of the API
 * @param durationMs Duration of the call in milliseconds
 * 
 * @deprecated Use AGW_SCOPED_API_TIMER instead for automatic success/error tracking
 * 
 * **Data Flow**:
 * - Uses RecordTelemetryMetric internally
 * - Reports generic API latency metric
 * - Does not distinguish between different API methods
 * 
 * Example:
 *   AGW_REPORT_API_SUCCESS(context, "GetSettings", 45)
 */
#define AGW_REPORT_API_SUCCESS(context, apiName, durationMs) \
    do { \
        auto& client = GetLocalTelemetryClient(); \
        if (client.IsAvailable()) { \
            std::string metricName = std::string("AppGw") + client.GetPluginName() + "_ApiLatency_split"; \
            client.RecordMetric(context, metricName, static_cast<double>(durationMs), AGW_UNIT_MILLISECONDS); \
        } \
    } while(0)
