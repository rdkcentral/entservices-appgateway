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

**Class Interface:**
```cpp
class AppGatewayTelemetry {
public:
    // Singleton access
    static AppGatewayTelemetry& Instance();
    
    // Bootstrap tracking
    void RecordBootstrapTime(uint64_t durationMs, uint32_t pluginCount);
    
    // Health stats tracking
    void IncrementWebSocketConnections();
    void DecrementWebSocketConnections();
    void IncrementTotalCalls();
    void IncrementSuccessfulCalls();
    void IncrementFailedCalls();
    
    // Error tracking
    void RecordApiError(const std::string& apiName);
    void RecordExternalServiceError(const std::string& serviceName);
    
    // Metric tracking (with aggregation)
    void RecordMetric(const std::string& metricName, double value, const std::string& unit);
    
    // COM-RPC Interface (IAppGatewayTelemetry)
    Core::hresult RecordTelemetryEvent(const GatewayContext& context,
                                       const std::string& eventName,
                                       const std::string& eventData);
    Core::hresult RecordTelemetryMetric(const GatewayContext& context,
                                        const std::string& metricName,
                                        double metricValue,
                                        const std::string& metricUnit);
    
private:
    // Timer callback
    void FlushTelemetryData();
    
    // Internal reporting methods
    void SendHealthStats();
    void SendApiErrorStats();
    void SendExternalServiceErrorStats();
    void SendAggregatedMetrics();
};
```

### 2. TelemetryClient (Plugin-side Helper)

**File:** `UtilsAppGatewayTelemetry.h` (macro-based implementation)

**Purpose:** Provides RAII-based telemetry client for external plugins to communicate with AppGatewayTelemetry via COM-RPC.

**Design Patterns:**
- **RAII (Resource Acquisition Is Initialization)**: Automatic resource management
- **Proxy Pattern**: Acts as proxy to remote AppGatewayTelemetry instance
- **Template Metaprogramming**: Macro-based code generation for type safety

**Key Features:**
- Automatic interface acquisition and release
- Automatic plugin name injection
- Null-safe operations (checks availability before calls)
- Zero-overhead when telemetry unavailable

**Lifecycle:**
```cpp
// Initialization (in plugin Initialize())
AGW_TELEMETRY_INIT(service);  // Acquires IAppGatewayTelemetry via COM-RPC

// Usage (automatic context management)
AGW_REPORT_API_ERROR("GetSettings", "TIMEOUT");  // Single line, no boilerplate

// Cleanup (in plugin Deinitialize())
AGW_TELEMETRY_DEINIT();  // Releases interface, RAII cleanup
```

**Internal Mechanism:**
- Stores plugin name as thread-local or static variable
- Maintains weak reference to IAppGatewayTelemetry interface
- Constructs JSON payloads automatically
- Composes metric names from plugin + API/service + suffix

### 3. TelemetryTimer (Periodic Reporter)

**File:** `AppGatewayTelemetry.cpp` (internal component)

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

**Timer Dispatch Flow:**
```
TelemetryTimer (Armed)
    │
    ├─ Interval Elapsed → Dispatch() → FlushTelemetryData()
    │                                      ├─ Lock mTelemetryLock
    │                                      ├─ SendHealthStats()
    │                                      ├─ SendApiErrorStats()
    │                                      ├─ SendExternalServiceErrorStats()
    │                                      ├─ SendAggregatedMetrics()
    │                                      ├─ ResetCounters()
    │                                      └─ Unlock mTelemetryLock
    │
    └─ Cache Threshold Reached → FlushTelemetryData() (same flow)
```

### 4. MetricData (Aggregation Container)

**File:** `AppGatewayTelemetry.cpp` (internal structure)

**Purpose:** Aggregates multiple metric samples for statistical reporting.

**Structure:**
```cpp
struct MetricData {
    double sum;        // Total sum of all samples
    double min;        // Minimum value observed
    double max;        // Maximum value observed
    uint32_t count;    // Number of samples
    std::string unit;  // Unit of measurement (ms, count, kbps, etc.)
    
    // Aggregation methods
    void AddSample(double value);
    double GetAverage() const;  // sum / count
    void Reset();
};
```

**Usage:**
- Stored in `std::map<std::string, MetricData>` indexed by metric name
- Protected by `Core::CriticalSection` for thread safety
- Automatically calculated statistics: sum, min, max, count, average
- Sent to T2 as single metric with all aggregated values

