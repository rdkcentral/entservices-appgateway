# App Gateway Telemetry Markers Reference

This document is a **quick reference for all T2 telemetry markers** used in the App Gateway ecosystem.

**For integration steps, macro usage, and error codes, see:** [AppGatewayTelemetryIntegrationGuide.md](./AppGatewayTelemetryIntegrationGuide.md)

---

## Marker Naming Convention

All AppGateway telemetry markers follow this pattern:

- **Prefix:** `AppGw`
- **Style:** CamelCase
- **Suffix:** `_split` (indicates structured format for T2)

### Marker Types

**1. Generic Event Markers** (Optional - for immediate forensics)
- Pattern: `AppGwPlugin<Category>_split`
- Plugin name included in JSON payload
- Example: `AppGwPluginApiError_split` with `{"plugin":"Badger","api":"GetSettings","error":"TIMEOUT"}`

**2. Internal Metrics** (Aggregated by AppGateway)
- Pattern: `AppGw<MetricName>_split`
- Examples: `AppGwBootstrapDuration_split`, `AppGwTotalCalls_split`

**3. Error Count Metrics** (Per-API/Service)
- Pattern: `AppGw<ErrorType>Count_<Name>_split`
- Examples: `AppGwApiErrorCount_GetSettings_split`, `AppGwExtServiceErrorCount_ThorPermissionService_split`

**4. Latency Metrics** (Plugin-specific)
- Pattern: `AppGw<Plugin>_<Api/Service>_Latency_split`
- Examples: `AppGwBadger_GetSettings_Latency_split`, `AppGwOttServices_ThorPermissionService_Latency_split`

---

## Predefined Markers Table

### Internal AppGateway Metrics

| Marker Name | Reporting | Unit | Description |
|-------------|-----------|------|-------------|
| `AppGwBootstrapDuration_split` | Once (startup) | ms | Total time to bootstrap all plugins |
| `AppGwBootstrapPluginCount_split` | Once (startup) | count | Number of plugins successfully loaded |
| `AppGwWebSocketConnections_split` | Periodic | count | Active WebSocket connections |
| `AppGwTotalCalls_split` | Periodic | count | Total API calls in reporting period |
| `AppGwSuccessfulCalls_split` | Periodic | count | Successful API calls |
| `AppGwFailedCalls_split` | Periodic | count | Failed API calls |

### Error Count Metrics (Per-API/Service)

| Marker Pattern | Example | Reporting | Unit | Description |
|----------------|---------|-----------|------|-------------|
| `AppGwApiErrorCount_<ApiName>_split` | `AppGwApiErrorCount_GetSettings_split` | Periodic | count | Error count for specific API |
| `AppGwExtServiceErrorCount_<ServiceName>_split` | `AppGwExtServiceErrorCount_ThorPermissionService_split` | Periodic | count | Error count for specific external service |

### Generic Event Markers (Optional)

| Marker Name | Payload Fields | Description |
|-------------|----------------|-------------|
| `AppGwPluginApiError_split` | `plugin`, `api`, `error` | Individual API error with full context |
| `AppGwPluginExtServiceError_split` | `plugin`, `service`, `error` | Individual service error with full context |

---

## Plugin-Specific Latency Metrics

These metrics use composite naming with plugin and API/service names.

### Marker Pattern

| Pattern | Example | Unit | Description |
|---------|---------|------|-------------|
| `AppGw<Plugin>_<ApiName>_Latency_split` | `AppGwBadger_GetDeviceSessionId_Latency_split` | ms | API call latency for a plugin |
| `AppGw<Plugin>_<ServiceName>_Latency_split` | `AppGwOttServices_ThorPermissionService_Latency_split` | ms | Service call latency for a plugin |

### Examples by Plugin

**Badger Plugin:**
- `AppGwBadger_GetSettings_Latency_split`
- `AppGwBadger_GetDeviceSessionId_Latency_split`
- `AppGwBadger_GetAppPermissions_Latency_split`
- `AppGwBadger_OttServices_Latency_split` (external service)

**OttServices Plugin:**
- `AppGwOttServices_GetAppPermissions_Latency_split`
- `AppGwOttServices_ThorPermissionService_Latency_split` (gRPC)
- `AppGwOttServices_OttTokenService_Latency_split` (gRPC)

---

## Metric Payload Formats

### Basic Metric (Error Count, Health Stats)

```json
{
  "sum": 15,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Field Descriptions:**
- `sum` - The aggregated value (count of errors, number of calls, etc.)
- `count` - Number of data points (usually `1` for aggregated stats)
- `unit` - Unit of measurement (see [Units](#supported-units))
- `reporting_interval_sec` - Reporting period in seconds (`3600` = 1 hour, `0` = one-time)

### Extended Latency Metric

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

**Additional Fields:**
- `min` - Minimum (fastest) latency observed
- `max` - Maximum (slowest) latency observed
- `avg` - Average latency (`sum / count`)

### Event Payload (API Error)

```json
{
  "plugin": "Badger",
  "api": "GetSettings",
  "error": "TIMEOUT"
}
```

### Event Payload (Service Error)

```json
{
  "plugin": "OttServices",
  "service": "ThorPermissionService",
  "error": "CONNECTION_TIMEOUT"
}
```

---

## Supported Units

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_UNIT_MILLISECONDS` | `"ms"` | Time in milliseconds |
| `AGW_UNIT_SECONDS` | `"sec"` | Time in seconds |
| `AGW_UNIT_COUNT` | `"count"` | Count/quantity |
| `AGW_UNIT_BYTES` | `"bytes"` | Size in bytes |
| `AGW_UNIT_KILOBYTES` | `"KB"` | Size in kilobytes |
| `AGW_UNIT_MEGABYTES` | `"MB"` | Size in megabytes |
| `AGW_UNIT_KBPS` | `"kbps"` | Bitrate in kilobits/sec |
| `AGW_UNIT_MBPS` | `"Mbps"` | Bitrate in megabits/sec |
| `AGW_UNIT_PERCENT` | `"percent"` | Percentage value |

