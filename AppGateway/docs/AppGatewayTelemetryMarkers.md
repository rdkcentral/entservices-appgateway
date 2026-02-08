# App Gateway Telemetry Markers

This document provides a comprehensive reference for all T2 telemetry markers used in the App Gateway ecosystem.

## Marker Naming Convention

The App Gateway telemetry system uses a hybrid approach:

### Events (Optional - for forensics)
Pattern: `AppGwPlugin<Category>_split`
- Single marker per category, shared across all plugins
- Plugin name is included in the payload data, not in the marker name
- Used for immediate error reporting with JSON context
- Example: `AppGwPluginApiError_split` with `{"plugin":"Badger","api":"GetSettings","error":"TIMEOUT"}`
- Enables detailed debugging of WHAT went wrong

### Metrics (Required - for monitoring)
Pattern: `AppGw<Metric_Name>_split` (individual) or `AppGw<Plugin>_<Api/Service>_Latency_split` (composite)
- Each statistic gets its own unique metric name
- Sent periodically (default: hourly) with statistical aggregation
- Used for trending, alerting, and statistical analysis
- Examples:
  - Individual stats: `AppGwBootstrapDuration_split`, `AppGwWebSocketConnections_split`, `AppGwTotalCalls_split`
  - Error counts: `AppGwApiErrorCount_GetSettings_split`, `AppGwExtServiceErrorCount_ThorPermissionService_split`
  - Latency: `AppGwBadger_GetDeviceSessionId_Latency_split`, `AppGwOttServices_ThorPermissionService_Latency_split`
- Enables alerting on HOW MANY failures occurred or HOW LONG operations took

This design provides both immediate forensics (events) and long-term trending (metrics).

---

## Metric Payload Format Explained

All metrics sent to T2 use a standardized JSON payload format with the following fields:

