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

/**
 * @file AppGatewayTelemetryMarkers.h
 * @brief Predefined T2 telemetry markers for App Gateway ecosystem
 * 
 * This file defines all standard telemetry markers used across the App Gateway
 * plugin ecosystem. Plugins use these markers when reporting telemetry via the
 * IAppGatewayTelemetry COM-RPC interface.
 * 
 * ## Marker Design - Generic Category-Based Approach
 * 
 * The system uses GENERIC markers where the plugin/service name is part of the
 * payload data rather than the marker name itself. This reduces T2 marker count
 * and simplifies codebase maintenance.
 * 
 * All markers follow this pattern:
 *   `AppGw<Category><Type>_split`
 * 
 * Where:
 * - `AppGw` - App Gateway prefix (identifies the source)
 * - `<Category>` - Category of telemetry (e.g., Plugin, Health, Api)
 * - `<Type>` - Type of data (e.g., ApiError, ExtServiceError, ApiLatency)
 * - `_split` - Suffix indicating structured/split format for T2
 * 
 * ## Usage
 * 
 * Plugins should use the helper functions from UtilsAppGatewayTelemetry.h:
 *   - AGW_REPORT_API_ERROR() - Reports API failures (uses RecordTelemetryEvent)
 *   - AGW_REPORT_EXTERNAL_SERVICE_ERROR() - Reports external service failures (uses RecordTelemetryEvent)
 *   - AGW_REPORT_API_LATENCY() - Reports API call latency (uses RecordTelemetryEvent with JSON)
 *   - AGW_REPORT_SERVICE_LATENCY() - Reports external service latency (uses RecordTelemetryMetric with aggregation)
 * 
 * Direct COM-RPC interface usage (if helper macros unavailable):
 *   IAppGatewayTelemetry::RecordTelemetryEvent(context, markerName, jsonPayload)
 *   IAppGatewayTelemetry::RecordTelemetryMetric(context, metricName, value, unit)
 * 
 * ## Adding Support for New Plugins
 * 
 * When integrating a new plugin:
 * 1. Add your plugin name constant below (if not already present)
 * 2. Use existing generic markers (AGW_MARKER_PLUGIN_API_ERROR, etc.)
 * 3. Include plugin name in the payload data using your constant
 * 4. No need to create new plugin-specific markers!
 */

//=============================================================================
// METRIC UNITS
// Use these standard units for RecordTelemetryMetric
//=============================================================================

#define AGW_UNIT_MILLISECONDS           "ms"
#define AGW_UNIT_SECONDS                "sec"
#define AGW_UNIT_COUNT                  "count"
#define AGW_UNIT_BYTES                  "bytes"
#define AGW_UNIT_KILOBYTES              "KB"
#define AGW_UNIT_MEGABYTES              "MB"
#define AGW_UNIT_KBPS                   "kbps"
#define AGW_UNIT_MBPS                   "Mbps"
#define AGW_UNIT_PERCENT                "percent"

//=============================================================================
// TELEMETRY MARKER SUFFIX
// Standard suffix for all T2 telemetry markers indicating structured/split format
//=============================================================================

/**
 * @brief Standard T2 marker suffix
 * @details All telemetry markers use _split suffix to indicate structured format for T2
 */
#define AGW_METRIC_SUFFIX                           "_split"

//=============================================================================
// APP GATEWAY INTERNAL METRICS (Used by AppGatewayTelemetry internally)
// These are aggregated and reported by AppGateway itself as individual metrics
//=============================================================================

/**
 * @brief Bootstrap duration metric (sent once on startup)
 * @details Total time taken to start all App Gateway plugins
 * @payload { "sum": <duration_ms>, "count": 1, "unit": "ms", "reporting_interval_sec": 0 }
 */
#define AGW_MARKER_BOOTSTRAP_DURATION               "AppGwBootstrapDuration_split"

/**
 * @brief Bootstrap plugin count metric (sent once on startup)
 * @details Number of plugins successfully loaded
 * @payload { "sum": <plugins_loaded>, "count": 1, "unit": "count", "reporting_interval_sec": 0 }
 */
#define AGW_MARKER_BOOTSTRAP_PLUGIN_COUNT           "AppGwBootstrapPluginCount_split"

/**
 * @brief WebSocket connections metric (sent periodically)
 * @details Current active WebSocket connections
 * @payload { "sum": <connections>, "count": 1, "unit": "count", "reporting_interval_sec": 3600 }
 */
