# Scenario 2: Health Stats Periodic Reporting

## Overview

This sequence diagram illustrates how App Gateway continuously tracks WebSocket connections and API call statistics, reporting aggregated health metrics at regular intervals (default: 1 hour).

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant Responder as AppGatewayResponder
    participant Telemetry as AppGatewayTelemetry
    participant Timer as Telemetry Timer
    participant T2 as T2 Telemetry Server
    
    Note over Client,T2: Continuous Operation Phase
    
    %% WebSocket Connection
    Client->>Responder: WebSocket Connect
    Responder->>Telemetry: IncrementWebSocketConnections()
    activate Telemetry
    Note over Telemetry: websocket_connections++
    Telemetry-->>Responder: 
    deactivate Telemetry
    
    %% API Call - Success
    Client->>Responder: API Call (GetSettings)
    Responder->>Telemetry: IncrementTotalCalls()
    activate Telemetry
    Note over Telemetry: total_calls++
    Telemetry-->>Responder: 
    deactivate Telemetry
    
    Responder->>Responder: Process Request
    Note over Responder: Success
    
    Responder->>Telemetry: IncrementSuccessfulCalls()
    activate Telemetry
    Note over Telemetry: successful_calls++
    Telemetry-->>Responder: 
    deactivate Telemetry
    
    Responder-->>Client: Response (SUCCESS)
    
    %% API Call - Failure
    Client->>Responder: API Call (AuthorizeDataField)
    Responder->>Telemetry: IncrementTotalCalls()
    activate Telemetry
    Note over Telemetry: total_calls++
    Telemetry-->>Responder: 
    deactivate Telemetry
    
    Responder->>Responder: Process Request
    Note over Responder: Permission Denied
    
    Responder->>Telemetry: IncrementFailedCalls()
    activate Telemetry
    Note over Telemetry: failed_calls++
    Telemetry-->>Responder: 
    deactivate Telemetry
    
    Responder-->>Client: Response (ERROR)
    
    %% WebSocket Disconnect
    Client->>Responder: WebSocket Disconnect
    Responder->>Telemetry: DecrementWebSocketConnections()
    activate Telemetry
    Note over Telemetry: websocket_connections--
    Telemetry-->>Responder: 
    deactivate Telemetry
    
    Note over Timer: ... Time passes (1 hour) ...
    
    %% Periodic Timer Expiration
    Timer->>Telemetry: OnTimerExpired()
    activate Telemetry
    
    Telemetry->>Telemetry: SendHealthStats()
    activate Telemetry
    
    Note over Telemetry: Collect current stats:<br/>- websocket_connections: 12<br/>- total_calls: 1543<br/>- successful_calls: 1520<br/>- failed_calls: 23<br/>- reporting_interval_sec: 3600
    
    Telemetry->>Telemetry: Send each stat as individual METRIC
    
    Telemetry->>T2: t2_event_s("AppGwWebSocketConnections_split", payload)
    Note over T2: {"sum": 12, "count": 1, "unit": "count",<br/>"reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>T2: t2_event_s("AppGwTotalCalls_split", payload)
    Note over T2: {"sum": 1543, "count": 1, "unit": "count",<br/>"reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>T2: t2_event_s("AppGwSuccessfulCalls_split", payload)
    Note over T2: {"sum": 1520, "count": 1, "unit": "count",<br/>"reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>T2: t2_event_s("AppGwFailedCalls_split", payload)
    Note over T2: {"sum": 23, "count": 1, "unit": "count",<br/>"reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>Telemetry: ResetHealthStats()
    Note over Telemetry: Keep websocket_connections<br/>Reset call counters
    
    deactivate Telemetry
    deactivate Telemetry
```

## Key Components

| Component | Responsibility |
|-----------|---------------|
| **WebSocket Client** | Frontend applications connecting to AppGateway |
| **AppGatewayResponder** | Handles WebSocket connections and API calls |
| **AppGatewayTelemetry** | Tracks connection and call statistics |
| **Telemetry Timer** | Triggers periodic reporting (default: 1 hour) |
| **T2 Telemetry Server** | Receives and stores health metrics |

## Tracked Metrics

| Metric | Description | Reset After Reporting? |
|--------|-------------|----------------------|
| `websocket_connections` | Current active WebSocket connections | No (current state) |
| `total_calls` | Total API calls in reporting period | Yes |
| `successful_calls` | Number of successful API calls | Yes |
| `failed_calls` | Number of failed API calls | Yes |
| `reporting_interval_sec` | Reporting interval in seconds | No (config value) |

## T2 Markers (Individual Metrics)

**Metric 1:** `AppGwWebSocketConnections_split`
```json
{
  "sum": 12,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Metric 2:** `AppGwTotalCalls_split`
```json
{
  "sum": 1543,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Metric 3:** `AppGwSuccessfulCalls_split`
```json
{
  "sum": 1520,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Metric 4:** `AppGwFailedCalls_split`
```json
{
  "sum": 23,
  "count": 1,
  "unit": "count",
  "reporting_interval_sec": 3600
}
```

**Compact Format:**
```
AppGwWebSocketConnections_split: 12,1,count,3600
AppGwTotalCalls_split: 1543,1,count,3600
AppGwSuccessfulCalls_split: 1520,1,count,3600
AppGwFailedCalls_split: 23,1,count,3600
```

## Configuration

- **Default Interval**: 3600 seconds (1 hour)
- **Configurable via**: `SetReportingInterval(intervalSec)`
- **Format**: Individual numeric metrics (not aggregated JSON)
- **Data Type**: METRIC (for statistical aggregation)
- **Reset Behavior**: Call counters reset after each report; connection count persists

## Call Flow

1. **Connection Tracking**: WebSocket connect/disconnect events update connection count
2. **Call Tracking**: Each API call increments total; success/failure tracked separately
3. **Timer Expiration**: After configured interval, timer triggers reporting
4. **Data Collection**: Aggregate all tracked metrics
5. **Individual Metric Sending**: Each stat sent as separate metric with unique T2 marker
6. **T2 Reporting**: Four separate metrics sent to T2 server
7. **Reset Counters**: Reset call counters (keep connection count)

## Notes

- Connection count is a **current state** (not reset)
- Call counters are **cumulative** over the reporting interval (reset after reporting)
- Each health stat has its own T2 marker for independent trending
- Success rate can be calculated in analytics: `successful_calls / total_calls * 100`
- Failure rate: `failed_calls / total_calls * 100`
- Metrics enable alerting on thresholds (e.g., failed_calls > 100)
