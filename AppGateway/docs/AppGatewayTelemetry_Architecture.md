# App Gateway T2 Telemetry Architecture

## Overview

The App Gateway Telemetry module provides comprehensive telemetry collection and reporting for the App Gateway service. It tracks bootstrap time, WebSocket connections, API call statistics, and external service errors, sending individual numeric metrics to the T2 telemetry server at configurable intervals for statistical aggregation and trending.

## Documentation

- **[Integration Guide](./AppGatewayTelemetryIntegrationGuide.md)** - Step-by-step guide for plugin developers
- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual flows for all telemetry scenarios
- **[Marker Reference](./AppGatewayTelemetryMarkers.md)** - Complete list of predefined markers

## Architecture Diagram

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                         APP GATEWAY PLUGIN                                   ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║  ┌────────────────────────────────────────────────────────────────────────┐  ║
║  │ AppGateway.cpp                                                         │  ║
║  │ • Initialize() → Bootstrap Time Measurement → RecordBootstrapTime()    │  ║
║  └───────────────────────────────────┬────────────────────────────────────┘  ║
║                                      │                                       ║
║  ┌────────────────────────────────────────────────────────────────────────┐  ║
║  │ AppGatewayResponderImplementation.cpp                                  │  ║
║  │ • SetAuthHandler(connect) → IncrementWSConnections()                   │  ║
║  │ • SetDisconnectHandler → DecrementWSConnections()                      │  ║
║  │ • DispatchWsMsg() → IncrementTotal/Success/Failed()                    │  ║
║  │ • DispatchWsMsg() → RecordApiError()                                   │  ║
║  │ • Auth Failure → RecordExtServiceError()                               │  ║
║  └───────────────────────────────────┬────────────────────────────────────┘  ║
║                                      │                                       ║
║  ┌────────────────────────────────────────────────────────────────────────┐  ║
║  │ AppGatewayImplementation.cpp                                           │  ║
║  │ • Permission Check Failure → RecordExtServiceError()                   │  ║
║  └───────────────────────────────────┬────────────────────────────────────┘  ║
║                                      │                                       ║
╚══════════════════════════════════════╩═══════════════════════════════════════╝
                                       │
                                       ▼
╔══════════════════════════════════════════════════════════════════════════════╗
║                 APPGATEWAY TELEMETRY (Singleton)                             ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║  ╔════════════════════════════════════════════════════════════════════════╗  ║
║  ║ DATA AGGREGATION LAYER                                                 ║  ║
║  ╠════════════════════════════════════════════════════════════════════════╣  ║
║  ║ ┌────────────────┐  ┌──────────────────┐  ┌──────────────────────┐     ║  ║
║  ║ │ HealthStats    │  │ ApiErrorCounts   │  │ ExtServiceError      │     ║  ║
║  ║ │  (atomic)      │  │    (map)         │  │   Counts (map)       │     ║  ║
║  ║ │ • websocket    │  │ Per-API error    │  │ Per-service error    │     ║  ║
║  ║ │ • totalCalls   │  │   tracking       │  │   tracking           │     ║  ║
║  ║ │ • successCalls │  │                  │  │                      │     ║  ║
║  ║ │ • failedCalls  │  │                  │  │                      │     ║  ║
║  ║ └────────────────┘  └──────────────────┘  └──────────────────────┘     ║  ║
║  ╚════════════════════════════════════════════════════════════════════════╝  ║
║                                      │                                       ║
║                                      ▼                                       ║
║  ╔════════════════════════════════════════════════════════════════════════╗  ║
║  ║ TIMER & REPORTING LAYER                                                ║  ║
║  ╠════════════════════════════════════════════════════════════════════════╣  ║
║  ║ ┌──────────────────────┐      ┌──────────────────────────────────────┐ ║  ║
║  ║ │ TelemetryTimer       │  ──▶ │ FlushTelemetryData()                 │ ║  ║
║  ║ │ • 1 hour default     │      │ • SendHealthStats()                  │ ║  ║
║  ║ │ • Cache Threshold    │      │ • SendApiErrorStats()                │ ║  ║
║  ║ │   (1000 records)     │      │ • SendExtServiceErrorStats()         │ ║  ║
║  ║ │                      │      │ • ResetCounters()                    │ ║  ║
║  ║ └──────────────────────┘      └──────────────────────────────────────┘ ║  ║
║  ╚════════════════════════════════════════════════════════════════════════╝  ║
║                                      │                                       ║
║                                      ▼                                       ║
║  ╔════════════════════════════════════════════════════════════════════════╗  ║
║  ║ T2 INTEGRATION LAYER                                                   ║  ║
║  ╠════════════════════════════════════════════════════════════════════════╣  ║
║  ║ SendT2Event() - Individual metrics sent to T2:                         ║  ║
║  ║ • AppGwBootstrapDuration_split                                         ║  ║
║  ║ • AppGwBootstrapPluginCount_split                                      ║  ║
║  ║ • AppGwWebSocketConnections_split                                      ║  ║
║  ║ • AppGwTotalCalls_split                                                ║  ║
║  ║ • AppGwSuccessfulCalls_split                                           ║  ║
║  ║ • AppGwFailedCalls_split                                               ║  ║
║  ║ • AppGwApiErrorCount_<ApiName>_split                                   ║  ║
║  ║ • AppGwExtServiceErrorCount_<Service>_split                            ║  ║
║  ║                                                                        ║  ║
║  ║ Implementation: Utils::Telemetry::sendMessage()                        ║  ║
║  ╚════════════════════════════════════════════════════════════════════════╝  ║
║                                      │                                       ║
╚══════════════════════════════════════╩═══════════════════════════════════════╝
                                       │
                                       ▼
                    ╔═══════════════════════════════════════╗
                    ║    T2 TELEMETRY SERVER                ║
                    ╠═══════════════════════════════════════╣
                    ║  t2_event_s(marker, payload)          ║
                    ╚═══════════════════════════════════════╝
