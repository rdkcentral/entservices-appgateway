/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#pragma once

#include "Module.h"
#include <interfaces/IAppGateway.h>
#include <core/core.h>
#include <map>
#include <atomic>
#include <chrono>

// Include consolidated telemetry markers
#include "AppGatewayTelemetryMarkers.h"

// Default reporting interval in seconds (1 hour)
#define TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC             30

// Default cache threshold (number of records before forced flush)
#define TELEMETRY_DEFAULT_CACHE_THRESHOLD                    1000

namespace WPEFramework {
namespace Plugin {

    /**
     * @brief Telemetry output format enumeration
     * 
     * Determines how telemetry data is formatted before sending to T2:
     * - JSON: Full JSON objects with field names (more verbose, self-describing)
     * - COMPACT: Comma-separated values (smaller payload, requires schema knowledge)
     */
    enum class TelemetryFormat
    {
        JSON,       // {"field1":"value1","field2":123} - Self-describing, extensible
        COMPACT     // value1,value2,123 - Minimal size, requires external schema
    };

    /**
     * @brief AppGatewayTelemetry - Telemetry aggregator for App Gateway
     * 
     * This class implements the IAppGatewayTelemetry interface and acts as a
     * centralized telemetry aggregator. Other plugins (Badger, OttServices, etc.)
     * can report their telemetry data via COM-RPC, and AppGateway aggregates
     * and sends to the T2 server.
     * 
     * It tracks:
     * - Bootstrap time: Time taken to initialize all plugins
     * - Health stats: WebSocket connections, total/successful/failed calls
     * - API error stats: APIs that failed and their failure counts
     * - External service errors: Failures from external services (GrpsServer, ThorPermission, etc.)
     * 
     * Data is reported via T2 telemetry at configurable intervals (default 1 hour)
     * or when cache threshold is reached.
     */
    class AppGatewayTelemetry : public Exchange::IAppGatewayTelemetry
    {
    public:
        // Singleton access for internal components
        static AppGatewayTelemetry& getInstance();

        // Prevent copying
        AppGatewayTelemetry(const AppGatewayTelemetry&) = delete;
        AppGatewayTelemetry& operator=(const AppGatewayTelemetry&) = delete;

        BEGIN_INTERFACE_MAP(AppGatewayTelemetry)
            INTERFACE_ENTRY(Exchange::IAppGatewayTelemetry)
        END_INTERFACE_MAP

        // IAppGatewayTelemetry Interface Implementation
        // (Called by external plugins via COM-RPC)
        
        /**
         * @brief Records a telemetry event from external plugins
         * 
         * The eventName acts as the marker for T2 telemetry.
         * For API errors, use eventName like "agw_BadgerApiError" with eventData containing error details.
         * For service errors, use eventName like "agw_OttExternalServiceError" with eventData containing service info.
         * 
         * @param context Gateway context with caller info
         * @param eventName Event name (used as T2 marker)
         * @param eventData Event data in JSON format
         * @return Core::hresult Success or error code
         */
        Core::hresult RecordTelemetryEvent(const Exchange::GatewayContext& context,
                                           const string& eventName,
                                           const string& eventData) override;

        /**
         * @brief Records a telemetry metric from external plugins
         * 
         * The metricName acts as the marker for T2 telemetry.
         * Metrics are aggregated (sum, min, max, count) and reported periodically.
         * 
         * Example metric names:
         * - "agw_BadgerApiLatency" for Badger API latency in milliseconds
         * - "agw_OttStreamingBitrate" for OTT streaming bitrate in kbps
         * 
         * @param context Gateway context with caller info
         * @param metricName Metric name (used as T2 marker)
         * @param metricValue Metric value
         * @param metricUnit Metric unit (e.g., "ms", "kbps", "count")
         * @return Core::hresult Success or error code
         */
        Core::hresult RecordTelemetryMetric(const Exchange::GatewayContext& context,
                                            const string& metricName,
                                            const double metricValue,
                                            const string& metricUnit) override;

        // Internal Methods (for AppGateway components)

        // Initialization and configuration
        void Initialize(PluginHost::IShell* service);
        void Deinitialize();