**Example Aggregation:**
```
Metric: AppGwBadger_GetSettings_Latency_split
Samples: [120ms, 150ms, 95ms, 200ms, 110ms]

MetricData:
  sum = 675.0
  min = 95.0
  max = 200.0
  count = 5
  average = 135.0  (calculated: sum/count)
  unit = "ms"

T2 Payload:
{
  "sum": 675.0,
  "count": 5,
  "min": 95.0,
  "max": 200.0,
  "unit": "ms",
  "reporting_interval_sec": 3600
}
```

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC` | 3600 (1 hour) | Interval between telemetry reports |
| `TELEMETRY_DEFAULT_CACHE_THRESHOLD` | 1000 | Max records before forced flush |

## Thread Safety

- `HealthStats` uses `std::atomic<uint32_t>` for lock-free counter updates
- API error and external service error maps protected by `Core::CriticalSection`
- Timer operations are synchronized through the lock

## Data Structures and Thread Safety

### 1. HealthStats Structure

**Purpose:** Track real-time health metrics with atomic operations

**Implementation:**
```cpp
struct HealthStats {
    std::atomic<uint32_t> websocketConnections{0};
    std::atomic<uint32_t> totalCalls{0};
    std::atomic<uint32_t> successfulCalls{0};
    std::atomic<uint32_t> failedCalls{0};
};
```

**Thread Safety:**
- **Lock-free**: Uses C++11 `std::atomic` for wait-free operations
- **Memory Order**: Default `memory_order_seq_cst` ensures sequential consistency
- **Performance**: O(1) increment/decrement, no contention
- **Thread Model**: Multiple producers (WebSocket threads, API handlers), single consumer (timer)

**Access Pattern:**
```
WebSocket Thread 1 ──┐
WebSocket Thread 2 ──┼──▶ atomic increment/decrement ──▶ HealthStats
API Handler Thread ──┘                                      ▼
                                                     Timer reads atomically
                                                            ▼
                                                         T2 Report
```

### 2. Error Count Maps

**Purpose:** Track error counts per API/Service with unique metric names

**Implementation:**
```cpp
std::map<std::string, uint32_t> mApiErrorCounts;           // API name → count
std::map<std::string, uint32_t> mExternalServiceErrorCounts; // Service name → count
Core::CriticalSection mTelemetryLock;                       // Protects both maps
```

**Thread Safety:**
- **Mutex-based**: `Core::CriticalSection` (WPEFramework's mutex wrapper)
- **Granularity**: Coarse-grained lock protecting both maps and metric data
- **Lock Duration**: Short-lived (single map insert/update operation)
- **Deadlock Prevention**: Single lock, no nested locking

**Access Pattern:**
```cpp
void RecordApiError(const std::string& apiName) {
    Core::SafeSyncType<Core::CriticalSection> lock(mTelemetryLock);
    mApiErrorCounts[apiName]++;  // Thread-safe map update
}
```

### 3. Metrics Cache

**Purpose:** Aggregate metric samples for statistical reporting

**Implementation:**
```cpp
struct MetricData {
    double sum{0.0};
    double min{std::numeric_limits<double>::max()};
    double max{std::numeric_limits<double>::lowest()};
    uint32_t count{0};
    std::string unit;
};

std::map<std::string, MetricData> mMetricsCache;  // Metric name → aggregated data
Core::CriticalSection mTelemetryLock;              // Shared lock with error maps
```

**Thread Safety:**
- **Shared Lock**: Same `mTelemetryLock` protects metrics cache and error maps
- **Rationale**: All data structures modified from same call contexts, reduces lock contention
- **Critical Section**: Short-lived calculations (min/max/sum update)

**Aggregation Algorithm:**
```cpp
void RecordMetric(const std::string& metricName, double value, const std::string& unit) {
    Core::SafeSyncType<Core::CriticalSection> lock(mTelemetryLock);
    
    auto& metric = mMetricsCache[metricName];
    metric.sum += value;
    metric.min = std::min(metric.min, value);
    metric.max = std::max(metric.max, value);
    metric.count++;
    metric.unit = unit;
}
```

### 4. Timer Synchronization

**Timer Model:** Single-threaded dispatch via `Core::TimerType<Core::IDispatch>`

**Thread Safety Guarantee:**
- Timer callbacks execute on dedicated timer thread
- No concurrent timer callbacks (sequential execution guaranteed by Core::Timer)
- Lock acquisition only when accessing shared data structures

**Flush Sequence:**
```
Timer Thread:
    FlushTelemetryData() {
        // Read phase (under lock)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mTelemetryLock);
            Copy HealthStats atomics to local variables
            Copy API error counts to local map
            Copy service error counts to local map
            Copy metrics cache to local map
        }
        
        // Report phase (lock-free, safe to call T2 API)
        SendHealthStats(local copies);
        SendApiErrorStats(local copies);
        SendExternalServiceErrorStats(local copies);
        SendAggregatedMetrics(local copies);
        
        // Reset phase (under lock)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mTelemetryLock);
            Clear API error counts
            Clear service error counts
            Clear metrics cache
            // Note: HealthStats are gauges, not reset
        }
    }
```

**Why This Design:**
- Minimize lock hold time during expensive T2 communication
- Prevent producer threads from blocking on T2 network I/O
- Copy-on-read ensures consistent snapshot for reporting period

## Design Patterns

### 1. Singleton Pattern

**Class:** `AppGatewayTelemetry`

**Implementation:**
```cpp
class AppGatewayTelemetry {
private:
    static AppGatewayTelemetry* sInstance;
    static Core::CriticalSection sInstanceLock;
    