#define AGW_MARKER_WEBSOCKET_CONNECTIONS            "AppGwWebSocketConnections_split"

/**
 * @brief Total API calls metric (sent periodically)
 * @details Total number of API calls in reporting period
 * @payload { "sum": <calls>, "count": 1, "unit": "count", "reporting_interval_sec": 3600 }
 */
#define AGW_MARKER_TOTAL_CALLS                      "AppGwTotalCalls_split"

/**
 * @brief Successful API calls metric (sent periodically)
 * @details Number of successful API calls in reporting period
 * @payload { "sum": <calls>, "count": 1, "unit": "count", "reporting_interval_sec": 3600 }
 */
#define AGW_MARKER_SUCCESSFUL_CALLS                 "AppGwSuccessfulCalls_split"

/**
 * @brief Failed API calls metric (sent periodically)
 * @details Number of failed API calls in reporting period
 * @payload { "sum": <calls>, "count": 1, "unit": "count", "reporting_interval_sec": 3600 }
 */
#define AGW_MARKER_FAILED_CALLS                     "AppGwFailedCalls_split"

/**
 * @brief Consolidated health statistics marker (sent periodically)
 * @details Aggregated health metrics for AppGateway including WebSocket connections and API call statistics
 * @payload {
 *   "reporting_interval_sec": 3600,
 *   "websocket_connections": <active_connections>,
 *   "total_calls": <total_api_calls>,
 *   "successful_calls": <successful_calls>,
 *   "failed_calls": <failed_calls>,
 *   "unit": "count"
 * }
 * @note Individual markers (AGW_MARKER_WEBSOCKET_CONNECTIONS, AGW_MARKER_TOTAL_CALLS,
 *       AGW_MARKER_SUCCESSFUL_CALLS, AGW_MARKER_FAILED_CALLS) are available for plugin-specific use
 */
#define AGW_MARKER_HEALTH_STATS                     "AppGwHealthStats_split"

/**
 * @brief API error count metric prefix
 * @details Per-API error count metrics sent periodically
 * @usage Metric name: AGW_METRIC_API_ERROR_COUNT_PREFIX + <ApiName> + AGW_METRIC_SUFFIX
 * @example "AppGwApiErrorCount_GetSettings_split"
 * @payload { "sum": <error_count>, "count": 1, "unit": "count", "reporting_interval_sec": 3600 }
 */
#define AGW_METRIC_API_ERROR_COUNT_PREFIX           "AppGwApiErrorCount_"

/**
 * @brief DEPRECATED: Old aggregated API error stats marker (no longer used)
 * @details Replaced by per-API metrics using AGW_METRIC_API_ERROR_COUNT_PREFIX + <ApiName>
 */
#define AGW_MARKER_API_ERROR_STATS                  "AppGwApiErrorStats_split"

/**
 * @brief External service error count metric prefix
 * @details Per-service error count metrics sent periodically
 * @usage Metric name: AGW_METRIC_EXT_SERVICE_ERROR_COUNT_PREFIX + <ServiceName> + AGW_METRIC_SUFFIX
 * @example "AppGwExtServiceErrorCount_ThorPermissionService_split"
 * @payload { "sum": <error_count>, "count": 1, "unit": "count", "reporting_interval_sec": 3600 }
 */
#define AGW_METRIC_EXT_SERVICE_ERROR_COUNT_PREFIX   "AppGwExtServiceErrorCount_"

/**
 * @brief DEPRECATED: Old aggregated external service error marker (no longer used)
 * @details Replaced by per-service metrics using AGW_METRIC_EXT_SERVICE_ERROR_COUNT_PREFIX + <ServiceName>
 */
#define AGW_MARKER_EXT_SERVICE_ERROR_STATS          "AppGwExtServiceError_split"

/**
 * @brief Per-API method statistics marker (common marker for all plugin/method combinations)
 * @details Used to report detailed per-API statistics including counters and latency metrics
 * @usage Single marker for all: "AppGwApiMethod_split"
 * @payload {
 *   "plugin_name": "<PluginName>",
 *   "method_name": "<MethodName>",
 *   "reporting_interval_sec": 3600,
 *   "total_count": <total_calls>,
 *   "success_count": <success_count>,
 *   "success_latency_avg_ms": <avg>,
 *   "success_latency_min_ms": <min>,
 *   "success_latency_max_ms": <max>,
 *   "error_count": <error_count>,
 *   "error_latency_avg_ms": <avg>,
 *   "error_latency_min_ms": <min>,
 *   "error_latency_max_ms": <max>
 * }
 * @example For LaunchDelegate.session():
 *   Marker: "AppGwApiMethod_split"
 *   Payload includes: "plugin_name": "LaunchDelegate", "method_name": "session"
 */
