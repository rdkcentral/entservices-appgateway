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
#include <limits>
#include <sstream>
#include <iomanip>

namespace WPEFramework {
namespace Plugin {

    AppGatewayTelemetry& AppGatewayTelemetry::getInstance()
    {
        static Core::ProxyType<AppGatewayTelemetry> instance = Core::ProxyType<AppGatewayTelemetry>::Create();
        ASSERT(instance.IsValid());
        return *instance;
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
            mTimerHandler.Schedule(Core::Time::Now().Add(intervalMs), *mTimer);
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
            mTimerHandler.Revoke(*mTimer);
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
            mTimerHandler.Revoke(*mTimer);
            uint64_t intervalMs = static_cast<uint64_t>(mReportingIntervalSec) * 1000;
            mTimerHandler.Schedule(Core::Time::Now().Add(intervalMs), *mTimer);
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

    void AppGatewayTelemetry::RecordBootstrapTime(uint64_t durationMs)
    {
        // Increment plugin counter and accumulate total bootstrap time
        uint32_t pluginCount = mBootstrapPluginsLoaded.fetch_add(1, std::memory_order_relaxed) + 1;
        uint64_t totalTime = mTotalBootstrapTimeMs.fetch_add(durationMs, std::memory_order_relaxed) + durationMs;
        
        LOGINFO("Plugin bootstrap time recorded: %lu ms (Plugin #%u, Cumulative total: %lu ms)",
                durationMs, pluginCount, totalTime);
        
        RecordGenericMetric(AGW_MARKER_BOOTSTRAP_DURATION,
                            static_cast<double>(totalTime), AGW_UNIT_MILLISECONDS);
        RecordGenericMetric(AGW_MARKER_BOOTSTRAP_PLUGIN_COUNT,
                            static_cast<double>(pluginCount), AGW_UNIT_COUNT);
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

    // IAppGatewayTelemetry Interface Implementation
    // (Called by external plugins via COM-RPC)

    Core::hresult AppGatewayTelemetry::RecordTelemetryEvent(
        const Exchange::GatewayContext& context,
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
        // - "AppGwPluginApiError_split" - API errors from other plugins (sent immediately)
        // - "AppGwPluginExtServiceError_split" - External service errors (sent immediately)
        // - Any other event name - Generic telemetry event (cached and flushed periodically)

        bool isImmediateEvent = false;

        // Check if this is an API error event - send immediately to T2
        if (eventName == AGW_MARKER_PLUGIN_API_ERROR) {
            // Extract API name from eventData if possible
            // eventData expected format: {"plugin": "<pluginName>", "api": "<apiName>", "error": "<errorDetails>"}
            JsonObject data;
            data.FromString(eventData);
            std::string apiName = data.HasLabel("api") ? data["api"].String() : eventName;
            
            // Track error count for aggregated metrics (sent periodically)
            RecordApiError(apiName);
            
            // Send error event immediately to T2 for forensics
            SendT2Event(eventName.c_str(), eventData);
            LOGINFO("Sent immediate API error event to T2: api=%s", apiName.c_str());
            
            isImmediateEvent = true;
        }
        // Check if this is an external service error event - send immediately to T2
        else if (eventName == AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR) {
            // Extract service name from eventData if possible
            // eventData expected format: {"plugin": "<pluginName>", "service": "<serviceName>", "error": "<errorDetails>"}
            JsonObject data;
            data.FromString(eventData);
            std::string serviceName = data.HasLabel("service") ? data["service"].String() : eventName;
            
            // Track error count for aggregated metrics (sent periodically)
            RecordExternalServiceErrorInternal(serviceName);
            
            // Send error event immediately to T2 for forensics
            SendT2Event(eventName.c_str(), eventData);
            LOGINFO("Sent immediate external service error event to T2: service=%s", serviceName.c_str());
            
            isImmediateEvent = true;
        }

        // For non-immediate events, cache and check threshold
        if (!isImmediateEvent) {
            bool shouldFlush = false;
            {
                Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
                mCachedEventCount++;

                // Check if we've reached the threshold
                if (mCachedEventCount >= mCacheThreshold) {
                    shouldFlush = true;
                    LOGINFO("Cache threshold reached (%u), flushing telemetry data", mCachedEventCount);
                }
            }

            if (shouldFlush) {
                FlushTelemetryData();
            }
        }

        return Core::ERROR_NONE;
    }

    bool AppGatewayTelemetry::ParseApiMetricName(const std::string& metricName,
                                                 std::string& pluginName,
                                                 std::string& methodName,
                                                 bool& isError)
    {
        // SPECIFIC format for API method metrics with explicit tags:
        // "AppGw_PluginName_<Plugin>_MethodName_<Method>_Success_split"
        // "AppGw_PluginName_<Plugin>_MethodName_<Method>_Error_split"
        //
        // Examples:
        //   "AppGw_PluginName_LaunchDelegate_MethodName_session_Success_split"
        //   "AppGw_PluginName_Badger_MethodName_setValue_Error_split"
        //
        // Other metrics like "AppGwBootstrapDuration_split" or "AppGXYS_abc_def_split" 
        // will NOT match because they lack the explicit "PluginName_" and "MethodName_" tags
        
        const std::string successSuffix = "_Success_split";
        const std::string errorSuffix = "_Error_split";
        const std::string prefix = "AppGw_PluginName_";
        const std::string methodTag = "_MethodName_";
        
        // Check if it ends with "_Success_split" or "_Error_split"
        bool hasSuccessSuffix = false;
        bool hasErrorSuffix = false;
        
        if (metricName.length() > successSuffix.length() && 
            metricName.substr(metricName.length() - successSuffix.length()) == successSuffix) {
            isError = false;
            hasSuccessSuffix = true;
        } else if (metricName.length() > errorSuffix.length() && 
                   metricName.substr(metricName.length() - errorSuffix.length()) == errorSuffix) {
            isError = true;
            hasErrorSuffix = true;
        } else {
            return false;
        }
        
        // Check if it starts with the explicit prefix "AppGw_PluginName_"
        if (metricName.length() <= prefix.length() || 
            metricName.substr(0, prefix.length()) != prefix) {
            return false;
        }
        
        // Remove prefix and suffix to get "Plugin_MethodName_Method_Success"
        size_t suffixLen = hasSuccessSuffix ? successSuffix.length() : errorSuffix.length();
        std::string middle = metricName.substr(prefix.length(), 
                                              metricName.length() - prefix.length() - suffixLen);
        
        // Find "_MethodName_" tag
        size_t methodTagPos = middle.find(methodTag);
        if (methodTagPos == std::string::npos || methodTagPos == 0) {
            return false;
        }
        
        // Extract plugin name (everything before "_MethodName_")
        pluginName = middle.substr(0, methodTagPos);
        
        // Extract method name (everything after "_MethodName_")
        methodName = middle.substr(methodTagPos + methodTag.length());
        
        // Validate that both names are non-empty
        if (pluginName.empty() || methodName.empty()) {
            return false;
        }
        
        return true;
    }

    bool AppGatewayTelemetry::ParseApiLatencyMetricName(const std::string& metricName,
                                                        std::string& pluginName,
                                                        std::string& apiName)
    {
        // SPECIFIC format for API latency metrics with explicit tags:
        // "AppGw_PluginName_<Plugin>_ApiName_<Api>_ApiLatency_split"
        //
        // Examples:
        //   "AppGw_PluginName_Badger_ApiName_GetSettings_ApiLatency_split"
        //   "AppGw_PluginName_OttServices_ApiName_GetToken_ApiLatency_split"
        
        const std::string suffix = "_ApiLatency_split";
        const std::string prefix = "AppGw_PluginName_";
        const std::string apiTag = "_ApiName_";
        
        // Check if it ends with "_ApiLatency_split"
        if (metricName.length() <= suffix.length() || 
            metricName.substr(metricName.length() - suffix.length()) != suffix) {
            return false;
        }
        
        // Check if it starts with "AppGw_PluginName_"
        if (metricName.length() <= prefix.length() || 
            metricName.substr(0, prefix.length()) != prefix) {
            return false;
        }
        
        // Remove prefix and suffix
        std::string middle = metricName.substr(prefix.length(), 
                                              metricName.length() - prefix.length() - suffix.length());
        
        // Find "_ApiName_" tag
        size_t apiTagPos = middle.find(apiTag);
        if (apiTagPos == std::string::npos || apiTagPos == 0) {
            return false;
        }
        
        // Extract plugin name (everything before "_ApiName_")
        pluginName = middle.substr(0, apiTagPos);
        
        // Extract API name (everything after "_ApiName_")
        apiName = middle.substr(apiTagPos + apiTag.length());
        
        // Validate that both names are non-empty
        if (pluginName.empty() || apiName.empty()) {
            return false;
        }
        
        return true;
    }

    bool AppGatewayTelemetry::ParseServiceLatencyMetricName(const std::string& metricName,
                                                            std::string& pluginName,
                                                            std::string& serviceName)
    {
        // SPECIFIC format for service latency metrics with explicit tags:
        // "AppGw_PluginName_<Plugin>_ServiceName_<Service>_ServiceLatency_split"
        //
        // Examples:
        //   "AppGw_PluginName_OttServices_ServiceName_ThorPermissionService_ServiceLatency_split"
        //   "AppGw_PluginName_Badger_ServiceName_AuthService_ServiceLatency_split"
        
        const std::string suffix = "_ServiceLatency_split";
        const std::string prefix = "AppGw_PluginName_";
        const std::string serviceTag = "_ServiceName_";
        
        // Check if it ends with "_ServiceLatency_split"
        if (metricName.length() <= suffix.length() || 
            metricName.substr(metricName.length() - suffix.length()) != suffix) {
            return false;
        }
        
        // Check if it starts with "AppGw_PluginName_"
        if (metricName.length() <= prefix.length() || 
            metricName.substr(0, prefix.length()) != prefix) {
            return false;
        }
        
        // Remove prefix and suffix
        std::string middle = metricName.substr(prefix.length(), 
                                              metricName.length() - prefix.length() - suffix.length());
        
        // Find "_ServiceName_" tag
        size_t serviceTagPos = middle.find(serviceTag);
        if (serviceTagPos == std::string::npos || serviceTagPos == 0) {
            return false;
        }
        
        // Extract plugin name (everything before "_ServiceName_")
        pluginName = middle.substr(0, serviceTagPos);
        
        // Extract service name (everything after "_ServiceName_")
        serviceName = middle.substr(serviceTagPos + serviceTag.length());
        
        // Validate that both names are non-empty
        if (pluginName.empty() || serviceName.empty()) {
            return false;
        }
        
        return true;
    }

    bool AppGatewayTelemetry::ParseServiceMetricName(const std::string& metricName,
                                                     std::string& pluginName,
                                                     std::string& serviceName,
                                                     bool& isError)
    {
        // SPECIFIC format for service method metrics with explicit tags:
        // "AppGw_PluginName_<Plugin>_ServiceName_<Service>_Success_split"
        // "AppGw_PluginName_<Plugin>_ServiceName_<Service>_Error_split"
        //
        // Examples:
        //   "AppGw_PluginName_OttServices_ServiceName_ThorPermissionService_Success_split"
        //   "AppGw_PluginName_Badger_ServiceName_AuthService_Error_split"
        //
        // Other metrics like "AppGwBootstrapDuration_split" or service latency metrics
        // will NOT match because they lack the Success/Error suffix or use different patterns
        
        const std::string successSuffix = "_Success_split";
        const std::string errorSuffix = "_Error_split";
        const std::string prefix = "AppGw_PluginName_";
        const std::string serviceTag = "_ServiceName_";
        
        // Check if it ends with "_Success_split" or "_Error_split"
        bool hasSuccessSuffix = false;
        bool hasErrorSuffix = false;
        
        if (metricName.length() > successSuffix.length() && 
            metricName.substr(metricName.length() - successSuffix.length()) == successSuffix) {
            isError = false;
            hasSuccessSuffix = true;
        } else if (metricName.length() > errorSuffix.length() && 
                   metricName.substr(metricName.length() - errorSuffix.length()) == errorSuffix) {
            isError = true;
            hasErrorSuffix = true;
        } else {
            return false;
        }
        
        // Check if it starts with the explicit prefix "AppGw_PluginName_"
        if (metricName.length() <= prefix.length() || 
            metricName.substr(0, prefix.length()) != prefix) {
            return false;
        }
        
        // Remove prefix and suffix to get "Plugin_ServiceName_Service_Success"
        size_t suffixLen = hasSuccessSuffix ? successSuffix.length() : errorSuffix.length();
        std::string middle = metricName.substr(prefix.length(), 
                                              metricName.length() - prefix.length() - suffixLen);
        
        // Find "_ServiceName_" tag
        size_t serviceTagPos = middle.find(serviceTag);
        if (serviceTagPos == std::string::npos || serviceTagPos == 0) {
            return false;
        }
        
        // Extract plugin name (everything before "_ServiceName_")
        pluginName = middle.substr(0, serviceTagPos);
        
        // Extract service name (everything after "_ServiceName_")
        serviceName = middle.substr(serviceTagPos + serviceTag.length());
        
        // Validate that both names are non-empty
        if (pluginName.empty() || serviceName.empty()) {
            return false;
        }
        
        return true;
    }

    void AppGatewayTelemetry::RecordApiMethodMetric(
        const std::string& pluginName,
        const std::string& methodName,
        double latencyMs,
        bool isError)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        
        std::string apiKey = pluginName + "_" + methodName;
        ApiMethodStats& stats = mApiMethodStats[apiKey];
        
        // Initialize plugin and method names on first use
        if (stats.pluginName.empty()) {
            stats.pluginName = pluginName;
            stats.methodName = methodName;
        }
        
        // Track counters (plugin-specific stats only, do NOT affect AppGateway's health stats)
        
        if (isError) {
            // Error case
            stats.errorCount++;
            stats.totalErrorLatencyMs += latencyMs;
            
            if (latencyMs < stats.minErrorLatencyMs) {
                stats.minErrorLatencyMs = latencyMs;
            }
            if (latencyMs > stats.maxErrorLatencyMs) {
                stats.maxErrorLatencyMs = latencyMs;
            }
            
            LOGTRACE("API error tracked: %s::%s (error_count=%u, latency=%.2f ms)",
                     pluginName.c_str(), methodName.c_str(), stats.errorCount, latencyMs);
        } else {
            // Success case
            stats.successCount++;
            stats.totalSuccessLatencyMs += latencyMs;
            
            if (latencyMs < stats.minSuccessLatencyMs) {
                stats.minSuccessLatencyMs = latencyMs;
            }
            if (latencyMs > stats.maxSuccessLatencyMs) {
                stats.maxSuccessLatencyMs = latencyMs;
            }
            
            LOGTRACE("API success tracked: %s::%s (success_count=%u, latency=%.2f ms)",
                     pluginName.c_str(), methodName.c_str(), stats.successCount, latencyMs);
        }
        
        mCachedEventCount++;
    }

    void AppGatewayTelemetry::RecordApiLatencyMetric(
        const std::string& pluginName,
        const std::string& apiName,
        double latencyMs)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        
        std::string latencyKey = pluginName + "_" + apiName;
        ApiLatencyStats& stats = mApiLatencyStats[latencyKey];
        
        // Initialize plugin and API names on first use
        if (stats.pluginName.empty()) {
            stats.pluginName = pluginName;
            stats.apiName = apiName;
        }
        
        // Track latency
        stats.count++;
        stats.totalLatencyMs += latencyMs;
        
        if (latencyMs < stats.minLatencyMs) {
            stats.minLatencyMs = latencyMs;
        }
        if (latencyMs > stats.maxLatencyMs) {
            stats.maxLatencyMs = latencyMs;
        }
        
        LOGTRACE("API latency tracked: %s::%s (count=%u, latency=%.2f ms)",
                 pluginName.c_str(), apiName.c_str(), stats.count, latencyMs);
        
        mCachedEventCount++;
    }