    AppGatewayTelemetry();  // Private constructor
    ~AppGatewayTelemetry(); // Private destructor
    
public:
    static AppGatewayTelemetry& Instance() {
        if (sInstance == nullptr) {
            Core::SafeSyncType<Core::CriticalSection> lock(sInstanceLock);
            if (sInstance == nullptr) {  // Double-checked locking
                sInstance = new AppGatewayTelemetry();
            }
        }
        return *sInstance;
    }
    
    // Non-copyable, non-movable
    AppGatewayTelemetry(const AppGatewayTelemetry&) = delete;
    AppGatewayTelemetry& operator=(const AppGatewayTelemetry&) = delete;
};
```

**Rationale:**
- **Global Access**: Single point of telemetry collection across all components
- **Lazy Initialization**: Created on first use, destroyed on plugin shutdown
- **Thread-Safe**: Double-checked locking pattern for initialization

### 2. RAII (Resource Acquisition Is Initialization)

**Class:** `ScopedApiTimer` (via `AGW_SCOPED_API_TIMER` macro)

**Implementation:**
```cpp
class ScopedApiTimer {
private:
    std::string apiName;
    std::chrono::steady_clock::time_point startTime;
    bool failed{false};
    std::string errorDetails;
    
public:
    ScopedApiTimer(const std::string& api) : apiName(api) {
        startTime = std::chrono::steady_clock::now();
    }
    
    ~ScopedApiTimer() {
        auto endTime = std::chrono::steady_clock::now();
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        // Report latency
        AGW_REPORT_API_LATENCY(apiName, static_cast<double>(durationMs));
        
        // Report error if failed
        if (failed) {
            AGW_REPORT_API_ERROR(apiName, errorDetails);
        }
    }
    
    void SetFailed(const std::string& error) {
        failed = true;
        errorDetails = error;
    }
};

// Usage:
AGW_SCOPED_API_TIMER(timer, "GetSettings");  // Constructor starts timer
// ... API implementation ...
if (error) timer.SetFailed("TIMEOUT");
// Destructor automatically reports latency and error
```

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

**Facade Provided:**
```cpp
// Complex (direct COM-RPC):
auto telemetry = service->QueryInterface<Exchange::IAppGatewayTelemetry>(APPGATEWAY_CALLSIGN);
if (telemetry != nullptr) {
    Exchange::IAppGatewayTelemetry::GatewayContext ctx;
    ctx.appId = "MyPlugin";
    ctx.appInstanceId = "";
    JsonObject data;
    data["plugin"] = "MyPlugin";
    data["api"] = "GetSettings";
    data["error"] = "TIMEOUT";
    std::string eventData;
    data.ToString(eventData);
    telemetry->RecordTelemetryEvent(ctx, "AppGwPluginApiError_split", eventData);
    telemetry->Release();
}

// Simple (facade):
AGW_REPORT_API_ERROR("GetSettings", "TIMEOUT");
```

### 4. Observer Pattern (Timer-based)

**Implementation:** Timer observes elapsed time and triggers telemetry flush

**Components:**
- **Subject**: `TelemetryTimer` (observes time)
- **Observer**: `FlushTelemetryData()` callback
- **Event**: Time interval elapsed or cache threshold reached

**Pattern:**
```
TelemetryTimer ──observes──▶ Time Elapsed?
                                  │
                                  ▼ YES
                          Notify Observer
                                  │
                                  ▼
                        FlushTelemetryData()
                                  │
                                  ▼
                         Publish to T2 Server
```

## Memory Management

### 1. Singleton Lifecycle

**Creation:** Lazy initialization on first `Instance()` call
**Ownership:** Static member, owned by class itself
**Destruction:** Explicit `Deinitialize()` call on plugin shutdown
**Memory Leak Prevention:** Flush all cached data before destruction

### 2. COM-RPC Interface References

**Pattern:** Reference counting via `AddRef()` / `Release()`

**TelemetryClient Lifecycle:**
```cpp
// Initialization
AGW_TELEMETRY_INIT(service);
    ↓
QueryInterface<IAppGatewayTelemetry>(APPGATEWAY_CALLSIGN)  // AddRef() implicit
    ↓
Store in static/thread-local variable
    ↓
// Usage (multiple calls, same interface instance)
    ↓
AGW_TELEMETRY_DEINIT();
    ↓