#define AGW_MARKER_API_METHOD_STAT                       "AppGwApiMethod_split"

/**
 * @brief API latency statistics marker (common marker for all plugin/API combinations)
 * @details Used to report aggregated API latency metrics from plugins
 * @usage Single marker for all: "AppGwApiLatency_split"
 * @payload {
 *   "plugin_name": "<PluginName>",
 *   "api_name": "<ApiName>",
 *   "reporting_interval_sec": 3600,
 *   "count": <total_calls>,
 *   "avg_ms": <average_latency>,
 *   "min_ms": <minimum_latency>,
 *   "max_ms": <maximum_latency>,
 *   "total_ms": <total_latency>,
 *   "unit": "Milliseconds"
 * }
 * @example For Badger.GetSettings():
 *   Marker: "AppGwApiLatency_split"
 *   Payload includes: "plugin_name": "Badger", "api_name": "GetSettings"
 */
#define AGW_MARKER_API_LATENCY                      "AppGwApiLatency_split"

/**
 * @brief Service latency statistics marker (common marker for all plugin/service combinations)
 * @details Used to report aggregated external service latency metrics from plugins
 * @usage Single marker for all: "AppGwServiceLatency_split"
 * @payload {
 *   "plugin_name": "<PluginName>",
 *   "service_name": "<ServiceName>",
 *   "reporting_interval_sec": 3600,
 *   "count": <total_calls>,
 *   "avg_ms": <average_latency>,
 *   "min_ms": <minimum_latency>,
 *   "max_ms": <maximum_latency>,
 *   "total_ms": <total_latency>,
 *   "unit": "Milliseconds"
 * }
 * @example For OttServices calling ThorPermissionService:
 *   Marker: "AppGwServiceLatency_split"
 *   Payload includes: "plugin_name": "OttServices", "service_name": "ThorPermissionService"
 */
#define AGW_MARKER_SERVICE_LATENCY                  "AppGwServiceLatency_split"

/**
 * @brief Per-service method statistics marker (common marker for all plugin/service combinations)
 * @details Used to report detailed per-service statistics including counters and latency metrics
 * @usage Single marker for all: "AppGwServiceMethod_split"
 * @payload {
 *   "plugin_name": "<PluginName>",
 *   "service_name": "<ServiceName>",
 *   "reporting_interval_sec": 3600,
 *   "total_count": <total_calls>,
 *   "success_count": <success_count>,
 *   "success_latency_avg_ms": <avg>,
 *   "success_latency_min_ms": <min>,
 *   "success_latency_max_ms": <max>,
 *   "error_count": <error_count>,
 *   "error_latency_avg_ms": <avg>,
 *   "error_latency_min_ms": <min>,
 *   "error_latency_max_ms": <max>
 * }
 * @example For OttServices calling ThorPermissionService:
 *   Marker: "AppGwServiceMethod_split"
 *   Payload includes: "plugin_name": "OttServices", "service_name": "ThorPermissionService"
 */
#define AGW_MARKER_SERVICE_METHOD_STAT              "AppGwServiceMethod_split"

//=============================================================================
// GENERIC PLUGIN TELEMETRY MARKERS
// Used by all plugins - plugin/service name is included in the payload data
// Plugins should use helper macros from UtilsAppGatewayTelemetry.h
//=============================================================================

//=============================================================================
// LATENCY METRIC COMPONENTS
// Components used to construct composite latency metric names
//=============================================================================

/**
 * @brief Latency metric name prefix
 * @details Used to construct composite latency metric names
 * @usage AGW_METRIC_LATENCY_PREFIX + <PluginName> + "_" + <ApiOrService> + AGW_METRIC_LATENCY_SUFFIX
 */
#define AGW_METRIC_LATENCY_PREFIX                   "AppGw"

/**
 * @brief Latency metric name suffix
 * @details Appended to latency metric names (includes _split)
 * @usage AGW_METRIC_LATENCY_PREFIX + <PluginName> + "_" + <ApiOrService> + AGW_METRIC_LATENCY_SUFFIX
 */
#define AGW_METRIC_LATENCY_SUFFIX                   "_Latency_split"

