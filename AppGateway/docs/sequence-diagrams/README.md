# App Gateway Telemetry Sequence Diagrams

This directory contains detailed sequence diagrams for all App Gateway telemetry scenarios, illustrating the complete flow from event occurrence to T2 reporting.

## Diagrams Overview

| Diagram | Scenario | Key Plugins | Description |
|---------|----------|-------------|-------------|
| [01_Bootstrap_Time_Tracking.md](./01_Bootstrap_Time_Tracking.md) | Bootstrap Time | AppGateway | Measures total time to initialize all plugins during system startup |
| [02_Health_Stats_Reporting.md](./02_Health_Stats_Reporting.md) | Health Stats | AppGateway, AppGatewayResponder | Tracks WebSocket connections and API call statistics, reported periodically |
| [03_API_Error_Reporting_Badger.md](./03_API_Error_Reporting_Badger.md) | API Errors | Badger | Example of reporting API failures using generic markers via COM-RPC |
| [04_External_Service_Error_OttServices.md](./04_External_Service_Error_OttServices.md) | External Service Errors | OttServices | Example of reporting gRPC service errors with multi-plugin aggregation |
| [05_Metric_Latency_Tracking.md](./05_Metric_Latency_Tracking.md) | Latency Metrics | Badger, OttServices | API and service latency tracking using scoped timers and manual timing |

## Telemetry Architecture

### Four Core Scenarios

1. **Bootstrap Time Tracking**
   - Measured once during AppGateway initialization
   - Reports individual metrics: `AppGwBootstrapDuration_split` (ms) and `AppGwBootstrapPluginCount_split` (count)
   - Data Type: METRIC (for trending)

2. **Health Stats Reporting**
   - Continuous tracking of WebSocket connections and API calls
   - Periodic reporting as individual metrics (default: every hour)
   - Metrics: `AppGwWebSocketConnections_split`, `AppGwTotalCalls_split`, `AppGwSuccessfulCalls_split`, `AppGwFailedCalls_split`
   - Data Type: METRIC (for statistical aggregation)

3. **API Error Reporting**
   - Plugins report API failures via COM-RPC
   - Optional immediate events with plugin name in payload: `AppGwPluginApiError_split`
   - Periodic error count metrics per API: `AppGwApiErrorCount_<ApiName>_split`
   - Data Type: EVENT (immediate, optional) + METRIC (periodic, required)

4. **External Service Error Reporting**
   - Plugins report external service (gRPC, COM-RPC) failures
   - Optional immediate events: `AppGwPluginExtServiceError_split`
   - Periodic error count metrics per service: `AppGwExtServiceErrorCount_<ServiceName>_split`
   - Data Type: EVENT (immediate, optional) + METRIC (periodic, required)

5. **Latency Metric Tracking**
   - API latency: Automatic via scoped timers
   - Service latency: Manual timing of external service calls
   - Metrics: `AppGw<PluginName>_<ApiName>_Latency_split`, `AppGw<PluginName>_<ServiceName>_Latency_split`
   - Data Type: METRIC (for statistical aggregation)

### Generic Marker System + Metrics

The telemetry system uses a hybrid approach:

**Events (Optional - for forensics):**
- Generic category-based markers for immediate error reporting
- Single marker per category (e.g., `AppGwPluginApiError_split`)
- Plugin name included in payload data
- Enables detailed debugging of WHAT went wrong

**Metrics (Required - for monitoring):**
- Individual numeric metrics for aggregation and trending
- Unique metric name per API/service (e.g., `AppGwApiErrorCount_GetSettings_split`)
- Periodic reporting (hourly) with statistical data (sum, count, min, max, avg)
- Enables alerting on HOW MANY failures occurred

**Example Event Payload:**
```json
{
  "plugin": "Badger",
  "api": "GetDeviceSessionId",
  "error": "TIMEOUT"
}
```

