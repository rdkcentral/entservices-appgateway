# Scenario 4: External Service Error Reporting (OttServices Plugin Example)

## Overview

This sequence diagram illustrates how the OttServices plugin reports external service errors to App Gateway via COM-RPC. The example shows `GetAppPermissions` failing when the ThorPermissionService gRPC client is not initialized.

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant AppGw as AppGateway
    participant OttServices as OttServices Plugin
    participant Thor as ThorPermissionService (gRPC)
    participant TelemetryClient as TelemetryClient (OttServices)
    participant Telemetry as AppGatewayTelemetry
    participant T2 as T2 Telemetry Server
    
    Note over Client,T2: External Service Call with Error
    
    Client->>AppGw: GetAppPermissions(appId, forceNew)
    AppGw->>OttServices: GetAppPermissions(appId, forceNew, permissions)
    activate OttServices
    
    Note over OttServices: Create scoped API timer
    OttServices->>OttServices: AGW_TRACK_API_CALL(timer, "GetAppPermissions")
    
    OttServices->>OttServices: Check if _perms initialized
    Note over OttServices: _perms == nullptr<br/>(Client not initialized)
    
    OttServices->>OttServices: LOGERR("PermissionsClient not initialized")
    
    Note over OttServices: Report external service error
    OttServices->>TelemetryClient: AGW_REPORT_EXTERNAL_SERVICE_ERROR(<br/>AGW_SERVICE_THOR_PERMISSION,<br/>"CLIENT_NOT_INITIALIZED")
    activate TelemetryClient
    
    TelemetryClient->>TelemetryClient: RecordExternalServiceError(service, error)
    
    Note over TelemetryClient: Build JSON payload:<br/>{"plugin": "OttServices",<br/> "service": "ThorPermissionService",<br/> "error": "CLIENT_NOT_INITIALIZED"}
    
    TelemetryClient->>Telemetry: RecordTelemetryEvent(<br/>context,<br/>"AppGwPluginExtServiceError_split",<br/>eventData) [COM-RPC]
    activate Telemetry
    
    Note over Telemetry: Store in cache:<br/>service_errors["ThorPermissionService"]++
    
    Telemetry-->>TelemetryClient: Core::ERROR_NONE
    deactivate Telemetry
    
    TelemetryClient-->>OttServices: 
    deactivate TelemetryClient
    
    Note over OttServices: Mark timer as failed
    OttServices->>OttServices: timer.SetFailed("CLIENT_NOT_INITIALIZED")
    
    OttServices-->>AppGw: Core::ERROR_UNAVAILABLE
    deactivate OttServices
    
    Note over OttServices: On timer destruction:<br/>Reports failed API latency
    
    AppGw-->>Client: Error Response
    
    Note over Telemetry: ... Time passes (1 hour) ...
    
    Telemetry->>Telemetry: OnTimerExpired()
    activate Telemetry
    
    Telemetry->>Telemetry: SendExternalServiceErrorStats()
    
    Note over Telemetry: Send each service error count<br/>as individual METRIC
    
    Telemetry->>T2: t2_event_s("AppGwExtServiceErrorCount_ThorPermissionService_split", payload)
    Note over T2: Payload: {"sum": 42, "count": 1,<br/>"unit": "count", "reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>T2: t2_event_s("AppGwExtServiceErrorCount_OttTokenService_split", payload)
    Note over T2: Payload: {"sum": 8, "count": 1,<br/>"unit": "count", "reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>Telemetry: ResetExternalServiceErrorStats()
    Note over Telemetry: Clear error counters for next interval
    
    deactivate Telemetry
```

## Key Components

| Component | Responsibility |
|-----------|---------------|
| **WebSocket Client** | Initiates permission check via AppGateway |
| **AppGateway** | Routes request to OttServices plugin |
| **OttServices Plugin** | Manages app permissions via ThorPermissionService |
| **ThorPermissionService** | External gRPC service for permission checks |
| **TelemetryClient** | Helper class in OttServices for telemetry reporting |
| **AppGatewayTelemetry** | Aggregates errors from all plugins and reports to T2 |
| **T2 Telemetry Server** | Receives aggregated error statistics |

## Error Flow

1. **API Call**: Client requests app permissions via AppGateway
2. **Service Check**: OttServices checks if ThorPermissionService client is initialized
3. **Error Detection**: gRPC client is not initialized
4. **Error Logging**: OttServices logs error with context
5. **Telemetry Reporting**: Report external service error via `AGW_REPORT_EXTERNAL_SERVICE_ERROR`
6. **COM-RPC Call**: TelemetryClient calls AppGatewayTelemetry via COM-RPC
7. **Error Aggregation**: AppGatewayTelemetry increments error counter for "ThorPermissionService"
8. **Timer Tracking**: Scoped timer marks API call as failed
9. **Client Response**: Return error code to client
10. **Periodic Reporting**: Aggregated errors from all plugins sent to T2 every hour

## Generic Marker System

### Event Marker (Immediate - per plugin)
**Marker:** `AppGwPluginExtServiceError_split`
**Payload (OttServices):**
```json
{
  "plugin": "OttServices",
  "service": "ThorPermissionService",
  "error": "CLIENT_NOT_INITIALIZED"
}
```

**Payload (Badger - different plugin, same marker):**
```json
{
  "plugin": "Badger",
  "service": "OttServices",
  "error": "INTERFACE_UNAVAILABLE"
}
```

### Aggregated Error Count Metrics (Periodic - Per Service)
**Metric Name Pattern:** `AppGwExtServiceErrorCount_<ServiceName>_split`

**Example Metrics:**

`AppGwExtServiceErrorCount_ThorPermissionService_split`
```json
{
  "sum": 42,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

`AppGwExtServiceErrorCount_OttTokenService_split`
```json
{
  "sum": 8,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Compact Format:**
```
AppGwExtServiceErrorCount_ThorPermissionService_split: 42,1,count,3600
AppGwExtServiceErrorCount_OttTokenService_split: 8,1,count,3600
```

## Predefined Constants Used

```cpp
// From AppGatewayTelemetryMarkers.h
#define AGW_PLUGIN_OTTSERVICES                "OttServices"
#define AGW_SERVICE_THOR_PERMISSION           "ThorPermissionService"
#define AGW_SERVICE_OTT_TOKEN                 "OttTokenService"
#define AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR   "AppGwPluginExtServiceError_split"
```

## Multi-Plugin Aggregation

Each external service that experiences errors gets its own metric with error count aggregated across all plugins:

| Metric Name | Error Count (1 hour) | Contributing Plugins |
|-------------|---------------------|---------------------|
| `AppGwExtServiceErrorCount_ThorPermissionService_split` | 42 | OttServices |
| `AppGwExtServiceErrorCount_OttTokenService_split` | 8 | OttServices |
| `AppGwExtServiceErrorCount_LifecycleDelegate_split` | 15 | Badger |
| `AppGwExtServiceErrorCount_OttServices_split` | 3 | Badger |

Each metric has its own T2 marker for independent trending and alerting.

## Configuration

- **Reporting Interval**: Default 3600 seconds (1 hour), configurable
- **Format**: Individual numeric metrics (one per service)
- **Data Type**: METRIC (for statistical aggregation)
- **Reset**: Error counters reset after each periodic report
- **Granularity**: Per-service error counts (aggregated from all plugins)

## Notes

- **Service-level tracking**: Errors tracked by service name with individual metrics
- **Cross-plugin visibility**: Metrics aggregate errors from all plugins reporting to same service
- **Immediate vs Aggregated**: Individual events (optional) for forensics, metrics (required) for monitoring
- **Generic markers**: Single event marker `AppGwPluginExtServiceError_split` used by all plugins
- **Unique metrics**: Each service gets unique metric name for trending: `AppGwExtServiceErrorCount_<ServiceName>_split`
- **Payload differentiation**: Event payload includes plugin name for filtering
- **gRPC services**: Common pattern for OttServices to report gRPC client errors
- **Alerting**: Can set thresholds on individual service metrics (e.g., ThorPermissionService errors > 50)
