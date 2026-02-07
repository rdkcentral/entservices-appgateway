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

#include "AppGatewayTelemetry.h"
#include "UtilsLogging.h"
#include "UtilsTelemetry.h"
#include <plugins/json/JsonData_Container.h>
#include <limits>
#include <sstream>
#include <iomanip>

namespace WPEFramework {
namespace Plugin {

    AppGatewayTelemetry& AppGatewayTelemetry::getInstance()
    {
        static AppGatewayTelemetry instance;
        return instance;
    }

    AppGatewayTelemetry::AppGatewayTelemetry()
        : mService(nullptr)
        , mReportingIntervalSec(TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC)
        , mCacheThreshold(TELEMETRY_DEFAULT_CACHE_THRESHOLD)
        , mTelemetryFormat(TelemetryFormat::JSON)  // Default to JSON format
        , mTimer(Core::ProxyType<TelemetryTimer>::Create(this))
        , mTimerHandler(1024 * 64, _T("AppGwTelemetryTimer"))
        , mTimerRunning(false)
        , mCachedEventCount(0)
        , mInitialized(false)
    {
        LOGINFO("AppGatewayTelemetry constructor");
    }

    AppGatewayTelemetry::~AppGatewayTelemetry()
    {
        LOGINFO("AppGatewayTelemetry destructor");
        Deinitialize();
    }

    void AppGatewayTelemetry::Initialize(PluginHost::IShell* service)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);

        if (mInitialized) {
            LOGWARN("AppGatewayTelemetry already initialized");
            return;
        }

        mService = service;
        mReportingStartTime = std::chrono::steady_clock::now();

        // Initialize T2 telemetry
        Utils::Telemetry::init();

        // Start the periodic reporting timer
        if (!mTimerRunning) {
            uint64_t intervalMs = static_cast<uint64_t>(mReportingIntervalSec) * 1000;
            mTimerHandler.Schedule(Core::Time::Now().Add(intervalMs), Core::ProxyType<Core::IDispatch>(mTimer));
            mTimerRunning = true;
            LOGINFO("AppGatewayTelemetry: Started periodic reporting timer with interval %u seconds", mReportingIntervalSec);
        }