**Example Metric Payload:**
```json
{
  "sum": 15,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

## COM-RPC Interface

Plugins communicate with AppGatewayTelemetry via the `IAppGatewayTelemetry` interface:

```cpp
// Event reporting (optional - for forensics)
Core::hresult RecordTelemetryEvent(const GatewayContext& context,
                                   const string& eventName,
                                   const string& eventData);

// Metric reporting (required - for monitoring)
Core::hresult RecordTelemetryMetric(const GatewayContext& context,
                                    const string& metricName,
                                    const double metricValue,
                                    const string& metricUnit);
```

**Key Differences:**
- **RecordTelemetryEvent**: Individual occurrences with JSON context (errors, state changes)
- **RecordTelemetryMetric**: Numeric values for statistical aggregation (latencies, counts)

## Helper Macros

Plugins use convenience macros from `UtilsAppGatewayTelemetry.h`:

| Macro | Purpose |
|-------|---------|
| `AGW_TELEMETRY_INIT(service, pluginName)` | Initialize telemetry client |
| `AGW_REPORT_API_ERROR(api, error)` | Report API failure |
| `AGW_REPORT_EXTERNAL_SERVICE_ERROR(service, error)` | Report service failure |
| `AGW_REPORT_API_LATENCY(api, latencyMs)` | Report API latency |
| `AGW_REPORT_SERVICE_LATENCY(service, latencyMs)` | Report service latency |
| `AGW_SCOPED_API_TIMER(var, apiName)` | Auto-track API latency (RAII) |

## Reference Plugins

### Badger Plugin
- Demonstrates automatic API latency tracking with scoped timers
- Shows manual service latency measurement for OttServices calls
- Reports external service errors (LifecycleDelegate, OttServices)
- Example APIs: `GetDeviceSessionId`, `AuthorizeDataField`

### OttServices Plugin
- Demonstrates API latency tracking for permission operations
- Shows service latency for gRPC calls (ThorPermissionService, OttTokenService)
- Reports gRPC client initialization errors
- Example APIs: `GetAppPermissions`, `GetAppCIMAToken`

## Data Flow Summary

```
Plugin API Call
    ↓
Error Detection / Timing
    ↓
AGW_REPORT_* Macro
    ↓
TelemetryClient (Plugin)
    ↓
COM-RPC → IAppGatewayTelemetry
    ↓
AppGatewayTelemetry (Increment Counters)
    ↓
Periodic Timer (1 hour)
    ↓
Send Individual Metrics to T2
  - agw_WebSocket_Connections
  - agw_Total_Calls
  - agw_Successful_Calls
  - agw_Failed_Calls
  - agw_API_Error_Count_<ApiName>
  - agw_External_Service_Error_Count_<ServiceName>
  - agw_<Plugin>_<Api/Service>_Latency
    ↓
T2 Telemetry Server (Aggregation & Alerting)
```

## Mermaid Diagram Rendering

All sequence diagrams use Mermaid syntax for easy rendering in:
- GitHub/GitLab (native support)
- VS Code (with Mermaid extension)
- Documentation sites (Markdown processors with Mermaid support)

To view diagrams:
1. **GitHub/GitLab**: View the .md files directly in the web interface
2. **VS Code**: Install "Markdown Preview Mermaid Support" extension
3. **Local rendering**: Use any Markdown viewer with Mermaid support

## Related Documentation

- [AppGatewayTelemetryIntegrationGuide.md](../AppGatewayTelemetryIntegrationGuide.md) - Developer integration guide
- [AppGatewayTelemetryMarkers.md](../AppGatewayTelemetryMarkers.md) - Complete marker reference
- [AppGatewayTelemetry_Architecture.md](../AppGatewayTelemetry_Architecture.md) - Architecture overview

## Questions?

For implementation questions, refer to:
- Integration guide for step-by-step implementation
- Marker reference for predefined constants
- Badger and OttServices source code for real-world examples