Release() // Reference count decremented
```

**Smart Pointer Usage:** WPEFramework `Core::ProxyType<T>` for RAII-based reference management

### 3. String Memory Management

**API Parameter Strings:**
- Pass by `const std::string&` (no copy on function call)
- Copy into map/cache only when storing

**JSON Serialization:**
- Temporary `JsonObject` for construction
- Move semantics where possible
- Immediate serialization to avoid prolonged allocation

**Map Keys:**
- Stored as `std::string` in map (owned by map)
- No raw pointers, RAII ensures cleanup

## Integration Points

### AppGateway.cpp
- Initializes telemetry singleton on plugin start
- Records bootstrap time after all components initialize
- Exposes `IAppGatewayTelemetry` via `INTERFACE_AGGREGATE` for COM-RPC access
- Deinitializes (flushes data) on plugin shutdown

### AppGatewayResponderImplementation.cpp
- Tracks WebSocket connections (connect/disconnect)
- Tracks API call counts (total/success/failed)
- Records API-specific errors
- Records authentication service failures

### AppGatewayImplementation.cpp
- Records permission service failures

## Component Interaction Architecture

### 1. Initialization Flow

```
Plugin System (Thunder)
    │
    ├─▶ AppGateway::Initialize()
    │       │
    │       ├─▶ AppGatewayTelemetry::Instance()  [Creates Singleton]
    │       │       │
    │       │       ├─▶ Initialize timer (3600s interval)
    │       │       ├─▶ Initialize data structures
    │       │       └─▶ Arm timer
    │       │
    │       ├─▶ Load plugins (Badger, OttServices, etc.)
    │       │       │
    │       │       └─▶ External Plugin::Initialize()
    │       │               │
    │       │               └─▶ AGW_TELEMETRY_INIT(service)
    │       │                       │
    │       │                       └─▶ QueryInterface<IAppGatewayTelemetry>
    │       │                               │
    │       │                               └─▶ Returns COM-RPC proxy to AppGatewayTelemetry
    │       │
    │       └─▶ RecordBootstrapTime(duration, pluginCount)
    │               │
    │               └─▶ Immediately sends metrics to T2
    │
    └─▶ AppGateway is now operational
```

### 2. Runtime Data Flow (AppGateway Internal)

```
WebSocket Connection Event
    │
    ▼
AppGatewayResponderImplementation::SetAuthHandler()
    │
    ├─▶ AppGatewayTelemetry::Instance().IncrementWebSocketConnections()
    │       │
    │       └─▶ HealthStats.websocketConnections.fetch_add(1)  [Atomic, lock-free]
    │
    └─▶ Continue processing...

API Call Event
    │
    ▼
AppGatewayResponderImplementation::DispatchWsMsg()
    │
    ├─▶ AppGatewayTelemetry::Instance().IncrementTotalCalls()
    │       │
    │       └─▶ HealthStats.totalCalls.fetch_add(1)  [Atomic]
    │
    ├─▶ Process API call
    │   ├─▶ Success → IncrementSuccessfulCalls()  [Atomic]
    │   └─▶ Failure → IncrementFailedCalls() + RecordApiError(apiName)
    │                      │
    │                      └─▶ Lock mTelemetryLock
    │                          mApiErrorCounts[apiName]++
    │                          Unlock
    │
    └─▶ Return response
```

### 3. Runtime Data Flow (External Plugin via COM-RPC)

```
External Plugin (Badger)
    │
    ├─▶ API Call starts
    │       │
    │       └─▶ AGW_SCOPED_API_TIMER(timer, "GetDeviceSessionId")
    │               │
    │               └─▶ ScopedApiTimer::Constructor()  [Starts clock]
    │
    ├─▶ Call OttServices (external dependency)
    │   │
    │   └─▶ Failure detected
    │       │
    │       └─▶ AGW_REPORT_EXTERNAL_SERVICE_ERROR("OttServices", "TIMEOUT")
    │               │
    │               ├─▶ Build JSON: {"plugin":"Badger","service":"OttServices","error":"TIMEOUT"}
    │               │
    │               └─▶ IAppGatewayTelemetry::RecordTelemetryEvent(ctx, "AppGwPluginExtServiceError_split", json)
    │                       │ (COM-RPC call)
    │                       ▼
    │                   AppGatewayTelemetry::RecordTelemetryEvent()
    │                       │
    │                       ├─▶ Parse event name → contains "ExtServiceError"
    │                       ├─▶ Extract "service" from JSON → "OttServices"
    │                       ├─▶ Lock mTelemetryLock
    │                       ├─▶ mExternalServiceErrorCounts["OttServices"]++
    │                       ├─▶ Unlock
    │                       └─▶ Return ERROR_NONE
    │
    ├─▶ API Call returns
    │       │
    │       └─▶ ScopedApiTimer::Destructor()
    │               │
    │               ├─▶ Calculate latency (endTime - startTime)
    │               │
    │               └─▶ AGW_REPORT_API_LATENCY("GetDeviceSessionId", latencyMs)
    │                       │
    │                       └─▶ IAppGatewayTelemetry::RecordTelemetryMetric(ctx, "AppGwBadger_GetDeviceSessionId_Latency_split", latencyMs, "ms")
    │                               │ (COM-RPC call)
    │                               ▼
    │                           AppGatewayTelemetry::RecordMetric()
    │                               │
    │                               ├─▶ Lock mTelemetryLock
    │                               ├─▶ mMetricsCache["AppGwBadger_GetDeviceSessionId_Latency_split"].AddSample(latencyMs)
    │                               │       │
    │                               │       ├─▶ sum += latencyMs
    │                               │       ├─▶ min = std::min(min, latencyMs)
    │                               │       ├─▶ max = std::max(max, latencyMs)
    │                               │       └─▶ count++
    │                               │
    │                               ├─▶ Unlock
    │                               └─▶ Return ERROR_NONE
    │
    └─▶ Continue execution