        // Configuration
        void SetReportingInterval(uint32_t intervalSec);
        void SetCacheThreshold(uint32_t threshold);
        
        /**
         * @brief Set the telemetry output format
         * 
         * @param format TelemetryFormat::JSON for self-describing JSON payloads,
         *               TelemetryFormat::COMPACT for comma-separated values
         */
        void SetTelemetryFormat(TelemetryFormat format);
        
        /**
         * @brief Get the current telemetry output format
         * @return Current TelemetryFormat setting
         */
        TelemetryFormat GetTelemetryFormat() const;

        // Manual flush (for testing or shutdown)
        void FlushTelemetryData();

        // Scenario 1: Bootstrap Time Recording
        // Each plugin reports its own bootstrap duration. AppGatewayTelemetry tracks
        // the cumulative total and increments the plugin count automatically.
        void RecordBootstrapTime(uint64_t durationMs);

        // Scenario 2: Health Stats Tracking
        // These counters track AppGateway's OWN WebSocket API operations ONLY.
        // They should be incremented by AppGateway when handling incoming WebSocket API requests,
        // NOT when aggregating plugin telemetry (which happens via RecordTelemetryEvent/RecordTelemetryMetric).
        // These health stats are independent of plugin-level metrics reported via helper macros.
        void IncrementWebSocketConnections();
        void DecrementWebSocketConnections();
        void IncrementTotalCalls();
        void IncrementSuccessfulCalls();
        void IncrementFailedCalls();

        // Scenario 3: API Error Tracking (Internal)
        // Errors are counted, then sent as METRICS periodically
        void RecordApiError(const std::string& apiName);

        // Scenario 4: External Service Error Tracking (Internal)
        // Service errors are counted, then sent as METRICS periodically
        void RecordExternalServiceErrorInternal(const std::string& serviceName);

        AppGatewayTelemetry();
        ~AppGatewayTelemetry() override;

    private:
        /**
         * @brief Metric data structure for aggregation
         */
        struct MetricData
        {
            double sum;
            double min;
            double max;
            uint32_t count;
            std::string unit;

            MetricData() : sum(0.0), min(std::numeric_limits<double>::max()),
                          max(std::numeric_limits<double>::lowest()), count(0) {}
        };

        /**
         * @brief Health statistics structure
         */
        struct HealthStats
        {
            std::atomic<uint32_t> websocketConnections{0};
            std::atomic<uint32_t> totalCalls{0};
            std::atomic<uint32_t> successfulCalls{0};
            std::atomic<uint32_t> failedCalls{0};
        };

        /**
         * @brief Per-Plugin/API method statistics structure
         * 
         * Tracks detailed counters and latency metrics for each plugin/method combination.
         * This enables per-API visibility into performance and reliability.
         */
        struct ApiMethodStats
        {
            std::string pluginName;
            std::string methodName;
            uint32_t successCount;
            uint32_t errorCount;
            double totalSuccessLatencyMs;
            double totalErrorLatencyMs;
            double minSuccessLatencyMs;
            double maxSuccessLatencyMs;
            double minErrorLatencyMs;
            double maxErrorLatencyMs;
            
            ApiMethodStats()
                : successCount(0)
                , errorCount(0)
                , totalSuccessLatencyMs(0.0)
                , totalErrorLatencyMs(0.0)
                , minSuccessLatencyMs(std::numeric_limits<double>::max())
                , maxSuccessLatencyMs(std::numeric_limits<double>::lowest())
                , minErrorLatencyMs(std::numeric_limits<double>::max())
                , maxErrorLatencyMs(std::numeric_limits<double>::lowest())
            {}
        };

        /**
         * @brief API latency statistics structure
         * 
         * Tracks aggregated latency metrics for each plugin/API combination.
         * Used for RecordApiLatency() calls from plugins.
         */
        struct ApiLatencyStats
        {
            std::string pluginName;
            std::string apiName;
            uint32_t count;
            double totalLatencyMs;
            double minLatencyMs;
            double maxLatencyMs;
            
            ApiLatencyStats()
                : count(0)
                , totalLatencyMs(0.0)
                , minLatencyMs(std::numeric_limits<double>::max())
                , maxLatencyMs(std::numeric_limits<double>::lowest())
            {}
        };

