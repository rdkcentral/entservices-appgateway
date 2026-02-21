# Scenario 1: Bootstrap Time Tracking

## Overview

This sequence diagram illustrates how App Gateway measures and reports the total time taken to initialize all plugins during system bootstrap.

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Thunder as Thunder Framework
    participant AppGw as AppGateway Plugin
    participant Telemetry as AppGatewayTelemetry
    participant T2 as T2 Telemetry Server
    
    Note over Thunder,T2: System Initialization Phase
    
    Thunder->>AppGw: Initialize()
    activate AppGw
    
    Note over AppGw: Start bootstrap timer
    AppGw->>AppGw: bootstrapStart = now()
    
    AppGw->>Telemetry: getInstance()
    Telemetry-->>AppGw: telemetry instance
    
    AppGw->>Telemetry: Initialize(service)
    activate Telemetry
    Note over Telemetry: Initialize timer<br/>Start periodic reporting
    Telemetry-->>AppGw: Core::ERROR_NONE
    deactivate Telemetry
    
    Note over AppGw: Initialize all plugins:<br/>- Badger<br/>- OttServices<br/>- FbAdvertising<br/>- etc.
    
    AppGw->>AppGw: Initialize child plugins
    Note over AppGw: bootstrapEnd = now()<br/>Calculate duration
    
    AppGw->>AppGw: Calculate duration_ms and plugins_loaded
    
    AppGw->>Telemetry: RecordBootstrapTime(duration_ms, plugins_loaded)
    activate Telemetry
    
    Note over Telemetry: Send as individual METRICS<br/>for aggregation and trending
    
    Telemetry->>Telemetry: RecordTelemetryMetric(<br/>"AppGwBootstrapDuration_split", duration_ms, "ms")
    Note over Telemetry: Metric 1: Duration in milliseconds
    
    Telemetry->>T2: t2_event_s("AppGwBootstrapDuration_split", metric_payload)
    Note over T2: Payload: {"sum": 2500, "count": 1,<br/>"unit": "ms", "reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry->>Telemetry: RecordTelemetryMetric(<br/>"AppGwBootstrapPluginCount_split", plugins_loaded, "count")
    Note over Telemetry: Metric 2: Number of plugins
    
    Telemetry->>T2: t2_event_s("AppGwBootstrapPluginCount_split", metric_payload)
    Note over T2: Payload: {"sum": 8, "count": 1,<br/>"unit": "count", "reporting_interval_sec": 3600}
    T2-->>Telemetry: Success
    
    Telemetry-->>AppGw: Core::ERROR_NONE
    deactivate Telemetry
    
    AppGw-->>Thunder: SUCCESS
    deactivate AppGw
```

## Key Components

| Component | Responsibility |
|-----------|---------------|
| **Thunder Framework** | Initiates plugin loading sequence |
| **AppGateway Plugin** | Measures bootstrap time across all child plugins |
| **AppGatewayTelemetry** | Aggregates and reports bootstrap metrics to T2 |
| **T2 Telemetry Server** | Receives and stores telemetry data |

## Data Flow

1. **Bootstrap Start**: AppGateway records timestamp when `Initialize()` is called
2. **Plugin Initialization**: All child plugins (Badger, OttServices, etc.) are loaded
3. **Bootstrap End**: After all plugins load, calculate total duration
4. **Telemetry Recording**: Report bootstrap time and plugin count to telemetry
5. **T2 Reporting**: Telemetry formats and sends data to T2 server

## T2 Markers

**Metric 1 Name:** `AppGwBootstrapDuration_split`

**Metric Payload Format:**
```json
{
  "sum": 2500,
  "count": 1,
  "unit": "ms",
  "reporting_interval_sec": 3600
}
```

**Metric 2 Name:** `AppGwBootstrapPluginCount_split`

**Metric Payload Format:**
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
AppGwBootstrapDuration_split: 2500,1,ms,3600
AppGwBootstrapPluginCount_split: 8,1,count,3600
```

## Configuration

- **Reporting**: Immediate upon bootstrap completion
- **Frequency**: Once per system start
- **Format**: Individual numeric metrics for aggregation
- **Data Type**: METRIC (not EVENT)

## Notes

- Bootstrap time is critical for measuring system startup performance
- Helps identify slow plugin initialization
- Reported as individual METRICS for statistical aggregation
- Each metric (duration and plugin count) sent separately with its own T2 marker
- Metrics can be aggregated, trended, and alerted on over time
- Independent of periodic telemetry reporting interval