```

## Component Description

### 1. AppGatewayTelemetry (Singleton)

**File:** `AppGatewayTelemetry.h/cpp`

The core telemetry aggregator class that serves as the central hub for all telemetry collection and reporting.

**Design Patterns:**
- **Singleton Pattern**: Ensures single instance across entire AppGateway ecosystem
- **Facade Pattern**: Provides simplified interface for complex telemetry operations
- **Observer Pattern**: Timer-based periodic observer for automated reporting

**Key Responsibilities:**
- Aggregate telemetry data from multiple sources (AppGateway, external plugins)
- Manage periodic reporting lifecycle
- Thread-safe data collection and access
- Metric aggregation (sum, count, min, max)
- Interface to T2 telemetry system

**Public Interface:** See [AppGatewayTelemetry.h](../AppGatewayTelemetry.h) for detailed API

### 2. TelemetryClient (Plugin-side Helper)

**File:** `helpers/UtilsAppGatewayTelemetry.h`

**Purpose:** Provides RAII-based telemetry client for external plugins to communicate with AppGatewayTelemetry via COM-RPC.

**Design Patterns:**
- **RAII (Resource Acquisition Is Initialization)**: Automatic resource management
- **Proxy Pattern**: Acts as proxy to remote AppGatewayTelemetry instance
- **Per-Plugin Isolation**: Each plugin has its own TelemetryClient instance via anonymous namespace

**Key Features:**
- Automatic interface acquisition and release
- Automatic plugin name injection
- Per-plugin instance isolation (prevents cross-plugin contamination)
- Null-safe operations (checks availability before calls)
- Zero-overhead when telemetry unavailable

**See:** [Integration Guide](./AppGatewayTelemetryIntegrationGuide.md) for usage details

### 3. TelemetryTimer (Periodic Reporter)

**Type:** `Core::TimerType<Core::IDispatch>`

**Purpose:** Manages periodic telemetry reporting and cache-based flushing.

**Configuration:**
- Default interval: 3600 seconds (1 hour)
- Cache threshold: 1000 events/metrics
- Configurable via plugin configuration

**Trigger Conditions:**
1. **Time-based**: Every N seconds (configurable interval)
2. **Event-based**: When cache exceeds threshold (1000 records)
3. **Manual**: On plugin shutdown (flush all data)



## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC` | 3600 (1 hour) | Interval between telemetry reports |
| `TELEMETRY_DEFAULT_CACHE_THRESHOLD` | 1000 | Max records before forced flush |

## Thread Safety

**Thread Safety Strategy:**
- **Health Stats**: Lock-free atomic operations for high-frequency counters (WebSocket, API calls)
- **Error Maps**: Protected by `Core::CriticalSection` for thread-safe updates
- **Metrics Cache**: Shared lock with error maps, short-lived critical sections
- **Timer Callbacks**: Single-threaded dispatch, copy data under lock then release before T2 I/O