```

### 4. Periodic Reporting Flow

```
TelemetryTimer (armed at 3600s interval)
    │
    │ ... time elapses ...
    │
    ▼
Timer::Dispatch()  [Timer thread]
    │
    └─▶ AppGatewayTelemetry::FlushTelemetryData()
            │
            ├─▶ Phase 1: Data Collection (under lock)
            │   │
            │   ├─▶ Lock mTelemetryLock
            │   ├─▶ Copy HealthStats (atomic reads)
            │   │       websocketConnections = 12
            │   │       totalCalls = 1543
            │   │       successfulCalls = 1520
            │   │       failedCalls = 23
            │   ├─▶ Copy mApiErrorCounts
            │   │       {"GetSettings": 5, "AuthorizeDataField": 3, ...}
            │   ├─▶ Copy mExternalServiceErrorCounts
            │   │       {"OttServices": 8, "ThorPermissionService": 15, ...}
            │   ├─▶ Copy mMetricsCache
            │   │       {"AppGwBadger_GetSettings_Latency_split": {sum:675, count:5, min:95, max:200}, ...}
            │   └─▶ Unlock mTelemetryLock
            │
            ├─▶ Phase 2: T2 Reporting (lock-free, using local copies)
            │   │
            │   ├─▶ SendHealthStats()
            │   │       │
            │   │       ├─▶ Utils::Telemetry::sendMessage("AppGwWebSocketConnections_split", {"sum":12,"count":1,"unit":"count"})
            │   │       ├─▶ Utils::Telemetry::sendMessage("AppGwTotalCalls_split", {"sum":1543,"count":1,"unit":"count"})
            │   │       ├─▶ Utils::Telemetry::sendMessage("AppGwSuccessfulCalls_split", {"sum":1520,"count":1,"unit":"count"})
            │   │       └─▶ Utils::Telemetry::sendMessage("AppGwFailedCalls_split", {"sum":23,"count":1,"unit":"count"})
            │   │
            │   ├─▶ SendApiErrorStats()
            │   │       │
            │   │       ├─▶ For each (apiName, count) in local copy:
            │   │       │   Utils::Telemetry::sendMessage("AppGwApiErrorCount_" + apiName + "_split", {"sum":count,"count":1,...})
            │   │       │
            │   │       ├─▶ sendMessage("AppGwApiErrorCount_GetSettings_split", {"sum":5,...})
            │   │       └─▶ sendMessage("AppGwApiErrorCount_AuthorizeDataField_split", {"sum":3,...})
            │   │
            │   ├─▶ SendExternalServiceErrorStats()
            │   │       │
            │   │       ├─▶ For each (serviceName, count):
            │   │       │   Utils::Telemetry::sendMessage("AppGwExtServiceErrorCount_" + serviceName + "_split", ...)
            │   │       │
            │   │       ├─▶ sendMessage("AppGwExtServiceErrorCount_OttServices_split", {"sum":8,...})
            │   │       └─▶ sendMessage("AppGwExtServiceErrorCount_ThorPermissionService_split", {"sum":15,...})
            │   │
            │   └─▶ SendAggregatedMetrics()
            │           │
            │           └─▶ For each (metricName, metricData):
            │               Utils::Telemetry::sendMessage(metricName, {
            │                   "sum": metricData.sum,
            │                   "count": metricData.count,
            │                   "min": metricData.min,
            │                   "max": metricData.max,
            │                   "unit": metricData.unit,
            │                   "reporting_interval_sec": 3600
            │               })
            │
            └─▶ Phase 3: Reset (under lock)
                │
                ├─▶ Lock mTelemetryLock
                ├─▶ mApiErrorCounts.clear()
                ├─▶ mExternalServiceErrorCounts.clear()
                ├─▶ mMetricsCache.clear()
                ├─▶ Unlock
                └─▶ Re-arm timer for next interval
```

### 5. Shutdown Flow

```
Plugin System
    │
    ├─▶ External Plugin::Deinitialize()
    │       │
    │       └─▶ AGW_TELEMETRY_DEINIT()
    │               │
    │               └─▶ IAppGatewayTelemetry::Release()
    │                       │
    │                       └─▶ Decrement COM-RPC reference count
    │
    └─▶ AppGateway::Deinitialize()
            │
            ├─▶ Disarm timer
            │
            ├─▶ AppGatewayTelemetry::Instance().FlushTelemetryData()
            │       │
            │       └─▶ Send all cached data to T2 (final report)
            │
            └─▶ Delete AppGatewayTelemetry singleton
                    │
                    └─▶ Cleanup all resources
