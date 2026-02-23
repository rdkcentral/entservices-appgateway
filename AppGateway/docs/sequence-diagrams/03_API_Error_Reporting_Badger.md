# Scenario 3: API Error Reporting (Badger Plugin Example)

## Overview

This sequence diagram illustrates how the Badger plugin reports API errors to App Gateway via COM-RPC using the generic marker system. The example shows `GetDeviceSessionId` API failing due to LifecycleDelegate unavailability.

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant AppGw as AppGateway
    participant Badger as Badger Plugin
    participant Lifecycle as LifecycleDelegate
    participant TelemetryClient as TelemetryClient (Badger)
    participant Telemetry as AppGatewayTelemetry
    participant T2 as T2 Telemetry Server
    
    Note over Client,T2: API Call with Error
    
    Client->>AppGw: GetDeviceSessionId(appId)
    AppGw->>Badger: GetDeviceSessionId(context, appId)
    activate Badger
    
    Note over Badger: Create scoped API timer
    Badger->>Badger: AGW_TRACK_API_CALL(timer, "GetDeviceSessionId")
    
    Badger->>Badger: Get LifecycleDelegate
    Note over Badger: lifecycle = nullptr<br/>(Delegate not available)
    
    Badger->>Badger: LOGERR("LifecycleDelegate not available")
    
    Note over Badger: Report external service error
    Badger->>TelemetryClient: AGW_REPORT_EXTERNAL_SERVICE_ERROR(<br/>AGW_SERVICE_LIFECYCLE_DELEGATE,<br/>AGW_ERROR_NOT_AVAILABLE)
    activate TelemetryClient
    
    TelemetryClient->>TelemetryClient: RecordExternalServiceError(service, error)
    
    Note over TelemetryClient: Build JSON payload:<br/>{"plugin": "Badger",<br/> "service": "LifecycleDelegate",<br/> "error": "NOT_AVAILABLE"}
    
    TelemetryClient->>Telemetry: RecordTelemetryEvent(<br/>context,<br/>"AppGwPluginExtServiceError_split",<br/>eventData) [COM-RPC]
    activate Telemetry
    
    Note over Telemetry: Store in cache:<br/>service_errors["LifecycleDelegate"]++
    
    Telemetry-->>TelemetryClient: Core::ERROR_NONE
    deactivate Telemetry
    
    TelemetryClient-->>Badger: 
    deactivate TelemetryClient
    
    Note over Badger: Mark timer as failed
    Badger->>Badger: timer.SetFailed(AGW_ERROR_NOT_AVAILABLE)
    
    Badger-->>AppGw: "app_session_id.not.set"
    deactivate Badger
    
    Note over Badger: On timer destruction:<br/>Reports failed API latency
    
    AppGw-->>Client: Error Response
    
    Note over Telemetry: ... Time passes (1 hour) ...
    
    Telemetry->>Telemetry: OnTimerExpired()
    activate Telemetry
    
    Telemetry->>Telemetry: SendApiErrorStats()
    
    Note over Telemetry: Send each API error count<br/>as individual METRIC
    
    Telemetry->>T2: t2_event_s("AppGwApiErrorCount_GetDeviceSessionId_split", payload)
    Note over T2: Payload: {"sum": 15, "count": 1,<br/>"unit": "count", "reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>Telemetry: ResetApiErrorStats()
    Note over Telemetry: Clear error counters<br/>for next interval
    
    deactivate Telemetry
```

## Key Components

| Component | Responsibility |
|-----------|---------------|
| **WebSocket Client** | Initiates API call to AppGateway |
| **AppGateway** | Routes request to Badger plugin |
| **Badger Plugin** | Attempts to get device session ID, encounters error |
| **LifecycleDelegate** | External service (unavailable in this scenario) |
| **TelemetryClient** | Helper class in Badger for telemetry reporting |
| **AppGatewayTelemetry** | Aggregates errors and reports to T2 |
| **T2 Telemetry Server** | Receives aggregated error statistics |

## Error Flow

1. **API Call**: Client requests device session ID via AppGateway
2. **Service Check**: Badger attempts to get LifecycleDelegate
3. **Error Detection**: LifecycleDelegate is unavailable
4. **Error Logging**: Badger logs error with context
5. **Telemetry Reporting**: Report external service error via `AGW_REPORT_EXTERNAL_SERVICE_ERROR`
6. **COM-RPC Call**: TelemetryClient calls AppGatewayTelemetry via COM-RPC
7. **Error Aggregation**: AppGatewayTelemetry increments error counter
8. **Timer Tracking**: Scoped timer marks API call as failed
9. **Client Response**: Return default value to client
10. **Periodic Reporting**: Aggregated errors sent to T2 every hour

## Generic Marker System

### Event Marker (Immediate - Individual Occurrences)
**Marker:** `AppGwPluginExtServiceError_split`
**Payload:**
```json
{
  "plugin": "Badger",
  "service": "LifecycleDelegate",
  "error": "NOT_AVAILABLE"
}
```

### Aggregated Error Count Metrics (Periodic - Per API)
**Metric Name Pattern:** `AppGwApiErrorCount_<ApiName>_split`

**Example Metric:** `AppGwApiErrorCount_GetDeviceSessionId_split`
**Payload:**
```json
{
  "sum": 15,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Compact Format:**
```
AppGwApiErrorCount_GetDeviceSessionId_split: 15,1,count,3600
```

## Predefined Constants Used

```cpp
// From AppGatewayTelemetryMarkers.h
#define AGW_PLUGIN_BADGER                "Badger"
#define AGW_SERVICE_LIFECYCLE_DELEGATE   "LifecycleDelegate"
#define AGW_ERROR_NOT_AVAILABLE          "NOT_AVAILABLE"
#define AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR  "AppGwPluginExtServiceError_split"
```

## Benefits of Generic Markers

- **Single Marker per Category**: `AppGwPluginExtServiceError_split` used by all plugins for immediate error reporting
- **Individual Metrics per API**: Each failing API gets its own metric for trending: `agw_API_Error_Count_<ApiName>`
- **Plugin Name in Payload**: Events include plugin context for filtering
- **No Marker Duplication**: No need for `AGW_MARKER_BADGER_API_ERROR`
- **Consistent Reporting**: Same pattern across all plugins
- **Scalable**: Adding new APIs automatically creates new metrics
- **Statistical Aggregation**: Metrics support sum, count, min, max, avg

## Notes

- Event reporting (immediate) captures WHAT happened with full context
- Metric reporting (periodic) captures HOW MANY times it happened
- Error counts tracked per API name (e.g., "GetDeviceSessionId", "AuthorizeDataField")
- Each API that experiences errors gets a unique metric in T2
- Metrics reset after each reporting interval (default: 1 hour)
- Scoped timer automatically tracks failed API latency
- COM-RPC enables cross-plugin telemetry communication