        /**
         * @brief Service latency statistics structure
         * 
         * Tracks aggregated latency metrics for each plugin/service combination.
         * Used for RecordServiceLatency() calls from plugins.
         */
        struct ServiceLatencyStats
        {
            std::string pluginName;
            std::string serviceName;
            uint32_t count;
            double totalLatencyMs;
            double minLatencyMs;
            double maxLatencyMs;
            
            ServiceLatencyStats()
                : count(0)
                , totalLatencyMs(0.0)
                , minLatencyMs(std::numeric_limits<double>::max())
                , maxLatencyMs(std::numeric_limits<double>::lowest())
            {}
        };

        /**
         * @brief Per-Plugin/Service method statistics structure
         * 
         * Tracks detailed counters and latency metrics for each plugin/service combination.
         * This enables per-service visibility into performance and reliability.
         * Used by AGW_TRACK_SERVICE_CALL macro.
         */
        struct ServiceMethodStats
        {
            std::string pluginName;
            std::string serviceName;
            uint32_t successCount;
            uint32_t errorCount;
            double totalSuccessLatencyMs;
            double totalErrorLatencyMs;
            double minSuccessLatencyMs;
            double maxSuccessLatencyMs;
            double minErrorLatencyMs;
            double maxErrorLatencyMs;
            
            ServiceMethodStats()
                : successCount(0)
                , errorCount(0)
                , totalSuccessLatencyMs(0.0)
                , totalErrorLatencyMs(0.0)
                , minSuccessLatencyMs(std::numeric_limits<double>::max())
                , maxSuccessLatencyMs(std::numeric_limits<double>::lowest())
                , minErrorLatencyMs(std::numeric_limits<double>::max())
                , maxErrorLatencyMs(std::numeric_limits<double>::lowest())
            {}
        };

        // Timer callback for periodic reporting
        class TelemetryTimer : public Core::IDispatch
        {
        public:
            TelemetryTimer(AppGatewayTelemetry* parent) : mParent(parent) {}
            ~TelemetryTimer() override = default;

            void Dispatch() override
            {
                if (mParent) {
                    mParent->OnTimerExpired();
                }
            }

            // Required by TimerType - return 0 to indicate no automatic rescheduling
            uint64_t Timed(const uint64_t scheduledTime)
            {
                Dispatch();
                return 0; // No automatic reschedule, we handle it manually in OnTimerExpired
            }

            // Required by TimerType::Revoke for comparison
            bool operator==(const TelemetryTimer& other) const
            {
                return mParent == other.mParent;
            }

        private:
            AppGatewayTelemetry* mParent;
        };

        // Timer expired callback
        void OnTimerExpired();

        // Send aggregated telemetry to T2
        void SendHealthStats();
        void SendApiErrorStats();
        void SendExternalServiceErrorStats();
        void SendAggregatedMetrics();
        void SendApiMethodStats();
        void SendApiLatencyStats();
        void SendServiceLatencyStats();
        void SendServiceMethodStats();

        // Helper to send telemetry via T2
        void SendT2Event(const char* marker, const std::string& payload);

        // Reset counters after reporting
        void ResetHealthStats();
        void ResetApiErrorStats();
        void ResetExternalServiceErrorStats();
        void ResetApiMethodStats();
        void ResetApiLatencyStats();
        void ResetServiceLatencyStats();
        void ResetServiceMethodStats();

        // Metric recording helpers (called by RecordTelemetryMetric)
        void RecordApiMethodMetric(const std::string& pluginName,
                                  const std::string& methodName,
                                  double latencyMs,
                                  bool isError);
        
        void RecordApiLatencyMetric(const std::string& pluginName,
                                   const std::string& apiName,
                                   double latencyMs);
        
        void RecordServiceLatencyMetric(const std::string& pluginName,
                                       const std::string& serviceName,
                                       double latencyMs);
        
        void RecordServiceMethodMetric(const std::string& pluginName,
                                      const std::string& serviceName,
                                      double latencyMs,
                                      bool isError);
        
        void RecordGenericMetric(const std::string& metricName,
                                double metricValue,
                                const std::string& metricUnit);