```json
{
  "sum": 15,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

### Field Descriptions

| Field | Type | Description |
|-------|------|-------------|
| `sum` | number | **The aggregated value being reported**<br/>• For error counts: Total number of errors in the reporting period<br/>• For latency metrics: Sum of all latency measurements<br/>• For counters: Current count value |
| `count` | integer | **Number of data points in this aggregation**<br/>• For error counts: Always `1` (single aggregated value per period)<br/>• For latency metrics: Number of samples collected (e.g., 25 API calls)<br/>• Used by T2 backend to calculate averages |
| `unit` | string | **Unit of measurement for the sum value**<br/>• `"count"` - number of occurrences/errors<br/>• `"ms"` - milliseconds (for latency/duration)<br/>• `"percent"` - percentage values<br/>• See [Metric Units](#metric-units) section for all supported units |
| `reporting_interval_sec` | integer | **Time period over which data was collected**<br/>• `3600` = 1 hour (default for periodic metrics)<br/>• `0` = immediate/one-time metric (e.g., bootstrap)<br/>• Tells T2: "This data covers X seconds of operation" |

### Extended Format for Latency Metrics

Latency metrics include additional statistical fields:

```json
{
  "sum": 1250.5,
  "min": 45.2,
  "max": 350.8,
  "count": 25,
  "avg": 50.02,
  "unit": "ms",
  "reporting_interval_sec": 3600
}
```

| Field | Type | Description |
|-------|------|-------------|
| `min` | number | Minimum (fastest) value observed in the period |
| `max` | number | Maximum (slowest) value observed in the period |
| `avg` | number | Average value (`sum / count`) |

### Real-World Example: Error Count Metric

**Scenario**: API "GetSettings" failed 15 times during the last hour

**What Happens**:
1. Each API failure calls `RecordApiError("GetSettings")`
2. AppGatewayTelemetry increments internal counter: `mApiErrorCounts["GetSettings"]++`
3. After 1 hour, timer fires and sends this metric:

```
Marker: AppGwApiErrorCount_GetSettings_split
Payload: {"sum": 15, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
```

**T2 Backend Calculation**:
- Error rate = 15 errors / 3600 seconds = **0.0042 errors/second**
- Error rate = 15 errors / 60 minutes = **0.25 errors/minute**

### Real-World Example: Latency Metric

**Scenario**: API "GetDeviceSessionId" was called 25 times in the last hour

**What Happens**:
1. Each API call measures latency and calls `RecordApiLatency("GetDeviceSessionId", latencyMs)`
2. AppGatewayTelemetry aggregates: sum, min, max, count
3. After 1 hour, sends this metric:

```
Marker: AppGwBadger_GetDeviceSessionId_Latency_split
Payload: {
  "sum": 1250.5,
  "min": 45.2,
  "max": 350.8,
  "count": 25,
  "avg": 50.02,
  "unit": "ms",
  "reporting_interval_sec": 3600
}
```

**T2 Backend Analysis**:
- Average latency = 50.02 ms
- Performance range: 45.2 ms (best) to 350.8 ms (worst)
- Call volume: 25 calls/hour = 1 call every 144 seconds

### Why This Design?

This metric-based approach provides:

**Efficiency**: One payload per hour instead of hundreds/thousands of individual events

**Trending**: Track error rates and performance over time (hourly, daily, weekly)

**Alerting**: Backend can trigger alerts when metrics exceed thresholds

**Aggregation**: Multiple devices' data can be easily combined and analyzed

**Statistical Analysis**: Min/max/avg values enable performance profiling

---

## App Gateway Internal Metrics

These metrics are aggregated and reported by App Gateway itself as individual numeric values.

### Bootstrap Metrics (Sent once on startup)

| Telemetry Marker Name | Unit | Description |
|-----------------------|------|-------------|
| `AppGwBootstrapDuration_split` | ms | Total time to bootstrap all App Gateway ecosystem plugins |
| `AppGwBootstrapPluginCount_split` | count | Number of plugins successfully loaded |

**Example Payload (Duration):**
```json
{"sum": 1250, "count": 1, "unit": "ms", "reporting_interval_sec": 0}
```

### Health Metrics (Sent periodically, default: 1 hour)

| Telemetry Marker Name | Unit | Description |
|-----------------------|------|-------------|
| `AppGwWebSocketConnections_split` | count | Active WebSocket connections |
| `AppGwTotalCalls_split` | count | Total API calls in reporting period |
| `AppGwSuccessfulCalls_split` | count | Successful API calls |
| `AppGwFailedCalls_split` | count | Failed API calls |

**Example Payload (Total Calls):**
```json
{"sum": 1543, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
```

### Error Count Metrics (Sent periodically per API/Service)

| Telemetry Marker Pattern | Unit | Description |
|--------------------------|------|-------------|
| `AppGwApiErrorCount_<ApiName>_split` | count | Error count for specific API (e.g., `AppGwApiErrorCount_GetSettings_split`) |
| `AppGwExtServiceErrorCount_<ServiceName>_split` | count | Error count for specific service (e.g., `AppGwExtServiceErrorCount_ThorPermissionService_split`) |

**Example Payload (API Error Count):**
```json
{"sum": 15, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
```

---

## Generic Plugin Event Markers (Optional - for forensics)

These event markers are used by plugins for immediate error reporting with JSON context. They are OPTIONAL.

| Telemetry Marker Name | Category | Payload Fields | Description |
|-----------------------|----------|----------------|-------------|
| `AppGwPluginApiError_split` | API Errors | `plugin` (string) - plugin name (e.g., "Badger", "OttServices")<br>`api` (string) - name of the API that failed<br>`error` (string) - error code or description | Reports individual API failures with full context |
| `AppGwPluginExtServiceError_split` | External Service Errors | `plugin` (string) - plugin name<br>`service` (string) - name of external service<br>`error` (string) - error code or description | Reports individual external service failures with full context |

## Plugin Latency Metrics (Required - for monitoring)

These metrics use composite naming with plugin and API/service names.

| Telemetry Marker Pattern | Unit | Description |
|--------------------------|------|-------------|
| `AppGw<Plugin>_<ApiName>_Latency_split` | ms | API call latency (e.g., `AppGwBadger_GetDeviceSessionId_Latency_split`, `AppGwOttServices_GetAppPermissions_Latency_split`) |
| `AppGw<Plugin>_<ServiceName>_Latency_split` | ms | External service call latency (e.g., `AppGwBadger_OttServices_Latency_split`, `AppGwOttServices_ThorPermissionService_Latency_split`) |

### Example Event Payloads (Optional)

**API Error Event (JSON format):**
```json
{"plugin":"Badger","api":"GetSettings","error":"TIMEOUT"}
```

**External Service Error Event (JSON format):**
```json
{"plugin":"OttServices","service":"ThorPermissionService","error":"CONNECTION_TIMEOUT"}
```

### Example Metric Payloads (Required)

**API Latency Metric:**
```json
{"sum":1250.5,"count":25,"unit":"ms","reporting_interval_sec":3600}
```
*Marker: `AppGwBadger_GetDeviceSessionId_Latency_split`*

**Error Count Metric:**
```json
{"sum":15,"count":1,"unit":"count","reporting_interval_sec":3600}
```
*Marker: `AppGwApiErrorCount_GetSettings_split`*

**Health Metric:**
```json
{"sum":1543,"count":1,"unit":"count","reporting_interval_sec":3600}
```
*Marker: `AppGwTotalCalls_split`*

---

## Registered Plugin Names

Use these predefined constants for the plugin name field.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_PLUGIN_APPGATEWAY` | `AppGateway` | App Gateway main plugin |
| `AGW_PLUGIN_BADGER` | `Badger` | Badger plugin |
| `AGW_PLUGIN_OTTSERVICES` | `OttServices` | OttServices plugin |

---

## Predefined External Service Names

Use these constants when reporting external service errors for consistency in analytics.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_SERVICE_THOR_PERMISSION` | `ThorPermissionService` | Thor Permission gRPC service used by OttServices for permission checks |
| `AGW_SERVICE_OTT_TOKEN` | `OttTokenService` | OTT Token gRPC service used by OttServices for CIMA token generation |
| `AGW_SERVICE_AUTH` | `AuthService` | Auth Service (COM-RPC) used for SAT/xACT token retrieval |
| `AGW_SERVICE_AUTH_METADATA` | `AuthMetadataService` | Auth metadata collection service (token, deviceId, accountId, partnerId) |
| `AGW_SERVICE_OTT_SERVICES` | `OttServices` | OttServices interface (COM-RPC) used by Badger to access OTT permissions |
| `AGW_SERVICE_LAUNCH_DELEGATE` | `LaunchDelegate` | Launch Delegate interface (COM-RPC) for app session management |
| `AGW_SERVICE_LIFECYCLE_DELEGATE` | `LifecycleDelegate` | Lifecycle Delegate for device session management |
| `AGW_SERVICE_PERMISSION` | `PermissionService` | AppGateway internal permission checking service |
| `AGW_SERVICE_AUTHENTICATION` | `AuthenticationService` | AppGateway WebSocket authentication service |

---

## Predefined Error Codes

Use these constants when reporting errors for consistency in analytics.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_ERROR_INTERFACE_UNAVAILABLE` | `INTERFACE_UNAVAILABLE` | COM-RPC interface is not available |
| `AGW_ERROR_INTERFACE_NOT_FOUND` | `INTERFACE_NOT_FOUND` | COM-RPC interface could not be found |
| `AGW_ERROR_CLIENT_NOT_INITIALIZED` | `CLIENT_NOT_INITIALIZED` | Service client not initialized |
| `AGW_ERROR_CONNECTION_REFUSED` | `CONNECTION_REFUSED` | Connection to service was refused |
| `AGW_ERROR_CONNECTION_TIMEOUT` | `CONNECTION_TIMEOUT` | Connection to service timed out |
| `AGW_ERROR_TIMEOUT` | `TIMEOUT` | Operation timed out |
| `AGW_ERROR_PERMISSION_DENIED` | `PERMISSION_DENIED` | Permission check failed |
| `AGW_ERROR_INVALID_RESPONSE` | `INVALID_RESPONSE` | Service returned invalid response |
| `AGW_ERROR_INVALID_REQUEST` | `INVALID_REQUEST` | Request parameters were invalid |
| `AGW_ERROR_NOT_AVAILABLE` | `NOT_AVAILABLE` | Service or resource not available |
| `AGW_ERROR_FETCH_FAILED` | `FETCH_FAILED` | Failed to fetch data from service |
| `AGW_ERROR_UPDATE_FAILED` | `UPDATE_FAILED` | Failed to update data in service |
| `AGW_ERROR_COLLECTION_FAILED` | `COLLECTION_FAILED` | Failed to collect metadata |
| `AGW_ERROR_GENERAL` | `GENERAL_ERROR` | General/unspecified error |

---

## Metric Units

Use these standard units when reporting metrics via `RecordTelemetryMetric()`.

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_UNIT_MILLISECONDS` | `ms` | Time in milliseconds |
| `AGW_UNIT_SECONDS` | `sec` | Time in seconds |
| `AGW_UNIT_COUNT` | `count` | Count/quantity |
| `AGW_UNIT_BYTES` | `bytes` | Size in bytes |
| `AGW_UNIT_KILOBYTES` | `KB` | Size in kilobytes |
| `AGW_UNIT_MEGABYTES` | `MB` | Size in megabytes |
| `AGW_UNIT_KBPS` | `kbps` | Bitrate in kilobits per second |
| `AGW_UNIT_MBPS` | `Mbps` | Bitrate in megabits per second |
| `AGW_UNIT_PERCENT` | `percent` | Percentage value |

---

## Data Format

App Gateway supports two output formats for telemetry data, configurable via the `TelemetryFormat` enum.

### JSON Format (Default)
Self-describing format with field names included. **This is the standard format used for all metrics.**

**Advantages:**
- Self-documenting - field names are included
- Easy to parse and debug
- Standard format supported by all T2 backends

**Example - Simple Metric:**
```json
{"sum":1250,"count":1,"unit":"ms","reporting_interval_sec":0}
```

**Example - Health Stats (multiple fields):**
```json
{"websocket_connections":12,"total_calls":1543,"successful_calls":1520,"failed_calls":23,"reporting_interval_sec":3600}
```

### COMPACT Format
Comma-separated values that extract only the values from JSON, omitting field names. **This format reduces payload size but requires the receiver to know the field order.**

**Current Status:** Fully supported in the implementation via `TelemetryFormat::COMPACT` configuration. Currently, **JSON format is the active setting** for all deployments.

**Advantages:**
- Smaller payload size (reduces bandwidth)
- Faster serialization
- Useful for high-frequency telemetry in bandwidth-constrained environments

**Disadvantages:**
- Requires external schema knowledge to interpret
- Less human-readable
- Field order must be strictly maintained

**Example - Simple Values:**
```
12,1543,1520,23,3600
```
*Represents: websocket_connections=12, total_calls=1543, successful_calls=1520, failed_calls=23, reporting_interval_sec=3600*

**To Enable:** Set `TelemetryFormat::COMPACT` in the AppGateway configuration if bandwidth optimization is required.

---

## Future Enhancement: Batch Metric Reporting

**Note:** This capability is **not currently implemented**. The current architecture sends each metric individually with its own unique marker.

### Proposed: Array-Based Batch Reporting

**Concept:** Send multiple related metrics in a single payload to reduce the number of T2 events.

**Example - Batch API Error Counts (COMPACT format):**
```
3600,(GetData,5),(SetConfig,2),(LoadResource,1)
```

**Interpretation:**
- `3600` - reporting_interval_sec (1 hour)
- `(GetData,5)` - API "GetData" had 5 errors
- `(SetConfig,2)` - API "SetConfig" had 2 errors  
- `(LoadResource,1)` - API "LoadResource" had 1 error

**Example - Batch API Error Counts (JSON format):**
```json
{
  "items": [
    {"api": "GetData", "count": 5},
    {"api": "SetConfig", "count": 2},
    {"api": "LoadResource", "count": 1}
  ],
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

### Why Not Currently Implemented

**Current Design Benefits:**
- Individual metrics provide better T2 backend support for per-API alerting and trending
- Unique markers per statistic enable clearer analytics and dashboards
- Simpler integration with existing T2 monitoring infrastructure
- Each metric can be independently queried and visualized

**Trade-offs:**
- More T2 events generated (higher bandwidth)
- More individual markers to manage in T2 backend

### Implementation Requirements

If batch reporting becomes necessary, it would require:

1. **New API Method:**
   ```cpp
   Core::hresult RecordTelemetryBatch(
       const char* markerName,
       const std::vector<std::pair<std::string, double>>& items,
       const char* unit
   );
   ```

2. **Backend Changes:**
   - T2 backend must parse array payloads
   - Extract individual metrics for alerting/trending
   - Map array items to individual time series

3. **Use Cases:**
   - Reporting all plugin bootstrap times together
   - Sending cache statistics bundle (hits, misses, evictions)
   - Collecting all service health checks in one payload

**Future Consideration:** Evaluate if bandwidth savings justify the added complexity in T2 backend integration and loss of per-metric alerting simplicity.

---

## Adding Telemetry to a New Plugin

When adding telemetry to a new plugin, follow these steps:

### 1. Add Plugin Name Constant

In `AppGatewayTelemetryMarkers.h`, add a constant for your plugin name:

```cpp
#define AGW_PLUGIN_MYPLUGIN       "MyPlugin"
```

### 2. Include Required Headers

In your plugin source file:

```cpp
#include <helpers/UtilsAppGatewayTelemetry.h>
```

### 3. Initialize Telemetry Client

In your plugin class, define the telemetry client:

```cpp
// In header or implementation
AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_MYPLUGIN)
```

### 4. Report Telemetry Events

Use the simplified macros - no need to specify markers:

```cpp
// Report API error
AGW_REPORT_API_ERROR("GetSettings", AGW_ERROR_TIMEOUT);

// Report external service error
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_MY_SERVICE, "CONNECTION_FAILED");
```

### 5. Report Telemetry Metrics

Report latency and custom metrics for statistical aggregation:

```cpp
// Option 1: Auto-track API latency using scoped timer (RAII)
Core::hresult MyPlugin::GetSettings(const JsonObject& parameters, JsonObject& response)
{
    AGW_SCOPED_API_TIMER(timer, "GetSettings");
    
    // Your API implementation here
    // Timer automatically reports latency when it goes out of scope
    // Generates metric: AppGwMyPlugin_GetSettings_Latency_split
    
    return Core::ERROR_NONE;
}

// Option 2: Manually report API latency
auto startTime = Core::Time::Now().Ticks();
Core::hresult result = PerformApiCall();
auto latencyMs = (Core::Time::Now().Ticks() - startTime) / Core::Time::TicksPerMillisecond;
AGW_REPORT_API_LATENCY("GetAppPermissions", latencyMs);
// Generates metric: AppGwMyPlugin_GetAppPermissions_Latency_split

// Option 3: Report external service latency
auto serviceStart = Core::Time::Now().Ticks();
auto grpcResult = myGrpcClient->CallService();
auto serviceLatencyMs = (Core::Time::Now().Ticks() - serviceStart) / Core::Time::TicksPerMillisecond;
AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_MY_EXTERNAL_SERVICE, serviceLatencyMs);
// Generates metric: AppGwMyPlugin_MyExternalService_Latency_split

// Option 4: Report custom metrics (optional)
AGW_REPORT_METRIC("AppGwMyPluginCacheHitRate_split", 85.5, "percent");
AGW_REPORT_METRIC("AppGwMyPluginQueueDepth_split", 42, "count");
```

### 6. Add Plugin-Specific Service Names (Optional)

If your plugin uses specific external services, add constants:

```cpp
#define AGW_SERVICE_MY_EXTERNAL_SERVICE   "MyExternalService"
```

Then update this documentation to include the new service constant.

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.1 | 2026-01-31 | - | Refactored to generic category-based markers |
| 1.0 | 2026-01-31 | - | Initial release |
