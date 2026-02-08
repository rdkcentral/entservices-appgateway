# App Gateway Telemetry - Sequence Diagrams

## Scenario 1: Bootstrap Time Recording

```mermaid
sequenceDiagram
    participant Thunder as Thunder Framework
    participant AG as AppGateway
    participant AGT as AppGatewayTelemetry
    participant T2 as T2 Server

    Thunder->>AG: Initialize service
    activate AG
    
    Note over AG: Start bootstrap timer
    
    AG->>AGT: getInstance and Initialize
    activate AGT
    AGT->>AGT: Telemetry init
    AGT->>AGT: Start periodic timer 1 hour
    AGT-->>AG: Initialized
    deactivate AGT
    
    AG->>AG: Create AppGatewayResolver
    AG->>AG: Configure resolver
    Note over AG: pluginsLoaded++
    
    AG->>AG: Create AppGatewayResponder
    AG->>AG: Configure responder
    Note over AG: pluginsLoaded++
    
    Note over AG: Stop bootstrap timer
    
    AG->>AGT: RecordBootstrapTime
    activate AGT
    AGT->>AGT: Send TWO separate metrics
    AGT->>T2: sendMessage AppGwBootstrapDuration_split
    Note over T2: Metric: {"sum": 1250, "count": 1, "unit": "ms"}
    AGT->>T2: sendMessage AppGwBootstrapPluginCount_split
    Note over T2: Metric: {"sum": 2, "count": 1, "unit": "count"}
    deactivate AGT
    
    AG-->>Thunder: Success
    deactivate AG
```