    void AppGatewayTelemetry::RecordServiceLatencyMetric(
        const std::string& pluginName,
        const std::string& serviceName,
        double latencyMs)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        
        std::string latencyKey = pluginName + "_" + serviceName;
        ServiceLatencyStats& stats = mServiceLatencyStats[latencyKey];
        
        // Initialize plugin and service names on first use
        if (stats.pluginName.empty()) {
            stats.pluginName = pluginName;
            stats.serviceName = serviceName;
        }
        
        // Track latency
        stats.count++;
        stats.totalLatencyMs += latencyMs;
        
        if (latencyMs < stats.minLatencyMs) {
            stats.minLatencyMs = latencyMs;
        }
        if (latencyMs > stats.maxLatencyMs) {
            stats.maxLatencyMs = latencyMs;
        }
        
        LOGTRACE("Service latency tracked: %s::%s (count=%u, latency=%.2f ms)",
                 pluginName.c_str(), serviceName.c_str(), stats.count, latencyMs);
        
        mCachedEventCount++;
    }

    void AppGatewayTelemetry::RecordServiceMethodMetric(
        const std::string& pluginName,
        const std::string& serviceName,
        double latencyMs,
        bool isError)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        
        std::string serviceKey = pluginName + "_" + serviceName;
        ServiceMethodStats& stats = mServiceMethodStats[serviceKey];
        