        // Helper to parse metric name format: "AppGw<Plugin>_<Method>_<Success|Error>_split"
        bool ParseApiMetricName(const std::string& metricName,
                               std::string& pluginName,
                               std::string& methodName,
                               bool& isError);

        // Helper to parse API latency metric: "AppGw_PluginName_<Plugin>_ApiName_<Api>_ApiLatency_split"
        bool ParseApiLatencyMetricName(const std::string& metricName,
                                      std::string& pluginName,
                                      std::string& apiName);

        // Helper to parse service latency metric: "AppGw_PluginName_<Plugin>_ServiceName_<Service>_ServiceLatency_split"
        bool ParseServiceLatencyMetricName(const std::string& metricName,
                                          std::string& pluginName,
                                          std::string& serviceName);

        // Helper to parse service method metric: "AppGw_PluginName_<Plugin>_ServiceName_<Service>_<Success|Error>_split"
        bool ParseServiceMetricName(const std::string& metricName,
                                   std::string& pluginName,
                                   std::string& serviceName,
                                   bool& isError);

        /**
         * @brief Format JSON payload based on current format setting
         * 
         * This single helper converts a JsonObject to the appropriate output format:
         * - JSON format: Returns the JSON string as-is (self-describing)
         * - COMPACT format: Extracts values only, comma-separated (ignores keys)
         * 
         * For nested arrays, each item is wrapped in parentheses for clear grouping.
         * 
         * Example:
         *   JSON:    {"websocket_connections":12,"total_calls":1543,"failed_calls":23}
         *   COMPACT: 12,1543,23
         * 
         *   JSON:    {"interval":3600,"failures":[{"api":"GetData","count":5},{"api":"SetConfig","count":2}]}
         *   COMPACT: 3600,(GetData,5),(SetConfig,2)
         * 
         * @param jsonPayload The JsonObject containing telemetry data
         * @return Formatted string based on current mTelemetryFormat setting
         */
        std::string FormatTelemetryPayload(const JsonObject& jsonPayload);

    private:
        mutable Core::CriticalSection mAdminLock;
        PluginHost::IShell* mService;

        // Configuration
        uint32_t mReportingIntervalSec;
        uint32_t mCacheThreshold;
        TelemetryFormat mTelemetryFormat;  // Output format (JSON or COMPACT)

        // Timer for periodic reporting
        Core::ProxyType<TelemetryTimer> mTimer;
        Core::TimerType<TelemetryTimer> mTimerHandler;
        // Per-Plugin/API latency statistics: map<"PluginName_ApiName", ApiLatencyStats>
        std::map<std::string, ApiLatencyStats> mApiLatencyStats;

        // Per-Plugin/Service latency statistics: map<"PluginName_ServiceName", ServiceLatencyStats>
        std::map<std::string, ServiceLatencyStats> mServiceLatencyStats;

        // Timer running state
        bool mTimerRunning;

        // Health statistics
        HealthStats mHealthStats;

        // API error counts: map<apiName, count>
        std::map<std::string, uint32_t> mApiErrorCounts;

        // External service error counts: map<serviceName, count>
        std::map<std::string, uint32_t> mExternalServiceErrorCounts;

        // Per-Plugin/API method statistics: map<"PluginName_MethodName", ApiMethodStats>
        std::map<std::string, ApiMethodStats> mApiMethodStats;

        // Per-Plugin/Service method statistics: map<"PluginName_ServiceName", ServiceMethodStats>
        std::map<std::string, ServiceMethodStats> mServiceMethodStats;

        // Cached metrics: map<metricName, MetricData>
        std::map<std::string, MetricData> mMetricsCache;

        // Cached events count (for threshold checking)
        uint32_t mCachedEventCount;

        // Reporting start time (for interval calculation)
        std::chrono::steady_clock::time_point mReportingStartTime;

        // Bootstrap tracking (cumulative across all plugins)
        std::atomic<uint32_t> mBootstrapPluginsLoaded{0};
        std::atomic<uint64_t> mTotalBootstrapTimeMs{0};

        // Initialization state
        bool mInitialized;
    };

} // namespace Plugin
} // namespace WPEFramework