//=============================================================================
// GENERIC PLUGIN EVENT MARKERS (OPTIONAL - for forensics)
// Used by plugins for immediate error reporting with JSON context
// Plugins should use helper macros from UtilsAppGatewayTelemetry.h
//=============================================================================

/**
 * @brief Plugin API error event marker
 * @details Reports API failures from any plugin. Plugin name included in payload.
 * @usage Use AGW_REPORT_API_ERROR(context, api, error) helper macro from UtilsAppGatewayTelemetry.h
 * @payload { "plugin": "<pluginName>", "api": "<apiName>", "error": "<errorCode>" }
 * @example AGW_REPORT_API_ERROR(context, "GetAppSessionId", AGW_ERROR_TIMEOUT)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_API_ERROR                 "AppGwPluginApiError_split"

/**
 * @brief Plugin external service error event marker
 * @details Reports external service failures from any plugin. Plugin name included in payload.
 * @usage Use AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, service, error) helper macro from UtilsAppGatewayTelemetry.h
 * @payload { "plugin": "<pluginName>", "service": "<serviceName>", "error": "<errorCode>" }
 * @example AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_CONNECTION_TIMEOUT)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR         "AppGwPluginExtServiceError_split"

/**
 * @brief Plugin API latency metric marker
 * @details Reports API call latency from any plugin using RecordTelemetryEvent with JSON payload.
 * @usage Use AGW_REPORT_API_LATENCY(context, api, latencyMs) helper macro from UtilsAppGatewayTelemetry.h
 * @payload { "plugin": "<pluginName>", "api": "<apiName>", "latency_ms": <double> }
 * @example AGW_REPORT_API_LATENCY(context, "AuthorizeDataField", 125.5)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_API_LATENCY               "AppGwPluginApiLatency_split"

/**
 * @brief Plugin external service latency metric marker (DEPRECATED - not used)
 * @details This marker is no longer used. Service latency is now reported using RecordTelemetryMetric
 *          with composite metric names in the format: agw_<PluginName>_<ServiceName>_Latency
 * @usage Use AGW_REPORT_SERVICE_LATENCY(context, service, latencyMs) helper macro from UtilsAppGatewayTelemetry.h
 * @metric_name agw_<PluginName>_<ServiceName>_Latency (e.g., agw_OttServices_ThorPermissionService_Latency)
 * @metric_value Latency in milliseconds
 * @metric_unit AGW_UNIT_MILLISECONDS
 * @example AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_THOR_PERMISSION, 85.3)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_SERVICE_LATENCY           "AppGwPluginServiceLatency_split"

//=============================================================================
// PREDEFINED PLUGIN NAMES
// Use these when reporting telemetry for consistency
//=============================================================================

#define AGW_PLUGIN_BADGER                           "Badger"
#define AGW_PLUGIN_OTTSERVICES                      "OttServices"
#define AGW_PLUGIN_APPGATEWAY                       "AppGateway"
#define AGW_PLUGIN_FBADVERTISING                    "FbAdvertising"
#define AGW_PLUGIN_FBDISCOVERY                      "FbDiscovery"
#define AGW_PLUGIN_FBENTOS                          "FbEntos"
#define AGW_PLUGIN_FBMETRICS                        "FbMetrics"
#define AGW_PLUGIN_FBPRIVACY                        "FbPrivacy"

//=============================================================================
// PREDEFINED EXTERNAL SERVICE NAMES
// Use these when reporting external service errors for consistency
//=============================================================================

/**
 * @brief Thor Permission Service (gRPC)
 * @details Used by OttServices for permission checks
 */
#define AGW_SERVICE_THOR_PERMISSION                 "ThorPermissionService"

/**
 * @brief OTT Token Service (gRPC)
 * @details Used by OttServices for CIMA token generation
 */
#define AGW_SERVICE_OTT_TOKEN                       "OttTokenService"

/**
 * @brief Auth Service (COM-RPC)
 * @details Used for SAT/xACT token retrieval
 */
#define AGW_SERVICE_AUTH                            "AuthService"

/**
 * @brief Auth Metadata Service
 * @details Used for collecting authentication metadata (token, deviceId, accountId, partnerId)
 */
#define AGW_SERVICE_AUTH_METADATA                   "AuthMetadataService"

/**
 * @brief OttServices Interface (COM-RPC)
 * @details Used by Badger to access OTT permissions
 */
#define AGW_SERVICE_OTT_SERVICES                    "OttServices"

/**
 * @brief Launch Delegate Interface (COM-RPC)
 * @details Used for app session management
 */
#define AGW_SERVICE_LAUNCH_DELEGATE                 "LaunchDelegate"