        // Initialize plugin and service names on first use
        if (stats.pluginName.empty()) {
            stats.pluginName = pluginName;
            stats.serviceName = serviceName;
        }
        
        // Track counters (plugin-specific stats only)
        
        if (isError) {
            // Error case
            stats.errorCount++;
            stats.totalErrorLatencyMs += latencyMs;
            
            if (latencyMs < stats.minErrorLatencyMs) {
                stats.minErrorLatencyMs = latencyMs;
            }
            if (latencyMs > stats.maxErrorLatencyMs) {
                stats.maxErrorLatencyMs = latencyMs;
            }
            
            LOGTRACE("Service error tracked: %s::%s (error_count=%u, latency=%.2f ms)",
                     pluginName.c_str(), serviceName.c_str(), stats.errorCount, latencyMs);
        } else {
            // Success case
            stats.successCount++;
            stats.totalSuccessLatencyMs += latencyMs;
            
            if (latencyMs < stats.minSuccessLatencyMs) {
                stats.minSuccessLatencyMs = latencyMs;
            }
            if (latencyMs > stats.maxSuccessLatencyMs) {
                stats.maxSuccessLatencyMs = latencyMs;
            }
            
            LOGTRACE("Service success tracked: %s::%s (success_count=%u, latency=%.2f ms)",
                     pluginName.c_str(), serviceName.c_str(), stats.successCount, latencyMs);
        }
        