```

## Data Flow Pattern Summary

### Synchronous Path (AppGateway Internal)
```
Event → Atomic Increment (lock-free) → HealthStats updated immediately
```

### Synchronous Path with Lock (Error Tracking)
```
Error → Lock → Map increment → Unlock (microseconds)
```

### Asynchronous Path (COM-RPC)
```
Plugin → COM-RPC call → AppGatewayTelemetry → Lock → Update data → Unlock → Return
                                                                            (microseconds)
```

### Periodic Path (Timer)
```
Timer → Copy under lock → Release lock → Send to T2 → Clear under lock
         (milliseconds)      (unlocked)    (seconds)    (milliseconds)
```

**Performance Characteristics:**
- **Hot Path** (atomic operations): Lock-free, O(1), nanoseconds
- **Warm Path** (error recording): Short lock, O(log N) map insert, microseconds  
- **Cold Path** (periodic reporting): Long-running T2 I/O, but lock-free during transmission, seconds
- **COM-RPC overhead**: Negligible compared to business logic, microseconds per call

## External Plugin Integration via COM-RPC

Other plugins (Badger, OttServices, etc.) can report telemetry to AppGateway using the `IAppGatewayTelemetry` interface via COM-RPC.

### COM-RPC Interface Definition

**Interface:** `Exchange::IAppGatewayTelemetry`  
**Access:** Via `INTERFACE_AGGREGATE` exposed by AppGateway plugin  
**Protocol:** WPEFramework COM-RPC (binary, efficient)

### Interface Methods

#### 1. RecordTelemetryEvent

**Signature:**
```cpp
Core::hresult RecordTelemetryEvent(const GatewayContext& context,
                                   const string& eventName,
                                   const string& eventData);
```

**Purpose:** Report discrete telemetry events (errors, state changes)

**Parameters:**
- `context`: Gateway context containing app/plugin identification
  - `appId`: Plugin name (e.g., "Badger", "OttServices")
  - `appInstanceId`: Instance identifier (typically empty for plugins)
- `eventName`: T2 marker name (generic category-based)
  - `"AppGwPluginApiError_split"` - API errors from any plugin
  - `"AppGwPluginExtServiceError_split"` - Service errors from any plugin
- `eventData`: JSON string with event details
  - Must include `"plugin"` field for generic markers
  - Additional fields: `"api"`, `"service"`, `"error"`, etc.

**Internal Processing:**
1. Parse `eventName` to determine event category
2. Extract relevant fields from `eventData` JSON
3. Route to appropriate aggregation mechanism:
   - If contains "ApiError" → `RecordApiError(apiName)`
   - If contains "ExtServiceError" → `RecordExternalServiceError(serviceName)`
   - Otherwise → Log and count generic event
4. Increment cache counter (triggers flush if threshold reached)
5. Return `Core::ERROR_NONE` or error code

**Thread Safety:** Method is COM-RPC thread-safe, internal operations use `mTelemetryLock`

#### 2. RecordTelemetryMetric

**Signature:**
```cpp
Core::hresult RecordTelemetryMetric(const GatewayContext& context,
                                    const string& metricName,
                                    const double metricValue,
                                    const string& metricUnit);
