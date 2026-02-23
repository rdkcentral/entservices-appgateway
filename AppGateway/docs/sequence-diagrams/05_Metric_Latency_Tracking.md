# Scenario 5: API and Service Latency Tracking (Metric Reporting)

## Overview

This sequence diagram illustrates how plugins track API call latency and external service call latency using both automatic (scoped timer) and manual timing methods. Examples from both Badger and OttServices plugins demonstrate `RecordTelemetryMetric` usage.

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant AppGw as AppGateway
    participant Badger as Badger Plugin
    participant OttSvc as OttServices (COM-RPC)
    participant TelemetryClient as TelemetryClient (Badger)
    participant Telemetry as AppGatewayTelemetry
    participant T2 as T2 Telemetry Server
    
    Note over Client,T2: API Call with Latency Tracking
    
    Client->>AppGw: AuthorizeDataField(appId, field)
    AppGw->>Badger: AuthorizeDataField(appId, field)
    activate Badger
    
    Note over Badger: Start automatic API timer
    Badger->>Badger: AGW_TRACK_API_CALL(apiTimer, "AuthorizeDataField")
    Note over Badger: apiStart = now()
    
    Badger->>Badger: Check permissions cache
    Note over Badger: Cache miss - need to fetch
    
    Note over Badger: Start manual service timer
    Badger->>Badger: serviceCallStart = now()
    
    Badger->>OttSvc: GetAppPermissions(appId, false, permissions) [COM-RPC]
    activate OttSvc
    
    Note over OttSvc: Process request<br/>(150ms elapsed)
    
    OttSvc-->>Badger: Core::ERROR_NONE + permissions
    deactivate OttSvc
    
    Note over Badger: Stop manual service timer
    Badger->>Badger: serviceCallEnd = now()<br/>serviceLatencyMs = 150
    
    Note over Badger: Report service latency metric
    Badger->>TelemetryClient: AGW_REPORT_SERVICE_LATENCY(<br/>AGW_SERVICE_OTT_SERVICES, 150.0)
    activate TelemetryClient
    
    TelemetryClient->>TelemetryClient: RecordServiceLatency(service, latency)
    
    Note over TelemetryClient: Build composite metric name:<br/>"AppGwBadger_OttServices_Latency_split"
    
    TelemetryClient->>Telemetry: RecordTelemetryMetric(<br/>context,<br/>"AppGwBadger_OttServices_Latency_split",<br/>150.0,<br/>"ms") [COM-RPC]
    activate Telemetry
    
    Note over Telemetry: Aggregate metric:<br/>- sum += 150.0<br/>- count++<br/>- min = min(current, 150.0)<br/>- max = max(current, 150.0)
    
    Telemetry-->>TelemetryClient: Core::ERROR_NONE
    deactivate Telemetry
    
    TelemetryClient-->>Badger: 
    deactivate TelemetryClient
    
    Badger->>Badger: Process permissions<br/>Check authorization
    Note over Badger: Authorization successful
    
    Badger-->>AppGw: Core::ERROR_NONE
    deactivate Badger
    
    Note over Badger: Scoped timer destructs
    Note over Badger: apiEnd = now()<br/>apiLatencyMs = 200
    
    Badger->>TelemetryClient: timer destructor -> RecordApiLatency("AuthorizeDataField", 200.0)
    activate TelemetryClient
    
    Note over TelemetryClient: Build composite metric name:<br/>"AppGwBadger_AuthorizeDataField_Latency_split"
    
    TelemetryClient->>Telemetry: RecordTelemetryMetric(<br/>context,<br/>"AppGwBadger_AuthorizeDataField_Latency_split",<br/>200.0,<br/>"ms") [COM-RPC]
    activate Telemetry
    
    Note over Telemetry: Aggregate API latency metric:<br/>- sum += 200.0<br/>- count++<br/>- min, max updated
    
    Telemetry-->>TelemetryClient: Core::ERROR_NONE
    deactivate Telemetry
    deactivate TelemetryClient
    
    AppGw-->>Client: Success Response
    
    Note over Telemetry: ... Time passes (1 hour) ...
    
    Telemetry->>Telemetry: OnTimerExpired()
    activate Telemetry
    
    Telemetry->>Telemetry: SendAggregatedMetrics()
    
    Note over Telemetry: Calculate statistics:<br/>API Latency (AuthorizeDataField):<br/>- sum: 5000.0ms<br/>- min: 120.0ms<br/>- max: 450.0ms<br/>- count: 25<br/>- avg: 200.0ms<br/><br/>Service Latency (OttServices):<br/>- sum: 3750.0ms<br/>- min: 100.0ms<br/>- max: 300.0ms<br/>- count: 25<br/>- avg: 150.0ms
    
    Telemetry->>Telemetry: FormatTelemetryPayload()
    
    Telemetry->>T2: t2_event_s("AppGwBadger_AuthorizeDataField_Latency_split", apiMetrics)
    T2-->>Telemetry: Success
    
    Telemetry->>T2: t2_event_s("AppGwBadger_OttServices_Latency_split", svcMetrics)
    T2-->>Telemetry: Success
    
    Telemetry->>Telemetry: Reset metric aggregations
    
    deactivate Telemetry