---

## Registered Plugin Names

**Defined in header:** `AppGatewayTelemetryMarkers.h`

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_PLUGIN_APPGATEWAY` | `"AppGateway"` | App Gateway main plugin |
| `AGW_PLUGIN_BADGER` | `"Badger"` | Badger plugin |
| `AGW_PLUGIN_OTTSERVICES` | `"OttServices"` | OttServices plugin |
| `AGW_PLUGIN_FBADVERTISING` | `"FbAdvertising"` | FbAdvertising plugin |
| `AGW_PLUGIN_FBDISCOVERY` | `"FbDiscovery"` | FbDiscovery plugin |
| `AGW_PLUGIN_FBENTOS` | `"FbEntos"` | FbEntos plugin |
| `AGW_PLUGIN_FBMETRICS` | `"FbMetrics"` | FbMetrics plugin |
| `AGW_PLUGIN_FBPRIVACY` | `"FbPrivacy"` | FbPrivacy plugin |

---

## Predefined External Service Names

**Defined in header:** `AppGatewayTelemetryMarkers.h`

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_SERVICE_THOR_PERMISSION` | `"ThorPermissionService"` | Thor Permission gRPC service |
| `AGW_SERVICE_OTT_TOKEN` | `"OttTokenService"` | OTT Token gRPC service |
| `AGW_SERVICE_AUTH` | `"AuthService"` | Auth Service (COM-RPC) |
| `AGW_SERVICE_AUTH_METADATA` | `"AuthMetadataService"` | Auth metadata collection |
| `AGW_SERVICE_OTT_SERVICES` | `"OttServices"` | OttServices interface (COM-RPC) |
| `AGW_SERVICE_LAUNCH_DELEGATE` | `"LaunchDelegate"` | Launch Delegate interface |
| `AGW_SERVICE_LIFECYCLE_DELEGATE` | `"LifecycleDelegate"` | Lifecycle Delegate interface |
| `AGW_SERVICE_PERMISSION` | `"PermissionService"` | Internal permission checking |
| `AGW_SERVICE_AUTHENTICATION` | `"AuthenticationService"` | WebSocket authentication |

---

## How to Use These Markers

**Do not reference markers directly in your code.** Use the helper macros from `UtilsAppGatewayTelemetry.h` instead:

- `AGW_REPORT_API_ERROR(context, api, error)` - Uses `AppGwPluginApiError_split` internally
- `AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, service, error)` - Uses `AppGwPluginExtServiceError_split` internally
- `AGW_REPORT_API_LATENCY(context, api, latencyMs)` - Generates `AppGw<Plugin>_<Api>_Latency_split`
- `AGW_REPORT_SERVICE_LATENCY(context, service, latencyMs)` - Generates `AppGw<Plugin>_<Service>_Latency_split`

All macros now require a `context` parameter (type: `Exchange::GatewayContext`) for request correlation tracking.

**For complete integration instructions, see:**
[AppGatewayTelemetryIntegrationGuide.md](./AppGatewayTelemetryIntegrationGuide.md)

---

## Adding a New Plugin

To add telemetry for a new plugin:

1. **Add plugin name constant** in `AppGatewayTelemetryMarkers.h`:
   ```cpp
   #define AGW_PLUGIN_MYPLUGIN "MyPlugin"
   ```

2. **Use existing generic markers** - no need to create plugin-specific ones
3. **Follow the Integration Guide** for macro usage

The plugin name will automatically be included in payloads and composite metric names.

---

## Data Format

**Current Format:** JSON (default)
```json
{"sum": 1250, "count": 1, "unit": "ms", "reporting_interval_sec": 3600}
```

**Supported Alternative:** COMPACT (comma-separated values)
```
1250,1,ms,3600
```

**Note:** COMPACT format is fully implemented but not currently enabled in production. JSON is the standard format used for all deployments.

---

## Future Enhancement: Batch Reporting

**Status:** Not currently implemented.

Current design sends each metric individually. Future enhancement may allow batch reporting of related metrics in a single payload to reduce T2 event count.

**See Integration Guide for details on proposed batch reporting format.**

---

## See Also

- **[AppGatewayTelemetryIntegrationGuide.md](./AppGatewayTelemetryIntegrationGuide.md)** - How to integrate telemetry (macros, error codes, examples)
- **[AppGatewayTelemetry_Architecture.md](./AppGatewayTelemetry_Architecture.md)** - System architecture and design patterns
- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual flows for telemetry scenarios
- **`AppGatewayTelemetryMarkers.h`** - Header file with all constant definitions

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 2.0 | 2026-02-10 | Restructured as marker reference only; moved integration details to Integration Guide |
| 1.1 | 2026-01-31 | Refactored to generic category-based markers |
| 1.0 | 2026-01-31 | Initial release |