```

**Purpose:** Report numeric metrics for statistical aggregation

**Parameters:**
- `context`: Same as RecordTelemetryEvent
- `metricName`: Unique metric identifier (becomes T2 marker)
  - Pattern: `AppGw<Plugin>_<Api/Service>_Latency_split`
  - Examples: `"AppGwBadger_GetSettings_Latency_split"`, `"AppGwOttStreamingBitrate_split"`
- `metricValue`: Numeric value (double precision)
- `metricUnit`: Unit of measurement (`"ms"`, `"count"`, `"kbps"`, etc.)

**Internal Processing:**
1. Lock `mTelemetryLock`
2. Get or create `MetricData` entry in `mMetricsCache[metricName]`
3. Update aggregation:
   - `sum += metricValue`
   - `min = std::min(min, metricValue)`
   - `max = std::max(max, metricValue)`
   - `count++`
   - `unit = metricUnit`
4. Unlock
5. Check cache threshold, trigger flush if needed
6. Return `Core::ERROR_NONE`

**Thread Safety:** Protected by `mTelemetryLock`, safe for concurrent calls

### Plugin-Side Helper Layer (UtilsAppGatewayTelemetry.h)

**Purpose:** Macro-based facade to simplify COM-RPC telemetry integration

**Key Macros:**
- `AGW_TELEMETRY_INIT(service)` - Acquire IAppGatewayTelemetry interface
- `AGW_TELEMETRY_DEINIT()` - Release interface (RAII)
- `AGW_REPORT_API_ERROR(api, error)` - Report API failure
- `AGW_REPORT_EXTERNAL_SERVICE_ERROR(service, error)` - Report service failure
- `AGW_REPORT_API_LATENCY(api, latencyMs)` - Report API timing
- `AGW_REPORT_SERVICE_LATENCY(service, latencyMs)` - Report service timing
- `AGW_SCOPED_API_TIMER(var, api)` - Auto-track latency (RAII timer)

**Macro Benefits:**
- Eliminates COM-RPC boilerplate (15+ lines → 1 line)
- Automatic plugin name injection
- Automatic JSON construction
- Null-safe (checks interface availability)
- RAII resource management

**Design Decision:** Macros vs. Classes
- Macros chosen for zero overhead when telemetry disabled
- Entire code path optimized away at compile-time if feature disabled
- No virtual function overhead
- Inline expansion for maximum performance

### Example: Direct COM-RPC vs. Macro

**Direct COM-RPC (verbose):**
```cpp
auto telemetry = service->QueryInterface<Exchange::IAppGatewayTelemetry>(APPGATEWAY_CALLSIGN);
if (telemetry != nullptr) {
    Exchange::IAppGatewayTelemetry::GatewayContext ctx;
    ctx.appId = "Badger";
    ctx.appInstanceId = "";
    JsonObject data;
    data["plugin"] = "Badger";
    data["api"] = "GetUserProfile";
    data["error"] = "TIMEOUT";
    std::string eventData;
    data.ToString(eventData);
    telemetry->RecordTelemetryEvent(ctx, "AppGwPluginApiError_split", eventData);
    telemetry->Release();
}
```

**Macro (simplified):**
```cpp
AGW_REPORT_API_ERROR("GetUserProfile", "TIMEOUT");
```

**For detailed integration examples, see:**
- [Integration Guide](./AppGatewayTelemetryIntegrationGuide.md) - Step-by-step plugin integration
- [Sequence Diagrams](./sequence-diagrams/README.md) - Visual flow diagrams
- [Marker Reference](./AppGatewayTelemetryMarkers.md) - Complete marker catalog

## Dependencies

- `UtilsTelemetry.h` - T2 telemetry utility functions
- `UtilsAppGatewayTelemetry.h` - Telemetry helper macros for external plugins
- `Core::CriticalSection` - Thread synchronization
- `Core::TimerType<Core::IDispatch>` - Periodic timer
- `Core::ProxyType` - Smart pointer for timer dispatch

## Configuration

| Parameter | Default | Description | Configurable |
|-----------|---------|-------------|--------------|
| `TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC` | 3600 (1 hour) | Interval between telemetry reports | Yes (plugin config) |
| `TELEMETRY_DEFAULT_CACHE_THRESHOLD` | 1000 | Max cached events/metrics before forced flush | Yes (compile-time constant) |
| Bootstrap reporting | Immediate | Bootstrap metrics sent immediately | No |
| Health stats | Gauges | Not reset between reports (current state) | No |
| Error counts | Counters | Reset to zero after each report | No |
| Metrics | Aggregated | Reset after each report | No |

### Configuration via Plugin Config

**File:** `AppGateway.json` (or equivalent Thunder configuration)

```json
{
  "telemetry": {
    "reportingInterval": 3600,
    "cacheThreshold": 1000,
    "enabled": true
  }
}
```

## Telemetry Markers

All telemetry data is sent as **individual numeric METRICS** for statistical aggregation:

| Metric Name Pattern | Category | Description | Reset Policy |
|---------------------|----------|-------------|--------------|
| `AppGwBootstrapDuration_split` | Bootstrap | Duration in milliseconds | Once per boot |
| `AppGwBootstrapPluginCount_split` | Bootstrap | Number of plugins loaded | Once per boot |
| `AppGwWebSocketConnections_split` | Health | Current active connections | Gauge (not reset) |
| `AppGwTotalCalls_split` | Health | Total API calls in period | Gauge (not reset) |
| `AppGwSuccessfulCalls_split` | Health | Successful calls in period | Gauge (not reset) |
| `AppGwFailedCalls_split` | Health | Failed calls in period | Gauge (not reset) |
| `AppGwApiErrorCount_<ApiName>_split` | Errors | Error count per API | Reset after report |
| `AppGwExtServiceErrorCount_<ServiceName>_split` | Errors | Error count per service | Reset after report |
| `AppGw<Plugin>_<Api>_Latency_split` | Latency | API call latency (aggregated) | Reset after report |
| `AppGw<Plugin>_<Service>_Latency_split` | Latency | Service call latency (aggregated) | Reset after report |

Each metric is sent with aggregation data: `sum`, `count`, `min`, `max`, `unit`, `reporting_interval_sec`.

## Class Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    AppGatewayTelemetry                           │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   Private Members                          │  │
│  ├───────────────────────────────────────────────────────────┤  │
│  │  - static AppGatewayTelemetry* sInstance                  │  │
│  │  - Core::CriticalSection mTelemetryLock                   │  │
│  │  - HealthStats mHealthStats                               │  │
│  │  - std::map<string, uint32_t> mApiErrorCounts             │  │
│  │  - std::map<string, uint32_t> mExternalServiceErrorCounts │  │
│  │  - std::map<string, MetricData> mMetricsCache             │  │
│  │  - Core::TimerType<Core::IDispatch> mTelemetryTimer       │  │
│  │  - uint32_t mCachedEventCount                             │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   Public Interface                         │  │
│  ├───────────────────────────────────────────────────────────┤  │
│  │  + static AppGatewayTelemetry& Instance()                 │  │
│  │  + void RecordBootstrapTime(uint64_t, uint32_t)           │  │
│  │  + void IncrementWebSocketConnections()                   │  │
│  │  + void DecrementWebSocketConnections()                   │  │
│  │  + void IncrementTotalCalls()                             │  │
│  │  + void IncrementSuccessfulCalls()                        │  │
│  │  + void IncrementFailedCalls()                            │  │
│  │  + void RecordApiError(const string&)                     │  │
│  │  + void RecordExternalServiceError(const string&)         │  │
│  │  + void RecordMetric(const string&, double, const string&)│  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │             COM-RPC Interface (IAppGatewayTelemetry)       │  │
│  ├───────────────────────────────────────────────────────────┤  │
│  │  + Core::hresult RecordTelemetryEvent(...)                │  │
│  │  + Core::hresult RecordTelemetryMetric(...)               │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   Private Methods                          │  │
│  ├───────────────────────────────────────────────────────────┤  │
│  │  - void FlushTelemetryData()                              │  │
│  │  - void SendHealthStats()                                 │  │
│  │  - void SendApiErrorStats()                               │  │
│  │  - void SendExternalServiceErrorStats()                   │  │
│  │  - void SendAggregatedMetrics()                           │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                             │
                             │ uses
                             ▼
┌─────────────────────────────────────────────┐
│        HealthStats (struct)                 │
├─────────────────────────────────────────────┤
│  + std::atomic<uint32_t> websocketConnections
│  + std::atomic<uint32_t> totalCalls        │
│  + std::atomic<uint32_t> successfulCalls   │
│  + std::atomic<uint32_t> failedCalls       │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│        MetricData (struct)                  │
├─────────────────────────────────────────────┤
│  + double sum                               │
│  + double min                               │
│  + double max                               │
│  + uint32_t count                           │
│  + std::string unit                         │
│  + void AddSample(double value)             │
│  + double GetAverage() const                │
│  + void Reset()                             │
└─────────────────────────────────────────────┘
```