## Scenario 2: Health Stats Reporting

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant Resp as Responder
    participant Impl as Implementation
    participant AGT as Telemetry
    participant Timer as Timer
    participant T2 as T2 Server

    Note over AGT,Timer: During Operation 1 hour period
    
    Note over Client,Resp: WebSocket Connection
    Client->>Resp: Connect with session token
    Resp->>Resp: Authenticate
    Resp->>AGT: IncrementWebSocketConnections
    AGT->>AGT: websocketConnections++
    
    Note over Client,Impl: API Calls
    Client->>Resp: API Request method params
    Resp->>AGT: IncrementTotalCalls
    AGT->>AGT: totalCalls++
    
    Resp->>Impl: Resolve context method params
    
    alt Success
        Impl-->>Resp: Resolution
        Resp->>AGT: IncrementSuccessfulCalls
        AGT->>AGT: successfulCalls++
    else Failure
        Impl-->>Resp: Error
        Resp->>AGT: IncrementFailedCalls
        AGT->>AGT: failedCalls++
    end
    
    Note over Client,Resp: WebSocket Disconnection
    Client->>Resp: Disconnect
    Resp->>AGT: DecrementWebSocketConnections
    AGT->>AGT: websocketConnections--
    
    Note over Timer,T2: After 1 hour or cache threshold
    
    Timer->>AGT: Dispatch Timer expired
    activate AGT
    AGT->>AGT: OnTimerExpired
    AGT->>AGT: FlushTelemetryData
    
    AGT->>AGT: SendHealthStats
    Note over AGT: Send FOUR separate metrics
    AGT->>T2: sendMessage AppGwWebSocketConnections_split
    AGT->>T2: sendMessage AppGwTotalCalls_split
    AGT->>T2: sendMessage AppGwSuccessfulCalls_split
    AGT->>T2: sendMessage AppGwFailedCalls_split
    Note over T2: Each metric: {"sum": <value>, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
    
    AGT->>AGT: ResetHealthStats
    AGT->>AGT: Reschedule timer 1 hour
    deactivate AGT
```

## Scenario 3: API Error Stats Reporting

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant Resp as Responder
    participant Impl as Implementation
    participant AGT as Telemetry
    participant Timer as Timer
    participant T2 as T2 Server

    Note over Client,AGT: During Operation API Failures

    Client->>Resp: API Request method1
    Resp->>AGT: IncrementTotalCalls
    Resp->>Impl: Resolve context method1 params
    Impl-->>Resp: ERROR resolution failed
    Resp->>AGT: IncrementFailedCalls
    Resp->>AGT: RecordApiError method1
    activate AGT
    AGT->>AGT: mApiErrorCounts method1 ++
    deactivate AGT
    
    Client->>Resp: API Request method2
    Resp->>AGT: IncrementTotalCalls
    Resp->>Impl: Resolve context method2 params
    Impl-->>Resp: ERROR not supported
    Resp->>AGT: IncrementFailedCalls
    Resp->>AGT: RecordApiError method2
    activate AGT
    AGT->>AGT: mApiErrorCounts method2 ++
    deactivate AGT
    
    Client->>Resp: API Request method1
    Resp->>AGT: IncrementTotalCalls
    Resp->>Impl: Resolve context method1 params
    Impl-->>Resp: ERROR
    Resp->>AGT: IncrementFailedCalls
    Resp->>AGT: RecordApiError method1
    activate AGT
    AGT->>AGT: mApiErrorCounts method1 ++
    Note over AGT: method1 count equals 2
    deactivate AGT

    Note over Timer,T2: Reporting interval elapsed

    Timer->>AGT: Dispatch
    activate AGT
    AGT->>AGT: FlushTelemetryData
    AGT->>AGT: SendApiErrorStats
    
    Note over AGT: Send individual metric per API
    AGT->>T2: sendMessage AppGwApiErrorCount_method1_split
    Note over T2: Metric: {"sum": 2, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
    AGT->>T2: sendMessage AppGwApiErrorCount_method2_split
    Note over T2: Metric: {"sum": 1, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
    
    AGT->>AGT: ResetApiErrorStats
    Note over AGT: Clear mApiErrorCounts
    deactivate AGT
```

## Scenario 4: External Service Error Reporting

```mermaid
sequenceDiagram
    participant Client as WebSocket Client
    participant Resp as Responder
    participant Auth as Authenticator
    participant Impl as Implementation
    participant Perm as PermissionService
    participant AGT as Telemetry
    participant Timer as Timer
    participant T2 as T2 Server

    Note over Client,AGT: External Service Failures

    Note over Client,Auth: Authentication Service Failure
    Client->>Resp: Connect with session token
    Resp->>Auth: Authenticate sessionId
    Auth-->>Resp: ERROR service unavailable
    Resp->>AGT: RecordExternalServiceError AuthenticationService
    activate AGT
    AGT->>AGT: mExternalServiceErrorCounts AuthenticationService ++
    deactivate AGT
    Resp-->>Client: Connection rejected

    Note over Client,Perm: Permission Service Failure
    Client->>Resp: API Request protected method
    Resp->>Impl: Resolve context method params
    Impl->>Impl: HasPermissionGroup method
    Note over Impl: Method requires permission check
    Impl->>Perm: CheckPermissionGroup appId group
    Perm-->>Impl: ERROR service failure
    Impl->>AGT: RecordExternalServiceError PermissionService
    activate AGT
    AGT->>AGT: mExternalServiceErrorCounts PermissionService ++
    deactivate AGT
    Impl-->>Resp: NotPermitted error
    Resp-->>Client: Error response

    Note over Timer,T2: Reporting interval elapsed

    Timer->>AGT: Dispatch
    activate AGT
    AGT->>AGT: FlushTelemetryData
    AGT->>AGT: SendExternalServiceErrorStats
    
    Note over AGT: Send individual metric per service
    AGT->>T2: sendMessage AppGwExtServiceErrorCount_AuthenticationService_split
    Note over T2: Metric: {"sum": 1, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
    AGT->>T2: sendMessage AppGwExtServiceErrorCount_PermissionService_split
    Note over T2: Metric: {"sum": 1, "count": 1, "unit": "count", "reporting_interval_sec": 3600}
    
    AGT->>AGT: ResetExternalServiceErrorStats
    deactivate AGT
```

## Shutdown Sequence

```mermaid
sequenceDiagram
    participant Thunder as Thunder Framework
    participant AG as AppGateway
    participant AGT as Telemetry
    participant T2 as T2 Server

    Thunder->>AG: Deinitialize service
    activate AG
    
    AG->>AGT: getInstance Deinitialize
    activate AGT
    
    AGT->>AGT: Stop timer Revoke
    AGT->>AGT: FlushTelemetryData
    
    Note over AGT: Send remaining aggregated data
    
    AGT->>T2: SendHealthStats
    AGT->>T2: SendApiErrorStats
    AGT->>T2: SendExternalServiceErrorStats
    AGT->>T2: SendAggregatedMetrics
    
    AGT->>AGT: Reset all counters
    AGT->>AGT: mInitialized equals false
    
    AGT-->>AG: Deinitialized
    deactivate AGT
    
    AG->>AG: Release mTelemetry
    AG->>AG: Release mResponder
    AG->>AG: Release mAppGateway
    AG->>AG: Release mService
    
    AG-->>Thunder: Done
    deactivate AG
```

## Scenario 5: External Plugin COM-RPC Telemetry

This diagram shows how external plugins (Badger, OttServices, etc.) report telemetry to AppGateway via COM-RPC.

```mermaid
sequenceDiagram
    participant Ext as External Plugin Badger OttServices
    participant Shell as PluginHost IShell
    participant COMRPC as COM-RPC
    participant AG as AppGateway
    participant AGT as Telemetry
    participant Timer as Timer
    participant T2 as T2 Server

    Note over Ext,AGT: External Plugin Reports Telemetry Event

    Ext->>Shell: QueryInterfaceIAppGatewayTelemetry
    Shell->>AG: Query INTERFACE_AGGREGATE
    AG-->>Shell: mTelemetry pointer
    Shell-->>Ext: IAppGatewayTelemetry interface
    
    Note over Ext: Report API Error
    Ext->>COMRPC: RecordTelemetryEvent context AppGwBadgerApiError_split data
    COMRPC->>AGT: RecordTelemetryEvent GatewayContext eventName eventData
    activate AGT
    AGT->>AGT: Parse eventName contains ApiError
    AGT->>AGT: Extract api from eventData JSON
    AGT->>AGT: RecordApiError api
    AGT->>AGT: mCachedEventCount++
    AGT-->>COMRPC: ERROR_NONE
    deactivate AGT
    COMRPC-->>Ext: Success
    
    Note over Ext: Report External Service Error
    Ext->>COMRPC: RecordTelemetryEvent context AppGwOttExtServiceError_split data
    COMRPC->>AGT: RecordTelemetryEvent GatewayContext eventName eventData
    activate AGT
    AGT->>AGT: Parse eventName contains ExternalServiceError
    AGT->>AGT: Extract service from eventData JSON
    AGT->>AGT: RecordExternalServiceErrorInternal service
    AGT->>AGT: mCachedEventCount++
    AGT-->>COMRPC: ERROR_NONE
    deactivate AGT
    COMRPC-->>Ext: Success
    
    Ext->>Ext: Release telemetry interface

    Note over Timer,T2: Reporting interval elapsed

    Timer->>AGT: Dispatch
    activate AGT
    AGT->>AGT: FlushTelemetryData
    AGT->>T2: SendHealthStats
    AGT->>T2: SendApiErrorStats
    AGT->>T2: SendExternalServiceErrorStats
    AGT->>T2: SendAggregatedMetrics
    deactivate AGT
```

## Scenario 6: External Plugin Metric Recording

```mermaid
sequenceDiagram
    participant Ext as External Plugin OttServices
    participant Shell as PluginHost IShell
    participant AGT as Telemetry
    participant Timer as Timer
    participant T2 as T2 Server

    Note over Ext,AGT: External Plugin Reports Metrics
    
    Ext->>Shell: QueryInterfaceIAppGatewayTelemetry
    Shell-->>Ext: IAppGatewayTelemetry interface
    
    Note over Ext: Report Streaming Bitrate Metric
    Ext->>AGT: RecordTelemetryMetric context AppGwOttStreamingBitrate_split 4500 kbps
    activate AGT
    AGT->>AGT: Get or create MetricData for AppGwOttStreamingBitrate_split
    AGT->>AGT: Update sum min max count
    AGT->>AGT: mCachedEventCount++
    AGT-->>Ext: ERROR_NONE
    deactivate AGT
    
    Note over Ext: Report multiple latency samples
    loop Multiple API calls
        Ext->>AGT: RecordTelemetryMetric context AppGwOttApiLatency_split value ms
        AGT->>AGT: Aggregate metric
    end
    
    Ext->>Ext: Release telemetry interface

    Note over Timer,T2: Reporting interval elapsed

    Timer->>AGT: Dispatch FlushTelemetryData
    activate AGT
    
    AGT->>AGT: SendAggregatedMetrics
    
    Note over AGT: For each metric in cache
    AGT->>AGT: Build JSON sum min max count avg unit
    AGT->>T2: sendMessage metricName as marker payload
    
    Note over AGT: AppGwOttStreamingBitrate_split payload
    Note over AGT: AppGwOttApiLatency_split payload
    
    AGT->>AGT: Clear mMetricsCache
    deactivate AGT
```

## Data Flow Summary

```mermaid
flowchart TD
    subgraph Sources[Telemetry Sources]
        A1[AppGateway - Bootstrap Time]
        A2[Responder - WS Connections]
        A3[Responder - API Calls]
        A4[Responder - Auth Errors]
        A5[Implementation - Permission Errors]
        A6[External Plugins via COM-RPC]
    end

    subgraph Aggregator[AppGatewayTelemetry Singleton]
        B1[HealthStats atomics]
        B2[ApiErrorCounts map]
        B3[ExtServiceErrors map]
        B4[MetricsCache map]
        B5[TelemetryTimer 1 hour]
    end

    subgraph Output[T2 Telemetry]
        C1[AppGwBootstrapDuration_split]
        C1b[AppGwBootstrapPluginCount_split]
        C2[AppGwWebSocketConnections_split]
        C2b[AppGwTotalCalls_split]
        C2c[AppGwSuccessfulCalls_split]
        C2d[AppGwFailedCalls_split]
        C3[AppGwApiErrorCount_<ApiName>_split]
        C4[AppGwExtServiceErrorCount_<ServiceName>_split]
        C5[AppGw<Plugin>_<Api/Service>_Latency_split]
    end

    A1 -->|RecordBootstrapTime| C1
    A1 -->|RecordBootstrapTime| C1b
    A2 -->|Increment Decrement WS| B1
    A3 -->|Increment Calls| B1
    A3 -->|RecordApiError| B2
    A4 -->|RecordExtServiceError| B3
    A5 -->|RecordExtServiceError| B3
    A6 -->|RecordTelemetryEvent| B2
    A6 -->|RecordTelemetryEvent| B3
    A6 -->|RecordTelemetryMetric| B4

    B5 -->|Timer Expired| B1
    B5 -->|Timer Expired| B2
    B5 -->|Timer Expired| B3
    B5 -->|Timer Expired| B4

    B1 -->|SendHealthStats| C2
    B1 -->|SendHealthStats| C2b
    B1 -->|SendHealthStats| C2c
    B1 -->|SendHealthStats| C2d
    B2 -->|SendApiErrorStats| C3
    B3 -->|SendExtServiceErrors| C4
    B4 -->|SendAggregatedMetrics| C5
```