        mCachedEventCount++;
    }

    void AppGatewayTelemetry::RecordGenericMetric(
        const std::string& metricName,
        double metricValue,
        const std::string& metricUnit)
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
    }

    Core::hresult AppGatewayTelemetry::RecordTelemetryMetric(
        const Exchange::GatewayContext& context,
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

        // Check for bootstrap duration metric - route to internal RecordBootstrapTime
        if (metricName == AGW_MARKER_BOOTSTRAP_DURATION) {
            RecordBootstrapTime(static_cast<uint64_t>(metricValue));
            return Core::ERROR_NONE;
        }

        // Determine metric type and record accordingly
        std::string pluginName, apiOrMethodName;
        bool isError = false;
        
        if (ParseApiMetricName(metricName, pluginName, apiOrMethodName, isError)) {
            // API method metric (success/error with latency tracking)
            RecordApiMethodMetric(pluginName, apiOrMethodName, metricValue, isError);
        } else if (ParseServiceMetricName(metricName, pluginName, apiOrMethodName, isError)) {
            // Service method metric (success/error with latency tracking from AGW_TRACK_SERVICE_CALL)
            RecordServiceMethodMetric(pluginName, apiOrMethodName, metricValue, isError);
        } else if (ParseApiLatencyMetricName(metricName, pluginName, apiOrMethodName)) {
            // API latency metric (deprecated, but still supported)
            RecordApiLatencyMetric(pluginName, apiOrMethodName, metricValue);
        } else if (ParseServiceLatencyMetricName(metricName, pluginName, apiOrMethodName)) {
            // External service latency metric
            RecordServiceLatencyMetric(pluginName, apiOrMethodName, metricValue);
        } else {
            // Generic metric aggregation (bootstrap time, etc.)
            RecordGenericMetric(metricName, metricValue, metricUnit);
        }

        // Check if cache threshold reached and flush if needed
        if (mCachedEventCount >= mCacheThreshold) {
            FlushTelemetryData();
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
            mTimerHandler.Schedule(Core::Time::Now().Add(intervalMs), *mTimer);
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
        SendApiMethodStats();
        SendApiLatencyStats();
        SendServiceLatencyStats();
        SendServiceMethodStats();
        SendApiErrorStats();
        SendExternalServiceErrorStats();
        SendAggregatedMetrics();

        // Reset counters and caches
        ResetHealthStats();
        ResetApiMethodStats();
        ResetApiLatencyStats();
        ResetServiceLatencyStats();
        ResetServiceMethodStats();
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

        // Send all health stats in a single consolidated payload to T2
        JsonObject healthPayload;
        healthPayload["reporting_interval_sec"] = mReportingIntervalSec;
        healthPayload["websocket_connections"] = wsConnections;
        healthPayload["total_calls"] = totalCalls;
        healthPayload["successful_calls"] = successfulCalls;
        healthPayload["failed_calls"] = failedCalls;        
        healthPayload["unit"] = AGW_UNIT_COUNT;
        
        std::string payload = FormatTelemetryPayload(healthPayload);
        SendT2Event(AGW_MARKER_HEALTH_STATS, payload);

        LOGINFO("Health stats sent as consolidated metric: ws=%u, total=%u, success=%u, failed=%u",
                wsConnections, totalCalls, successfulCalls, failedCalls);
    }

    void AppGatewayTelemetry::SendApiErrorStats()
    {
        if (mApiErrorCounts.empty()) {
            LOGTRACE("No API error stats to report");
            return;
        }

        // Send each API error count as a separate metric for proper aggregation
        for (const auto& item : mApiErrorCounts) {
            std::string metricName = std::string(AGW_METRIC_API_ERROR_COUNT_PREFIX) + item.first + AGW_METRIC_SUFFIX;
            
            JsonObject metricPayload;
            metricPayload["reporting_interval_sec"] = mReportingIntervalSec;
            metricPayload["sum"] = item.second;
            metricPayload["count"] = 1;
            metricPayload["unit"] = AGW_UNIT_COUNT;
            
            std::string payloadStr = FormatTelemetryPayload(metricPayload);
            SendT2Event(metricName.c_str(), payloadStr);
            
            LOGINFO("API error metric sent: %s = %u", metricName.c_str(), item.second);
        }
        
        LOGINFO("API error stats sent as metrics: %zu APIs with errors", mApiErrorCounts.size());
    }

    void AppGatewayTelemetry::SendExternalServiceErrorStats()
    {
        if (mExternalServiceErrorCounts.empty()) {
            LOGTRACE("No external service error stats to report");
            return;
        }

        // Send each external service error count as a separate metric for proper aggregation
        for (const auto& item : mExternalServiceErrorCounts) {
            std::string metricName = std::string(AGW_METRIC_EXT_SERVICE_ERROR_COUNT_PREFIX) + item.first + AGW_METRIC_SUFFIX;
            
            JsonObject metricPayload;
            metricPayload["reporting_interval_sec"] = mReportingIntervalSec;
            metricPayload["sum"] = item.second;
            metricPayload["count"] = 1;
            metricPayload["unit"] = AGW_UNIT_COUNT;
            
            std::string payloadStr = FormatTelemetryPayload(metricPayload);
            SendT2Event(metricName.c_str(), payloadStr);
            
            LOGINFO("External service error metric sent: %s = %u", metricName.c_str(), item.second);
        }
        
        LOGINFO("External service error stats sent as metrics: %zu services with errors", 
                mExternalServiceErrorCounts.size());
    }

    void AppGatewayTelemetry::SendAggregatedMetrics()
    {
        if (mMetricsCache.empty()) {
            LOGTRACE("No aggregated metrics to report");
            return;
        }

        // Send each metric with its own marker (the metric name)
        for (const auto& item : mMetricsCache) {
            const std::string& metricName = item.first;
            const MetricData& data = item.second;
            
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

    void AppGatewayTelemetry::SendApiMethodStats()
    {
        if (mApiMethodStats.empty()) {
            LOGTRACE("No API method stats to report");
            return;
        }

        // Send each plugin/method combination as a separate T2 marker
        for (const auto& item : mApiMethodStats) {
            const std::string& apiKey = item.first;
            const ApiMethodStats& stats = item.second;
            
            if (stats.successCount == 0 && stats.errorCount == 0) {
                continue;
            }

            // Build detailed payload with plugin name, method name, counters, and latency stats
            JsonObject payload;
            payload["plugin_name"] = stats.pluginName;
            payload["method_name"] = stats.methodName;
            payload["reporting_interval_sec"] = mReportingIntervalSec;
            
            // Success metrics
            if (stats.successCount > 0) {
                double avgSuccessLatency = stats.totalSuccessLatencyMs / stats.successCount;
                double minSuccess = (stats.minSuccessLatencyMs == std::numeric_limits<double>::max()) 
                                    ? 0.0 : stats.minSuccessLatencyMs;
                double maxSuccess = (stats.maxSuccessLatencyMs == std::numeric_limits<double>::lowest()) 
                                    ? 0.0 : stats.maxSuccessLatencyMs;
                
                payload["success_count"] = stats.successCount;
                payload["success_latency_avg_ms"] = avgSuccessLatency;
                payload["success_latency_min_ms"] = minSuccess;
                payload["success_latency_max_ms"] = maxSuccess;
            } else {
                payload["success_count"] = 0;
            }
            
            // Error metrics
            if (stats.errorCount > 0) {
                double avgErrorLatency = stats.totalErrorLatencyMs / stats.errorCount;
                double minError = (stats.minErrorLatencyMs == std::numeric_limits<double>::max()) 
                                  ? 0.0 : stats.minErrorLatencyMs;
                double maxError = (stats.maxErrorLatencyMs == std::numeric_limits<double>::lowest()) 
                                  ? 0.0 : stats.maxErrorLatencyMs;
                
                payload["error_count"] = stats.errorCount;
                payload["error_latency_avg_ms"] = avgErrorLatency;
                payload["error_latency_min_ms"] = minError;
                payload["error_latency_max_ms"] = maxError;
            } else {
                payload["error_count"] = 0;
            }
            
            // Total counts
            uint32_t totalCalls = stats.successCount + stats.errorCount;
            payload["total_count"] = totalCalls;
            
            // T2 marker: Use common marker since plugin_name and method_name are in payload
            std::string payloadStr = FormatTelemetryPayload(payload);
            SendT2Event(AGW_MARKER_API_METHOD_STAT, payloadStr);
            
            LOGINFO("API method stats sent: %s::%s (total=%u, success=%u, error=%u, avg_success_latency=%.2f ms)",
                    stats.pluginName.c_str(), stats.methodName.c_str(),
                    totalCalls, stats.successCount, stats.errorCount,
                    stats.successCount > 0 ? stats.totalSuccessLatencyMs / stats.successCount : 0.0);
        }
        
        LOGINFO("API method stats sent: %zu plugin/method combinations", mApiMethodStats.size());
    }

    void AppGatewayTelemetry::SendApiLatencyStats()
    {
        if (mApiLatencyStats.empty()) {
            LOGTRACE("No API latency stats to report");
            return;
        }

        // Send each plugin/API combination using common marker AGW_MARKER_API_LATENCY
        for (const auto& item : mApiLatencyStats) {
            const std::string& latencyKey = item.first;
            const ApiLatencyStats& stats = item.second;
            
            if (stats.count == 0) {
                continue;
            }

            // Build payload with plugin name, API name, and latency statistics
            JsonObject payload;
            payload["plugin_name"] = stats.pluginName;
            payload["api_name"] = stats.apiName;
            payload["reporting_interval_sec"] = mReportingIntervalSec;
            payload["count"] = stats.count;
            
            double avgLatency = stats.totalLatencyMs / stats.count;
            double minLatency = (stats.minLatencyMs == std::numeric_limits<double>::max()) 
                                ? 0.0 : stats.minLatencyMs;
            double maxLatency = (stats.maxLatencyMs == std::numeric_limits<double>::lowest()) 
                                ? 0.0 : stats.maxLatencyMs;
            
            payload["avg_ms"] = avgLatency;
            payload["min_ms"] = minLatency;
            payload["max_ms"] = maxLatency;
            payload["total_ms"] = stats.totalLatencyMs;
            payload["unit"] = AGW_UNIT_MILLISECONDS;
            
            // Use common T2 marker - plugin and API names are in payload
            std::string payloadStr = FormatTelemetryPayload(payload);
            SendT2Event(AGW_MARKER_API_LATENCY, payloadStr);
            
            LOGINFO("API latency stats sent: %s::%s (count=%u, avg=%.2f ms, min=%.2f ms, max=%.2f ms)",
                    stats.pluginName.c_str(), stats.apiName.c_str(),
                    stats.count, avgLatency, minLatency, maxLatency);
        }
        
        LOGINFO("API latency stats sent: %zu plugin/API combinations", mApiLatencyStats.size());
    }

    void AppGatewayTelemetry::SendServiceLatencyStats()
    {
        if (mServiceLatencyStats.empty()) {
            LOGTRACE("No service latency stats to report");
            return;
        }

        // Send each plugin/service combination using common marker AGW_MARKER_SERVICE_LATENCY
        for (const auto& item : mServiceLatencyStats) {
            const std::string& latencyKey = item.first;
            const ServiceLatencyStats& stats = item.second;
            
            if (stats.count == 0) {
                continue;
            }

            // Build payload with plugin name, service name, and latency statistics
            JsonObject payload;
            payload["plugin_name"] = stats.pluginName;
            payload["service_name"] = stats.serviceName;
            payload["reporting_interval_sec"] = mReportingIntervalSec;
            payload["count"] = stats.count;
            
            double avgLatency = stats.totalLatencyMs / stats.count;
            double minLatency = (stats.minLatencyMs == std::numeric_limits<double>::max()) 
                                ? 0.0 : stats.minLatencyMs;
            double maxLatency = (stats.maxLatencyMs == std::numeric_limits<double>::lowest()) 
                                ? 0.0 : stats.maxLatencyMs;
            
            payload["avg_ms"] = avgLatency;
            payload["min_ms"] = minLatency;
            payload["max_ms"] = maxLatency;
            payload["total_ms"] = stats.totalLatencyMs;
            payload["unit"] = AGW_UNIT_MILLISECONDS;
            
            // Use common T2 marker - plugin and service names are in payload
            std::string payloadStr = FormatTelemetryPayload(payload);
            SendT2Event(AGW_MARKER_SERVICE_LATENCY, payloadStr);
            
            LOGINFO("Service latency stats sent: %s::%s (count=%u, avg=%.2f ms, min=%.2f ms, max=%.2f ms)",
                    stats.pluginName.c_str(), stats.serviceName.c_str(),
                    stats.count, avgLatency, minLatency, maxLatency);
        }
        
        LOGINFO("Service latency stats sent: %zu plugin/service combinations", mServiceLatencyStats.size());
    }

    void AppGatewayTelemetry::SendServiceMethodStats()
    {
        if (mServiceMethodStats.empty()) {
            LOGTRACE("No service method stats to report");
            return;
        }

        // Send each plugin/service combination as a separate T2 marker
        for (const auto& item : mServiceMethodStats) {
            const std::string& serviceKey = item.first;
            const ServiceMethodStats& stats = item.second;
            
            if (stats.successCount == 0 && stats.errorCount == 0) {
                continue;
            }

            // Build detailed payload with plugin name, service name, counters, and latency stats
            JsonObject payload;
            payload["plugin_name"] = stats.pluginName;
            payload["service_name"] = stats.serviceName;
            payload["reporting_interval_sec"] = mReportingIntervalSec;
            
            // Success metrics
            if (stats.successCount > 0) {
                double avgSuccessLatency = stats.totalSuccessLatencyMs / stats.successCount;
                double minSuccess = (stats.minSuccessLatencyMs == std::numeric_limits<double>::max()) 
                                    ? 0.0 : stats.minSuccessLatencyMs;
                double maxSuccess = (stats.maxSuccessLatencyMs == std::numeric_limits<double>::lowest()) 
                                    ? 0.0 : stats.maxSuccessLatencyMs;
                
                payload["success_count"] = stats.successCount;
                payload["success_latency_avg_ms"] = avgSuccessLatency;
                payload["success_latency_min_ms"] = minSuccess;
                payload["success_latency_max_ms"] = maxSuccess;
            } else {
                payload["success_count"] = 0;
            }
            
            // Error metrics
            if (stats.errorCount > 0) {
                double avgErrorLatency = stats.totalErrorLatencyMs / stats.errorCount;
                double minError = (stats.minErrorLatencyMs == std::numeric_limits<double>::max()) 
                                  ? 0.0 : stats.minErrorLatencyMs;
                double maxError = (stats.maxErrorLatencyMs == std::numeric_limits<double>::lowest()) 
                                  ? 0.0 : stats.maxErrorLatencyMs;
                
                payload["error_count"] = stats.errorCount;
                payload["error_latency_avg_ms"] = avgErrorLatency;
                payload["error_latency_min_ms"] = minError;
                payload["error_latency_max_ms"] = maxError;
            } else {
                payload["error_count"] = 0;
            }
            
            // Total counts
            uint32_t totalCalls = stats.successCount + stats.errorCount;
            payload["total_count"] = totalCalls;
            
            // T2 marker: Use AGW_MARKER_SERVICE_METHOD_STAT since plugin_name and service_name are in payload
            std::string payloadStr = FormatTelemetryPayload(payload);
            SendT2Event(AGW_MARKER_SERVICE_METHOD_STAT, payloadStr);
            
            LOGINFO("Service method stats sent: %s::%s (total=%u, success=%u, error=%u, avg_success_latency=%.2f ms)",
                    stats.pluginName.c_str(), stats.serviceName.c_str(),
                    totalCalls, stats.successCount, stats.errorCount,
                    stats.successCount > 0 ? stats.totalSuccessLatencyMs / stats.successCount : 0.0);
        }
        
        LOGINFO("Service method stats sent: %zu plugin/service combinations", mServiceMethodStats.size());
    }

    void AppGatewayTelemetry::SendT2Event(const char* marker, const std::string& payload)
    {
        // The T2 API signature takes non-const char* but doesn't modify the strings
        // Safe to use const_cast to avoid unnecessary malloc/copy overhead
        Utils::Telemetry::sendMessage(const_cast<char*>(marker), 
                                       const_cast<char*>(payload.c_str()));
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

    void AppGatewayTelemetry::ResetApiMethodStats()
    {
        mApiMethodStats.clear();
    }

    void AppGatewayTelemetry::ResetApiLatencyStats()
    {
        mApiLatencyStats.clear();
    }

    void AppGatewayTelemetry::ResetServiceLatencyStats()
    {
        mServiceLatencyStats.clear();
    }

    void AppGatewayTelemetry::ResetServiceMethodStats()
    {
        mServiceMethodStats.clear();
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