/**
 * @brief Lifecycle Delegate
 * @details Used for device session management
 */
#define AGW_SERVICE_LIFECYCLE_DELEGATE              "LifecycleDelegate"

/**
 * @brief Internal Permission Service
 * @details AppGateway internal permission checking
 */
#define AGW_SERVICE_PERMISSION                      "PermissionService"

/**
 * @brief Authentication Service (WebSocket)
 * @details AppGateway WebSocket authentication
 */
#define AGW_SERVICE_AUTHENTICATION                  "AuthenticationService"

//=============================================================================
// PREDEFINED ERROR CODES
// Use these when reporting errors for consistency in analytics
//=============================================================================

#define AGW_ERROR_INTERFACE_UNAVAILABLE             "INTERFACE_UNAVAILABLE"
#define AGW_ERROR_INTERFACE_NOT_FOUND               "INTERFACE_NOT_FOUND"
#define AGW_ERROR_CLIENT_NOT_INITIALIZED            "CLIENT_NOT_INITIALIZED"
#define AGW_ERROR_CONNECTION_REFUSED                "CONNECTION_REFUSED"
#define AGW_ERROR_CONNECTION_TIMEOUT                "CONNECTION_TIMEOUT"
#define AGW_ERROR_TIMEOUT                           "TIMEOUT"
#define AGW_ERROR_PERMISSION_DENIED                 "PERMISSION_DENIED"
#define AGW_ERROR_INVALID_RESPONSE                  "INVALID_RESPONSE"
#define AGW_ERROR_INVALID_REQUEST                   "INVALID_REQUEST"
#define AGW_ERROR_NOT_AVAILABLE                     "NOT_AVAILABLE"
#define AGW_ERROR_FETCH_FAILED                      "FETCH_FAILED"
#define AGW_ERROR_UPDATE_FAILED                     "UPDATE_FAILED"
#define AGW_ERROR_COLLECTION_FAILED                 "COLLECTION_FAILED"
#define AGW_ERROR_GENERAL                           "GENERAL_ERROR"

//=============================================================================
// USAGE EXAMPLES
// How to use the markers with helper functions from UtilsAppGatewayTelemetry.h
//=============================================================================

