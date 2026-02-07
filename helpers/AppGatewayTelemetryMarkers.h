/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
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
// APP GATEWAY INTERNAL MARKERS (Used by AppGatewayTelemetry internally)
// These are aggregated and reported by AppGateway itself, not by external plugins
//=============================================================================

/**
 * @brief Bootstrap time marker
 * @details Records total time taken to start all App Gateway plugins
 * Format: JSON or COMPACT (configurable via SetTelemetryFormat)
 * @payload JSON: { "duration_ms": <uint64>, "plugins_loaded": <uint32> }
 *          COMPACT: <duration_ms>,<plugins_loaded>
 */
#define AGW_MARKER_BOOTSTRAP_TIME                   "AppGwBootstrapTime_split"

/**
 * @brief Health statistics marker
 * @details Aggregate stats emitted at configurable intervals (default 1 hour)
 * @payload JSON: { "websocket_connections": <uint32>, "total_calls": <uint32>,
 *                  "successful_calls": <uint32>, "failed_calls": <uint32>,
 *                  "reporting_interval_sec": <uint32> }
 *          COMPACT: <websocket_connections>,<total_calls>,<successful_calls>,<failed_calls>,<interval_sec>
 */
#define AGW_MARKER_HEALTH_STATS                     "AppGwHealthStats_split"

/**
 * @brief API error statistics marker
 * @details API failure counts aggregated over reporting interval
 * @payload JSON: { "reporting_interval_sec": <uint32>,
 *                  "api_failures": [{ "api": "<name>", "count": <uint32> }, ...] }
 *          COMPACT: <interval_sec>,(<api>,<count>),(<api>,<count>),...
 */
#define AGW_MARKER_API_ERROR_STATS                  "AppGwApiErrorStats_split"

/**
 * @brief External service error statistics marker
 * @details External service failure counts aggregated over reporting interval
 * @payload JSON: { "reporting_interval_sec": <uint32>,
 *                  "service_failures": [{ "service": "<name>", "count": <uint32> }, ...] }
 *          COMPACT: <interval_sec>,(<service>,<count>),(<service>,<count>),...
 */
#define AGW_MARKER_EXT_SERVICE_ERROR_STATS          "AppGwExtServiceError_split"

//=============================================================================
// GENERIC PLUGIN TELEMETRY MARKERS
// Used by all plugins - plugin/service name is included in the payload data
// Plugins should use helper macros from UtilsAppGatewayTelemetry.h
//=============================================================================

/**
 * @brief Plugin API error event marker
 * @details Reports API failures from any plugin. Plugin name included in payload.
 * @usage Use AGW_REPORT_API_ERROR() helper macro from UtilsAppGatewayTelemetry.h
 * @payload { "plugin": "<pluginName>", "api": "<apiName>", "error": "<errorCode>" }
 * @example AGW_REPORT_API_ERROR("GetAppSessionId", AGW_ERROR_TIMEOUT)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_API_ERROR                 "AppGwPluginApiError_split"

/**
 * @brief Plugin external service error event marker
 * @details Reports external service failures from any plugin. Plugin name included in payload.
 * @usage Use AGW_REPORT_EXTERNAL_SERVICE_ERROR() helper macro from UtilsAppGatewayTelemetry.h
 * @payload { "plugin": "<pluginName>", "service": "<serviceName>", "error": "<errorCode>" }
 * @example AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_CONNECTION_TIMEOUT)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR         "AppGwPluginExtServiceError_split"

/**
 * @brief Plugin API latency metric marker
 * @details Reports API call latency from any plugin using RecordTelemetryEvent with JSON payload.
 * @usage Use AGW_REPORT_API_LATENCY() helper macro from UtilsAppGatewayTelemetry.h
 * @payload { "plugin": "<pluginName>", "api": "<apiName>", "latency_ms": <double> }
 * @example AGW_REPORT_API_LATENCY("AuthorizeDataField", 125.5)
 * @note Plugin name comes from AGW_TELEMETRY_INIT initialization
 */
#define AGW_MARKER_PLUGIN_API_LATENCY               "AppGwPluginApiLatency_split"

/**
 * @brief Plugin external service latency metric marker (DEPRECATED - not used)
 * @details This marker is no longer used. Service latency is now reported using RecordTelemetryMetric
 *          with composite metric names in the format: agw_<PluginName>_<ServiceName>_Latency
 * @usage Use AGW_REPORT_SERVICE_LATENCY() helper macro from UtilsAppGatewayTelemetry.h
 * @metric_name agw_<PluginName>_<ServiceName>_Latency (e.g., agw_OttServices_ThorPermissionService_Latency)
 * @metric_value Latency in milliseconds
 * @metric_unit AGW_UNIT_MILLISECONDS
 * @example AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_THOR_PERMISSION, 85.3)
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
 *   // In Initialize() method:
 *   AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_BADGER);
 *
 *   // Report the error (plugin name automatic from init):
 *   AGW_REPORT_API_ERROR("GetAppSessionId", AGW_ERROR_INTERFACE_UNAVAILABLE);
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
 *   // In Initialize() method:
 *   AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_OTTSERVICES);
 *
 *   // Report the service error (plugin name automatic from init):
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION,
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
 *   AGW_REPORT_API_LATENCY("AuthorizeDataField", 125.5);  // latency in ms
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
 *   // In Initialize() method:
 *   AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_OTTSERVICES);
 *
 *   AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_THOR_PERMISSION, 85.3);  // latency in ms
 *
 *   // This internally calls RecordTelemetryMetric with:
 *   //   metricName = "agw_OttServices_ThorPermissionService_Latency"
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
 *   //   metricName = "agw_PermissionDeniedCount"
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
 *   telemetry->RecordTelemetryMetric(context,
 *                                    "agw_MyPlugin_ThorPermissionService_Latency",
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