```

## Key Components

| Component | Responsibility |
|-----------|---------------|
| **WebSocket Client** | Initiates API call requiring permission check |
| **AppGateway** | Routes request to Badger plugin |
| **Badger Plugin** | Performs authorization, tracks API latency automatically |
| **OttServices** | Provides permission data via COM-RPC |
| **TelemetryClient** | Reports both API and service latency metrics |
| **AppGatewayTelemetry** | Aggregates metrics (sum, min, max, count, avg) |
| **T2 Telemetry Server** | Receives aggregated latency statistics |

## Timing Methods

### 1. Automatic API Latency (Scoped Timer)

```cpp
Core::hresult Badger::AuthorizeDataField(...) {
    AGW_TRACK_API_CALL(apiTimer, "AuthorizeDataField");
    
    // ... API implementation ...
    
    if (error) {
        apiTimer.SetFailed(errorCode);  // Mark as failed
        return ERROR;
    }
    
    return SUCCESS;
    // Timer destructor automatically reports latency
}
```

**Benefits:**
- RAII-style automatic timing
- Handles both success and failure cases
- No manual timing code needed
- Guaranteed reporting even with early returns

### 2. Manual Service Latency

```cpp
// Start timer before external service call
auto serviceCallStart = std::chrono::steady_clock::now();

// Call external service
auto result = ottServices->GetAppPermissions(...);

// Stop timer after service call
auto serviceCallEnd = std::chrono::steady_clock::now();
auto serviceLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
    serviceCallEnd - serviceCallStart).count();

// Report service latency
AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_SERVICES, static_cast<double>(serviceLatencyMs));
```

**Benefits:**
- Precise measurement of specific service calls
- Can measure individual steps within an API call
- Flexible timing granularity

## Composite Metric Naming for Latency

### API Latency Metrics
**Pattern:** `AppGw<PluginName>_<ApiName>_Latency_split`

**Examples:**
- `AppGwBadger_AuthorizeDataField_Latency_split`
- `AppGwBadger_GetDeviceSessionId_Latency_split`
- `AppGwOttServices_GetAppPermissions_Latency_split`

**Payload (JSON):**
```json
{
  "sum": 5000.0,
  "count": 25,
  "unit": "ms",
  "reporting_interval_sec": 3600
}
```

### Service Latency Metrics
**Pattern:** `AppGw<PluginName>_<ServiceName>_Latency_split`

**Examples:**
- `AppGwBadger_OttServices_Latency_split`
- `AppGwOttServices_ThorPermissionService_Latency_split`
- `AppGwOttServices_OttTokenService_Latency_split`

**Payload (JSON):**
```json
{
  "sum": 3750.0,
  "count": 25,
  "unit": "ms",
  "reporting_interval_sec": 3600
}
```

**Key Change:** Plugin and API/service names are now part of the metric name, not the payload. This enables per-API/service alerting and trending.

## Metric Aggregation

AppGatewayTelemetry aggregates metrics over the reporting interval:

| Statistic | Calculation | Purpose |
|-----------|-------------|---------|
| `sum` | Σ(all latency values) | Total time spent |
| `min` | min(all latency values) | Best case performance |
| `max` | max(all latency values) | Worst case performance |
| `count` | Number of samples | Call frequency |
| `avg` | sum / count | Average latency |
| `unit` | "ms" | Measurement unit |

## Multi-Plugin Metric Tracking

Each plugin/API combination gets its own unique metric name:

| Plugin | API/Service | Metric Type | Metric Name |
|--------|-------------|-------------|-------------|
| Badger | AuthorizeDataField | API Latency | `AppGwBadger_AuthorizeDataField_Latency_split` |
| Badger | OttServices | Service Latency | `AppGwBadger_OttServices_Latency_split` |
| OttServices | GetAppPermissions | API Latency | `AppGwOttServices_GetAppPermissions_Latency_split` |
| OttServices | ThorPermissionService | Service Latency | `AppGwOttServices_ThorPermissionService_Latency_split` |
| OttServices | OttTokenService | Service Latency | `AppGwOttServices_OttTokenService_Latency_split` |

**Benefits:**
- Each API/service has independent metric for trending
- Enables per-API alerting (e.g., alert if `AppGwBadger_AuthorizeDataField_Latency_split` > 500ms)
- Simplifies dashboard creation (one metric = one graph)

## Configuration

- **Reporting Interval**: Default 3600 seconds (1 hour), configurable
- **Format**: JSON (self-describing) or COMPACT (minimal)
- **Reset**: Metric aggregations reset after each report
- **Granularity**: Per API/service, per plugin

## Performance Insights

Latency metrics enable:
1. **SLA Monitoring**: Track if APIs meet latency SLAs
2. **Bottleneck Identification**: Identify slow services (high max, high avg)
3. **Trend Analysis**: Compare latency over time
4. **Capacity Planning**: Understand call frequency (count) and total time (sum)
5. **Performance Regression Detection**: Detect when latency increases

## Notes

- **Automatic timing**: Preferred for API latency (scoped timer)
- **Manual timing**: Used for granular service call measurement
- **C++11 compatible**: Uses `std::chrono::steady_clock` (monotonic, not affected by time changes)
- **Zero overhead when telemetry unavailable**: Macros check availability before timing
- **Thread-safe**: AppGatewayTelemetry uses locks for metric aggregation
- **Failed API latency**: Scoped timer can track failed API call latency separately