        mInitialized = true;
        LOGINFO("AppGatewayTelemetry initialized successfully");
    }

    void AppGatewayTelemetry::Deinitialize()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);

        if (!mInitialized) {
            return;
        }

        // Stop the timer
        if (mTimerRunning) {
            mTimerHandler.Revoke(Core::ProxyType<Core::IDispatch>(mTimer));
            mTimerRunning = false;
        }

        // Flush any remaining telemetry data
        FlushTelemetryData();

        mService = nullptr;
        mInitialized = false;

        LOGINFO("AppGatewayTelemetry deinitialized");
    }

    void AppGatewayTelemetry::SetReportingInterval(uint32_t intervalSec)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        mReportingIntervalSec = intervalSec;
        LOGINFO("AppGatewayTelemetry: Reporting interval set to %u seconds", intervalSec);

        // Restart timer with new interval if running
        if (mTimerRunning) {
            mTimerHandler.Revoke(Core::ProxyType<Core::IDispatch>(mTimer));
            uint64_t intervalMs = static_cast<uint64_t>(mReportingIntervalSec) * 1000;
            mTimerHandler.Schedule(Core::Time::Now().Add(intervalMs), Core::ProxyType<Core::IDispatch>(mTimer));
        }
    }

    void AppGatewayTelemetry::SetCacheThreshold(uint32_t threshold)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        mCacheThreshold = threshold;
        LOGINFO("AppGatewayTelemetry: Cache threshold set to %u", threshold);
    }

    void AppGatewayTelemetry::SetTelemetryFormat(TelemetryFormat format)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        mTelemetryFormat = format;
        LOGINFO("AppGatewayTelemetry: Telemetry format set to %s", 
                format == TelemetryFormat::JSON ? "JSON" : "COMPACT");
    }

    TelemetryFormat AppGatewayTelemetry::GetTelemetryFormat() const
    {
        return mTelemetryFormat;
    }

    void AppGatewayTelemetry::RecordBootstrapTime(uint64_t durationMs, uint32_t pluginsLoaded)
    {
        JsonObject payload;
        payload["duration_ms"] = durationMs;
        payload["plugins_loaded"] = pluginsLoaded;

        std::string payloadStr = FormatTelemetryPayload(payload);
        SendT2Event(AGW_MARKER_BOOTSTRAP_TIME, payloadStr);
        LOGINFO("Bootstrap time recorded: %lu ms, plugins loaded: %u", durationMs, pluginsLoaded);
    }

    void AppGatewayTelemetry::IncrementWebSocketConnections()
    {
        mHealthStats.websocketConnections.fetch_add(1, std::memory_order_relaxed);
    }

    void AppGatewayTelemetry::DecrementWebSocketConnections()
    {
        uint32_t current = mHealthStats.websocketConnections.load(std::memory_order_relaxed);
        if (current > 0) {
            mHealthStats.websocketConnections.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    void AppGatewayTelemetry::IncrementTotalCalls()
    {
        mHealthStats.totalCalls.fetch_add(1, std::memory_order_relaxed);
    }

    void AppGatewayTelemetry::IncrementSuccessfulCalls()
    {
        mHealthStats.successfulCalls.fetch_add(1, std::memory_order_relaxed);
    }

    void AppGatewayTelemetry::IncrementFailedCalls()
    {
        mHealthStats.failedCalls.fetch_add(1, std::memory_order_relaxed);
    }

    void AppGatewayTelemetry::RecordApiError(const std::string& apiName)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        mApiErrorCounts[apiName]++;
        LOGTRACE("API error recorded: %s (count: %u)", apiName.c_str(), mApiErrorCounts[apiName]);
    }

    void AppGatewayTelemetry::RecordExternalServiceErrorInternal(const std::string& serviceName)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        mExternalServiceErrorCounts[serviceName]++;
        LOGTRACE("External service error recorded: %s (count: %u)", 
                 serviceName.c_str(), mExternalServiceErrorCounts[serviceName]);
    }

    // ============================================
    // IAppGatewayTelemetry Interface Implementation
    // (Called by external plugins via COM-RPC)
    // ============================================

    Core::hresult AppGatewayTelemetry::RecordTelemetryEvent(
        const Exchange::IAppGatewayTelemetry::GatewayContext& context,
        const string& eventName,
        const string& eventData)
    {
        if (!mInitialized) {
            LOGERR("AppGatewayTelemetry not initialized");
            return Core::ERROR_UNAVAILABLE;
        }

        LOGTRACE("RecordTelemetryEvent from %s: event=%s, data=%s",
                 context.appId.c_str(), eventName.c_str(), eventData.c_str());

        // The eventName acts as the T2 marker
        // Parse eventName to determine the type of telemetry
        // 
        // Supported event name patterns:
        // - "agw_<PluginName>ApiError" - API errors from other plugins
        // - "agw_<PluginName>ExternalServiceError" - External service errors
        // - Any other event name - Generic telemetry event

        // Check if this is an API error event
        if (eventName.find("ApiError") != std::string::npos) {
            // Extract API name from eventData if possible
            // eventData expected format: {"api": "<apiName>", "error": "<errorDetails>"}
            JsonObject data;
            data.FromString(eventData);
            std::string apiName = data.HasLabel("api") ? data["api"].String() : eventName;
            RecordApiError(apiName);
        }
        // Check if this is an external service error event
        else if (eventName.find("ExternalServiceError") != std::string::npos) {
            // Extract service name from eventData if possible
            // eventData expected format: {"service": "<serviceName>", "error": "<errorDetails>"}
            JsonObject data;
            data.FromString(eventData);
            std::string serviceName = data.HasLabel("service") ? data["service"].String() : eventName;
            RecordExternalServiceErrorInternal(serviceName);
        }

        // Increment cached event count
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
            mCachedEventCount++;

            // Check if we've reached the threshold
            if (mCachedEventCount >= mCacheThreshold) {
                LOGINFO("Cache threshold reached (%u), flushing telemetry data", mCachedEventCount);
                lock.Unlock();
                FlushTelemetryData();
            }
        }

        return Core::ERROR_NONE;
    }

    Core::hresult AppGatewayTelemetry::RecordTelemetryMetric(
        const Exchange::IAppGatewayTelemetry::GatewayContext& context,
        const string& metricName,
        const double metricValue,
        const string& metricUnit)
    {
        if (!mInitialized) {
            LOGERR("AppGatewayTelemetry not initialized");
            return Core::ERROR_UNAVAILABLE;
        }

        LOGTRACE("RecordTelemetryMetric from %s: metric=%s, value=%f, unit=%s",
                 context.appId.c_str(), metricName.c_str(), metricValue, metricUnit.c_str());

        // Aggregate the metric
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
            
            MetricData& data = mMetricsCache[metricName];
            data.sum += metricValue;
            data.count++;
            if (metricValue < data.min) {
                data.min = metricValue;
            }
            if (metricValue > data.max) {
                data.max = metricValue;
            }
            if (data.unit.empty()) {
                data.unit = metricUnit;
            }

            mCachedEventCount++;

            // Check if we've reached the threshold
            if (mCachedEventCount >= mCacheThreshold) {
                LOGINFO("Cache threshold reached (%u), flushing telemetry data", mCachedEventCount);
                lock.Unlock();
                FlushTelemetryData();
            }
        }

        return Core::ERROR_NONE;
    }

    void AppGatewayTelemetry::OnTimerExpired()
    {
        LOGINFO("Telemetry reporting timer expired, flushing data");
        FlushTelemetryData();

        // Reschedule the timer
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        if (mTimerRunning && mInitialized) {
            uint64_t intervalMs = static_cast<uint64_t>(mReportingIntervalSec) * 1000;
            mTimerHandler.Schedule(Core::Time::Now().Add(intervalMs), Core::ProxyType<Core::IDispatch>(mTimer));
        }
    }

    void AppGatewayTelemetry::FlushTelemetryData()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mReportingStartTime).count();

        LOGINFO("Flushing telemetry data (reporting period: %ld seconds)", elapsed);

        // Send all aggregated data
        SendHealthStats();
        SendApiErrorStats();
        SendExternalServiceErrorStats();
        SendAggregatedMetrics();

        // Reset counters and caches
        ResetHealthStats();
        ResetApiErrorStats();
        ResetExternalServiceErrorStats();
        mMetricsCache.clear();
        mCachedEventCount = 0;
        mReportingStartTime = now;
    }

    void AppGatewayTelemetry::SendHealthStats()
    {
        uint32_t wsConnections = mHealthStats.websocketConnections.load(std::memory_order_relaxed);
        uint32_t totalCalls = mHealthStats.totalCalls.load(std::memory_order_relaxed);
        uint32_t successfulCalls = mHealthStats.successfulCalls.load(std::memory_order_relaxed);
        uint32_t failedCalls = mHealthStats.failedCalls.load(std::memory_order_relaxed);

        // Only send if there's data
        if (totalCalls == 0 && wsConnections == 0) {
            LOGTRACE("No health stats to report");
            return;
        }

        JsonObject payload;
        payload["websocket_connections"] = wsConnections;
        payload["total_calls"] = totalCalls;
        payload["successful_calls"] = successfulCalls;
        payload["failed_calls"] = failedCalls;
        payload["reporting_interval_sec"] = mReportingIntervalSec;

        std::string payloadStr = FormatTelemetryPayload(payload);
        SendT2Event(AGW_MARKER_HEALTH_STATS, payloadStr);
        LOGINFO("Health stats sent: ws=%u, total=%u, success=%u, failed=%u",
                wsConnections, totalCalls, successfulCalls, failedCalls);
    }

    void AppGatewayTelemetry::SendApiErrorStats()
    {
        if (mApiErrorCounts.empty()) {
            LOGTRACE("No API error stats to report");
            return;
        }

        JsonObject payload;
        payload["reporting_interval_sec"] = mReportingIntervalSec;

        JsonArray failures;
        for (const auto& [api, count] : mApiErrorCounts) {
            JsonObject entry;
            entry["api"] = api;
            entry["count"] = count;
            failures.Add(entry);
        }
        payload["api_failures"] = failures;

        std::string payloadStr = FormatTelemetryPayload(payload);
        SendT2Event(AGW_MARKER_API_ERROR_STATS, payloadStr);
        LOGINFO("API error stats sent: %zu APIs with errors", mApiErrorCounts.size());
    }

    void AppGatewayTelemetry::SendExternalServiceErrorStats()
    {
        if (mExternalServiceErrorCounts.empty()) {
            LOGTRACE("No external service error stats to report");
            return;
        }

        JsonObject payload;
        payload["reporting_interval_sec"] = mReportingIntervalSec;

        JsonArray failures;
        for (const auto& [service, count] : mExternalServiceErrorCounts) {
            JsonObject entry;
            entry["service"] = service;
            entry["count"] = count;
            failures.Add(entry);
        }
        payload["service_failures"] = failures;

        std::string payloadStr = FormatTelemetryPayload(payload);
        SendT2Event(AGW_MARKER_EXT_SERVICE_ERROR_STATS, payloadStr);
        LOGINFO("External service error stats sent: %zu services with errors", 
                mExternalServiceErrorCounts.size());
    }

    void AppGatewayTelemetry::SendAggregatedMetrics()
    {
        if (mMetricsCache.empty()) {
            LOGTRACE("No aggregated metrics to report");
            return;
        }

        // Send each metric with its own marker (the metric name)
        for (const auto& [metricName, data] : mMetricsCache) {
            if (data.count == 0) {
                continue;
            }

            double minVal = (data.min == std::numeric_limits<double>::max()) ? 0.0 : data.min;
            double maxVal = (data.max == std::numeric_limits<double>::lowest()) ? 0.0 : data.max;
            double avgVal = data.sum / static_cast<double>(data.count);

            JsonObject payload;
            payload["sum"] = data.sum;
            payload["min"] = minVal;
            payload["max"] = maxVal;
            payload["count"] = data.count;
            payload["avg"] = avgVal;
            payload["unit"] = data.unit;
            payload["reporting_interval_sec"] = mReportingIntervalSec;

            std::string payloadStr = FormatTelemetryPayload(payload);

            // Use the metric name as the T2 marker
            SendT2Event(metricName.c_str(), payloadStr);
            LOGINFO("Aggregated metric sent: %s (count=%u, avg=%.2f %s)",
                    metricName.c_str(), data.count, avgVal, data.unit.c_str());
        }
    }

    void AppGatewayTelemetry::SendT2Event(const char* marker, const std::string& payload)
    {
        // Use the Utils::Telemetry helper which wraps T2 calls
        // sendMessage takes non-const char* so we need to make copies
        char* markerCopy = strdup(marker);
        char* payloadCopy = strdup(payload.c_str());

        if (markerCopy && payloadCopy) {
            Utils::Telemetry::sendMessage(markerCopy, payloadCopy);
            free(payloadCopy);
        }

        if (markerCopy) {
            free(markerCopy);
        }
    }

    void AppGatewayTelemetry::ResetHealthStats()
    {
        // Note: We don't reset websocketConnections as it represents current state
        mHealthStats.totalCalls.store(0, std::memory_order_relaxed);
        mHealthStats.successfulCalls.store(0, std::memory_order_relaxed);
        mHealthStats.failedCalls.store(0, std::memory_order_relaxed);
    }

    void AppGatewayTelemetry::ResetApiErrorStats()
    {
        mApiErrorCounts.clear();
    }

    void AppGatewayTelemetry::ResetExternalServiceErrorStats()
    {
        mExternalServiceErrorCounts.clear();
    }

    std::string AppGatewayTelemetry::FormatTelemetryPayload(const JsonObject& jsonPayload)
    {
        if (mTelemetryFormat == TelemetryFormat::JSON) {
            // JSON format: Return as-is
            std::string payloadStr;
            jsonPayload.ToString(payloadStr);
            return payloadStr;
        }

        // COMPACT format: Extract values only, comma-separated
        // For arrays containing objects, flatten to key:value pairs
        std::ostringstream oss;
        bool first = true;

        auto it = jsonPayload.Variants();
        while (it.Next()) {
            const auto& value = it.Current();
            
            if (!first) {
                oss << ",";
            }
            first = false;

            // Check if this is an array (for nested structures like api_failures)
            if (value.Content() == Core::JSON::Variant::type::ARRAY) {
                // Handle array of objects - wrap each item in parentheses
                // e.g., (GetData,5),(SetConfig,2),(LoadResource,1)
                JsonArray arr(value.Array());
                auto arrIt = arr.Elements();
                bool firstArrItem = true;
                
                while (arrIt.Next()) {
                    if (!firstArrItem) {
                        oss << ",";
                    }
                    firstArrItem = false;
                    
                    // Each array element is an object like {"api":"GetData","count":5}
                    // or {"service":"AuthService","count":3}
                    // Wrap in parentheses with comma-separated values inside
                    oss << "(";
                    JsonObject obj(arrIt.Current());
                    
                    auto objIt = obj.Variants();
                    bool firstField = true;
                    while (objIt.Next()) {
                        const auto& field = objIt.Current();
                        if (!firstField) {
                            oss << ",";
                        }
                        firstField = false;
                        
                        if (field.Content() == Core::JSON::Variant::type::STRING) {
                            oss << field.String();
                        } else if (field.Content() == Core::JSON::Variant::type::NUMBER) {
                            double num = field.Number();
                            if (num == static_cast<int64_t>(num)) {
                                oss << static_cast<int64_t>(num);
                            } else {
                                oss << std::fixed << std::setprecision(2) << num;
                            }
                        }
                    }
                    oss << ")";
                }
            } else if (value.Content() == Core::JSON::Variant::type::STRING) {
                oss << value.String();
            } else if (value.Content() == Core::JSON::Variant::type::NUMBER) {
                // Check if it's a floating point or integer
                double num = value.Number();
                if (num == static_cast<int64_t>(num)) {
                    oss << static_cast<int64_t>(num);
                } else {
                    oss << std::fixed << std::setprecision(2) << num;
                }
            } else if (value.Content() == Core::JSON::Variant::type::BOOLEAN) {
                oss << (value.Boolean() ? "true" : "false");
            }
        }

        return oss.str();
    }

} // namespace Plugin
} // namespace WPEFramework