## Architecture Summary

### Key Design Decisions

1. **Singleton Pattern**: Ensures single telemetry aggregation point across entire AppGateway ecosystem
2. **Generic Markers**: Reduces marker proliferation (2 generic markers vs. N*2 plugin-specific markers)
3. **Metric Aggregation**: Reduces T2 load (1 metric per reporting period vs. N individual events)
4. **Lock-free Hot Path**: Atomic operations for high-frequency counters (WebSocket, API calls)
5. **Copy-on-read**: Minimizes lock hold time during T2 I/O operations
6. **RAII Helpers**: Macros provide zero-overhead abstraction for plugins
7. **Periodic Reporting**: Batches telemetry to reduce T2 server load and network traffic

### Performance Characteristics

| Operation | Complexity | Thread Safety | Typical Duration |
|-----------|-----------|---------------|------------------|
| Increment atomic counter | O(1) | Lock-free | 1-10 nanoseconds |
| Record API error | O(log N) | Mutex-protected | 100-500 nanoseconds |
| Record metric sample | O(log N) | Mutex-protected | 100-500 nanoseconds |
| COM-RPC call overhead | O(1) | Thread-safe | 1-5 microseconds |
| Flush telemetry data | O(N) | Mutex-protected | 10-100 milliseconds |
| T2 network I/O per metric | O(1) | Lock-free (async) | 50-200 milliseconds |

Where N = number of unique APIs/services/metrics tracked

### Scalability

- **Multiple Producers**: Atomic operations allow unlimited concurrent producers for health stats
- **Lock Contention**: Minimal due to short critical sections and infrequent error reporting
- **Memory Footprint**: O(APIs + Services + Metrics) - typically < 1MB for 100+ tracked entities
- **T2 Load**: Bounded by reporting interval * unique metrics (e.g., 100 metrics/hour = 0.027 metrics/sec)

---

## Further Reading

For implementation details, integration steps, and usage examples:
- **[Integration Guide](./AppGatewayTelemetryIntegrationGuide.md)** - Complete integration walkthrough
- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual interaction flows
- **[Marker Reference](./AppGatewayTelemetryMarkers.md)** - Comprehensive marker catalog
- **[UtilsAppGatewayTelemetry.h](../helpers/UtilsAppGatewayTelemetry.h)** - Macro implementation source