/*
 * Example 1: Reporting an API error from Badger plugin
 *
 *   // In Badger.cpp - Include the helper and initialize
 *   #include "UtilsAppGatewayTelemetry.h"
 *
 *   // At top of file, before namespace:
 *   AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_BADGER)
 *
 *   // In Initialize() method:
 *   AGW_TELEMETRY_INIT(mService);
 *
 *   // Report the error (plugin name automatic from AGW_DEFINE_TELEMETRY_CLIENT):
 *   // Note: context parameter contains requestId, connectionId, appId for request correlation
 *   AGW_REPORT_API_ERROR(context, "GetAppSessionId", AGW_ERROR_INTERFACE_UNAVAILABLE);
 *
 *   // This internally calls RecordTelemetryEvent with:
 *   //   eventName = AGW_MARKER_PLUGIN_API_ERROR
 *   //   eventData = { "plugin": "Badger", "api": "GetAppSessionId", 
 *   //                 "error": "INTERFACE_UNAVAILABLE" }
 *   //   (Plugin name from initialization)
 *
 *
 * Example 2: Reporting an external service error from OttServices plugin
 *
 *   // In OttServicesImplementation.cpp
 *   #include "UtilsAppGatewayTelemetry.h"
 *
 *   // At top of file, before namespace:
 *   AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_OTTSERVICES)
 *
 *   // In Initialize() method:
 *   AGW_TELEMETRY_INIT(mService);
 *
 *   // Report the service error (plugin name automatic from AGW_DEFINE_TELEMETRY_CLIENT):
 *   // Note: context parameter contains requestId, connectionId, appId for request correlation
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_THOR_PERMISSION,
 *                                      AGW_ERROR_CONNECTION_TIMEOUT);
 *
 *   // This internally calls RecordTelemetryEvent with:
 *   //   eventName = AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR
 *   //   eventData = { "plugin": "OttServices", "service": "ThorPermissionService", 
 *   //                 "error": "CONNECTION_TIMEOUT" }
 *   //   (Plugin name from initialization)
 *
 *
 * Example 3: Reporting API latency from any plugin
 *
 *   #include "UtilsAppGatewayTelemetry.h"
 *
 *   // Note: context parameter contains requestId, connectionId, appId for request correlation
 *   AGW_REPORT_API_LATENCY(context, "AuthorizeDataField", 125.5);  // latency in ms
 *
 *   // This internally calls RecordTelemetryEvent (NOT RecordTelemetryMetric) with:
 *   //   eventName = AGW_MARKER_PLUGIN_API_LATENCY
 *   //   eventData = { "plugin": "<yourPluginName>", "api": "AuthorizeDataField", 
 *   //                 "latency_ms": 125.5 }
 *   //   (Plugin name comes from AGW_TELEMETRY_INIT initialization)
 *
 *
 * Example 4: Reporting external service latency
 *
 *   #include "UtilsAppGatewayTelemetry.h"
 *
 *   // At top of file, before namespace:
 *   AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_OTTSERVICES)
 *
 *   // In Initialize() method:
 *   AGW_TELEMETRY_INIT(mService);
 *
 *   // Note: context parameter contains requestId, connectionId, appId for request correlation
 *   AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_THOR_PERMISSION, 85.3);  // latency in ms
 *
 *   // This internally calls RecordTelemetryMetric with:
 *   //   metricName = AGW_METRIC_LATENCY_PREFIX + "OttServices_ThorPermissionService" + AGW_METRIC_LATENCY_SUFFIX
 *   //              = "AppGwOttServices_ThorPermissionService_Latency_split"
 *   //   metricValue = 85.3
 *   //   metricUnit = AGW_UNIT_MILLISECONDS
 *   //
 *   // AppGateway aggregates this metric and reports:
 *   //   - sum, min, max, avg, count over the reporting interval
 *   //   (Plugin name from AGW_TELEMETRY_INIT initialization)
 *
 *
 * Example 5: Reporting a custom numeric metric using RecordTelemetryMetric
 *
 *   #include "UtilsAppGatewayTelemetry.h"
 *
 *   // Track a custom counter metric
 *   static uint32_t permissionDeniedCount = 0;
 *   permissionDeniedCount++;
 *
 *   AGW_REPORT_METRIC("agw_PermissionDeniedCount", 
 *                     static_cast<double>(permissionDeniedCount), 
 *                     AGW_UNIT_COUNT);
 *
 *   // This internally calls RecordTelemetryMetric with:
 *   //   metricName = "AppGwPermissionDeniedCount"  // Use AppGw prefix for custom metrics
 *   //   metricValue = <count>
 *   //   metricUnit = "count"
 *
 *   // For aggregated metrics (sum/min/max/avg), AppGateway will compute:
 *   //   - sum of all reported values
 *   //   - min/max values
 *   //   - average (sum/count)
 *   //   - total number of reports
 *
 *
 * Example 6: Direct COM-RPC interface usage (without helper macros)
 *
 *   // When UtilsAppGatewayTelemetry.h is not available
 *   Exchange::IAppGatewayTelemetry* telemetry = ...;
 *   Exchange::GatewayContext context;
 *   context.appId = "MyPlugin";
 *   
 *   // Using RecordTelemetryEvent (for errors and events):
 *   JsonObject eventData;
 *   eventData["plugin"] = AGW_PLUGIN_BADGER;
 *   eventData["api"] = "GetData";
 *   eventData["error"] = AGW_ERROR_TIMEOUT;
 *   std::string eventDataStr;
 *   eventData.ToString(eventDataStr);
 *
 *   telemetry->RecordTelemetryEvent(context, 
 *                                   AGW_MARKER_PLUGIN_API_ERROR, 
 *                                   eventDataStr);
 *
 *   // Using RecordTelemetryMetric (for numeric metrics with aggregation):
 *   // Construct metric name using constants:
 *   std::string metricName = std::string(AGW_METRIC_LATENCY_PREFIX) + "MyPlugin_ThorPermissionService" + AGW_METRIC_LATENCY_SUFFIX;
 *   // Result: "AppGwMyPlugin_ThorPermissionService_Latency_split"
 *   telemetry->RecordTelemetryMetric(context,
 *                                    metricName.c_str(),
 *                                    125.5,
 *                                    AGW_UNIT_MILLISECONDS);
 *   
 *   // Metric will be aggregated (sum/min/max/avg/count) and reported periodically
 *
 *
 * Adding a new plugin:
 * 1. Add plugin name constant: #define AGW_PLUGIN_MYPLUGIN "MyPlugin"
 * 2. Use the existing generic markers (shown above)
 * 3. Call helper macros with your plugin name constant
 * 4. No need to create plugin-specific markers!
 */