**Performance Characteristics:**
- Hot path (atomic operations): Lock-free, O(1), nanoseconds
- Warm path (error recording): Short lock, O(log N) map insert, microseconds
- Cold path (periodic reporting): Long-running T2 I/O, but lock-free during transmission

## Design Patterns

### 1. Singleton Pattern

**Class:** `AppGatewayTelemetry`

**Rationale:**
- **Global Access**: Single point of telemetry collection across all components
- **Lazy Initialization**: Created on first use, destroyed on plugin shutdown
- **Thread-Safe**: Thread-safe initialization pattern

### 2. RAII (Resource Acquisition Is Initialization)

**Class:** `ScopedApiTimer` (via `AGW_SCOPED_API_TIMER` macro)

**Benefits:**
- **Automatic Timing**: No manual start/stop, prevents timing errors
- **Exception Safe**: Reports metrics even if function throws
- **Zero Overhead on Disable**: Entire object optimized away if telemetry disabled

### 3. Facade Pattern

**Class:** `TelemetryClient` (macro-based)

**Purpose:** Simplify complex COM-RPC telemetry interface for plugins

**Complexity Hidden:**
- Interface query through IShell
- GatewayContext construction
- JSON payload serialization
- Error handling and null checks
- Resource cleanup (Release())

### 4. Observer Pattern (Timer-based)

**Components:**
- **Subject**: `TelemetryTimer` (observes time)
- **Observer**: `FlushTelemetryData()` callback
- **Event**: Time interval elapsed or cache threshold reached

## Memory Management

- **Singleton Lifecycle**: Lazy initialization, explicit destruction on plugin shutdown
- **COM-RPC References**: Reference counting via `AddRef()` / `Release()`, RAII-based management
- **String Management**: Pass by const reference, copy only when storing, move semantics where possible

## External Plugin Integration via COM-RPC

Other plugins (Badger, OttServices, etc.) can report telemetry to AppGateway using the `IAppGatewayTelemetry` interface via COM-RPC.

**Interface:** `Exchange::IAppGatewayTelemetry`  
**Access:** Via `INTERFACE_AGGREGATE` exposed by AppGateway plugin  
**Protocol:** WPEFramework COM-RPC (binary, efficient)

**See:**
- [IAppGatewayTelemetry.h](../../../../../../entservices-apis/apis/IAppGatewayTelemetry.h) - COM-RPC interface definition
- [UtilsAppGatewayTelemetry.h](../helpers/UtilsAppGatewayTelemetry.h) - Helper macros for plugin integration
- [Integration Guide](./AppGatewayTelemetryIntegrationGuide.md) - Step-by-step plugin integration
- [Sequence Diagrams](./sequence-diagrams/README.md) - Visual interaction flows
- [Marker Reference](./AppGatewayTelemetryMarkers.md) - Complete marker catalog

## Dependencies

- `UtilsTelemetry.h` - T2 telemetry utility functions
- `UtilsAppGatewayTelemetry.h` - Telemetry helper macros for external plugins
- `Core::CriticalSection` - Thread synchronization
- `Core::TimerType<Core::IDispatch>` - Periodic timer
- `Core::ProxyType` - Smart pointer for timer dispatch

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC` | 3600 (1 hour) | Interval between telemetry reports |
| `TELEMETRY_DEFAULT_CACHE_THRESHOLD` | 1000 | Max cached events/metrics before forced flush |

## Key Design Decisions

1. **Singleton Pattern**: Single telemetry aggregation point across entire AppGateway ecosystem
2. **Generic Markers**: Reduces marker proliferation (2 generic markers vs. N*2 plugin-specific markers)
3. **Metric Aggregation**: Reduces T2 load (1 metric per reporting period vs. N individual events)
4. **Lock-free Hot Path**: Atomic operations for high-frequency counters (WebSocket, API calls)
5. **Copy-on-read**: Minimizes lock hold time during T2 I/O operations
6. **RAII Helpers**: Macros provide zero-overhead abstraction for plugins
7. **Periodic Reporting**: Batches telemetry to reduce T2 server load and network traffic

---

## Further Reading

For implementation details, integration steps, and usage examples:
- **[Integration Guide](./AppGatewayTelemetryIntegrationGuide.md)** - Complete integration walkthrough
- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual interaction flows
- **[Marker Reference](./AppGatewayTelemetryMarkers.md)** - Comprehensive marker catalog
- **[UtilsAppGatewayTelemetry.h](../helpers/UtilsAppGatewayTelemetry.h)** - Macro implementation source