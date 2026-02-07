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
 * @file AppGatewayTelemetryMarkers.h
 * @brief Predefined T2 telemetry markers for App Gateway ecosystem
 * 
 * This file defines all standard telemetry markers used across the App Gateway
 * plugin ecosystem. Other developers implementing telemetry in their plugins
 * MUST use these predefined markers when calling IAppGatewayTelemetry interface.
 * 
 * ## Naming Convention
 * 
 * All markers follow this pattern:
 *   `agw_<PluginName><Category><Type>_split`
 * 
 * Where:
 * - `agw_` - App Gateway prefix (mandatory)
 * - `<PluginName>` - Name of the plugin (e.g., Badger, OttServices)
 * - `<Category>` - Category of telemetry (e.g., Api, ExtService)
 * - `<Type>` - Type of data (e.g., Error, Latency, Stats)
 * - `_split` - Suffix indicating structured data payload
 * 
 * ## Usage
 * 
 * When calling IAppGatewayTelemetry::RecordTelemetryEvent():
 *   - eventName = Use one of the predefined markers below
 *   - eventData = JSON string with relevant data
 * 
 * When calling IAppGatewayTelemetry::RecordTelemetryMetric():
 *   - metricName = Use one of the predefined metric markers below
 *   - metricValue = Numeric value
 *   - metricUnit = Use predefined units (AGW_UNIT_*)
 * 
 * ## Adding New Markers
 * 
 * When adding markers for a new plugin:
 * 1. Add a new section below with your plugin name
 * 2. Follow the naming convention
 * 3. Document the expected eventData/metricValue format
 * 4. Update the architecture documentation
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
// These are aggregated and reported by AppGateway itself
//=============================================================================

/**
 * @brief Bootstrap time marker
 * @details Records total time taken to start all App Gateway plugins
 * @payload { "duration_ms": <uint64>, "plugins_loaded": <uint32> }
 */
#define AGW_MARKER_BOOTSTRAP_TIME                   "AppGwBootstrapTime_split"

/**
 * @brief Health statistics marker
 * @details Aggregate stats emitted at configurable intervals (default 1 hour)
 * @payload { "websocket_connections": <uint32>, "total_calls": <uint32>,
 *            "successful_calls": <uint32>, "failed_calls": <uint32>,
 *            "reporting_interval_sec": <uint32> }
 */
#define AGW_MARKER_HEALTH_STATS                     "AppGwHealthStats_split"

/**
 * @brief API error statistics marker
 * @details API failure counts aggregated over reporting interval
 * @payload { "reporting_interval_sec": <uint32>,
 *            "api_failures": [{ "api": "<name>", "count": <uint32> }, ...] }
 */
#define AGW_MARKER_API_ERROR_STATS                  "AppGwApiErrorStats_split"

/**
 * @brief External service error statistics marker
 * @details External service failure counts aggregated over reporting interval
 * @payload { "reporting_interval_sec": <uint32>,
 *            "service_failures": [{ "service": "<name>", "count": <uint32> }, ...] }
 */
#define AGW_MARKER_EXT_SERVICE_ERROR_STATS          "AppGwExtServiceError_split"

//=============================================================================
// GENERIC PLUGIN TELEMETRY MARKERS
// These markers are shared by all plugins - plugin name is included in the data
//=============================================================================

/**
 * @brief Plugin API error event marker
 * @details Reports API failures from any plugin. Plugin name included in data.
 * @payload { "plugin": "<pluginName>", "api": "<apiName>", "error": "<errorCode>" }
 * 
 * Example usage:
 *   AGW_REPORT_API_ERROR(AGW_MARKER_PLUGIN_API_ERROR, "Badger", "GetAppSessionId", "INTERFACE_UNAVAILABLE")
 */
#define AGW_MARKER_PLUGIN_API_ERROR                 "AppGwPluginApiError_split"

/**
 * @brief Plugin external service error event marker
 * @details Reports external service failures from any plugin. Plugin name included in data.
 * @payload { "plugin": "<pluginName>", "service": "<serviceName>", "error": "<errorCode>" }
 * 
 * Example usage:
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR, "OttServices", "ThorPermissionService", "CONNECTION_TIMEOUT")
 */
#define AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR         "AppGwPluginExtServiceError_split"

/**
 * @brief Plugin API latency metric marker
 * @details Reports API call latency from any plugin. Plugin and API name included in data.
 * @payload { "plugin": "<pluginName>", "api": "<apiName>", "latency_ms": <double> }
 * @unit AGW_UNIT_MILLISECONDS
 */
#define AGW_MARKER_PLUGIN_API_LATENCY               "AppGwPluginApiLatency_split"

/**
 * @brief Plugin external service latency metric marker
 * @details Reports external service call latency from any plugin.
 * @payload { "plugin": "<pluginName>", "service": "<serviceName>", "latency_ms": <double> }
 * @unit AGW_UNIT_MILLISECONDS
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
// How to use the generic markers with plugin/method names as values
//=============================================================================


/*
 * Example: Reporting an API error from Badger plugin
 *
 *   // In Badger.cpp
 *   AGW_REPORT_API_ERROR(AGW_MARKER_PLUGIN_API_ERROR, 
 *                        AGW_PLUGIN_BADGER, 
 *                        "GetAppSessionId", 
 *                        AGW_ERROR_INTERFACE_UNAVAILABLE);
 *
 *   // This sends: { "plugin": "Badger", "api": "GetAppSessionId", "error": "INTERFACE_UNAVAILABLE" }
 *
 * Example: Reporting an external service error from OttServices plugin
 *
 *   // In OttServicesImplementation.cpp
 *   AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR,
 *                                      AGW_PLUGIN_OTTSERVICES,
 *                                      AGW_SERVICE_THOR_PERMISSION,
 *                                      AGW_ERROR_CONNECTION_TIMEOUT);
 *
 *   // This sends: { "plugin": "OttServices", "service": "ThorPermissionService", "error": "CONNECTION_TIMEOUT" }
 *
 * Example: Reporting API latency from any plugin
 *
 *   AGW_REPORT_API_LATENCY(AGW_MARKER_PLUGIN_API_LATENCY,
 *                          AGW_PLUGIN_BADGER,
 *                          "AuthorizeDataField",
 *                          125.5);  // latency in ms
 *
 *   // This sends: { "plugin": "Badger", "api": "AuthorizeDataField", "latency_ms": 125.5 }
 *
 * Example: Reporting a custom metric (PermissionGroup failures) from Badger plugin
 *
 *   // In Badger.cpp
 *   static uint32_t permissionGroupFailureCount = 0;
 *   ++permissionGroupFailureCount;
 *   AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, permissionGroupFailureCount, AGW_UNIT_COUNT);
 *
 *   // This sends: { "plugin": "Badger", "metric": "PermissionGroupFailures", "value": <failureCount>, "unit": "count" }
 *
 * Example: Reporting a custom metric (gRPC failures) from OttServices plugin
 *
 *   // In OttServicesImplementation.cpp
 *   static uint32_t grpcFailureCount = 0;
 *   ++grpcFailureCount;
 *   AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, grpcFailureCount, AGW_UNIT_COUNT);
 *
 *   // This sends: { "plugin": "OttServices", "metric": "GrpcFailureCount", "value": <grpcFailureCount>, "unit": "count" }
 *
 * Adding a new plugin:
 * 1. Add plugin name constant: #define AGW_PLUGIN_MYPLUGIN "MyPlugin"
 * 2. Use the generic markers with your plugin name
 * 3. No need to create plugin-specific markers!
 */
