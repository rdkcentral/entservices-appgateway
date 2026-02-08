# App Gateway T2 Telemetry Architecture

## Overview

The App Gateway Telemetry module provides comprehensive telemetry collection and reporting for the App Gateway service. It tracks bootstrap time, WebSocket connections, API call statistics, and external service errors, sending individual numeric metrics to the T2 telemetry server at configurable intervals for statistical aggregation and trending.

## Documentation

- **[Integration Guide](./AppGatewayTelemetryIntegrationGuide.md)** - Step-by-step guide for plugin developers
- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual flows for all telemetry scenarios
- **[Marker Reference](./AppGatewayTelemetryMarkers.md)** - Complete list of predefined markers

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              App Gateway Plugin                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                           AppGateway.cpp                                 │   │
│  │  ┌─────────────────┐    ┌─────────────────┐    ┌───────────────────┐   │   │
│  │  │   Initialize()  │───▶│ Bootstrap Time  │───▶│ RecordBootstrap   │   │   │
│  │  │   Deinitialize()│    │   Measurement   │    │     Time()        │   │   │
│  │  └─────────────────┘    └─────────────────┘    └─────────┬─────────┘   │   │
│  └──────────────────────────────────────────────────────────┼─────────────┘   │
│                                                              │                  │
│  ┌───────────────────────────────────────────────────────────┼─────────────┐   │
│  │                 AppGatewayResponderImplementation.cpp     │             │   │
│  │  ┌─────────────────┐    ┌─────────────────┐              │             │   │
│  │  │  SetAuthHandler │───▶│ IncrementWS     │──────────────┤             │   │
│  │  │    (connect)    │    │  Connections()  │              │             │   │
│  │  └─────────────────┘    └─────────────────┘              │             │   │
│  │  ┌─────────────────┐    ┌─────────────────┐              │             │   │
│  │  │SetDisconnect    │───▶│ DecrementWS     │──────────────┤             │   │
│  │  │   Handler       │    │  Connections()  │              │             │   │
│  │  └─────────────────┘    └─────────────────┘              │             │   │
│  │  ┌─────────────────┐    ┌─────────────────┐              │             │   │
│  │  │ DispatchWsMsg() │───▶│ IncrementTotal/ │──────────────┤             │   │
│  │  │                 │    │ Success/Failed  │              │             │   │
│  │  │                 │───▶│ RecordApiError()│──────────────┤             │   │
│  │  └─────────────────┘    └─────────────────┘              │             │   │
│  │                         ┌─────────────────┐              │             │   │
│  │  Auth Failure ─────────▶│RecordExtService │──────────────┤             │   │
│  │                         │    Error()      │              │             │   │
│  │                         └─────────────────┘              │             │   │
│  └───────────────────────────────────────────────────────────┼─────────────┘   │
│                                                              │                  │
│  ┌───────────────────────────────────────────────────────────┼─────────────┐   │
│  │                   AppGatewayImplementation.cpp            │             │   │
│  │                         ┌─────────────────┐              │             │   │
│  │  Permission Check ─────▶│RecordExtService │──────────────┤             │   │
│  │     Failure             │    Error()      │              │             │   │
│  │                         └─────────────────┘              │             │   │
│  └───────────────────────────────────────────────────────────┼─────────────┘   │
│                                                              │                  │
│                                                              ▼                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                    AppGatewayTelemetry (Singleton)                       │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │                      Data Aggregation Layer                      │   │   │
│  │  │  ┌──────────────┐ ┌──────────────┐ ┌────────────────────────┐  │   │   │
│  │  │  │ HealthStats  │ │ ApiError     │ │ ExternalServiceError   │  │   │   │
│  │  │  │  (atomic)    │ │   Counts     │ │       Counts           │  │   │   │
│  │  │  │ ─websocket   │ │  (map)       │ │       (map)            │  │   │   │
│  │  │  │ ─totalCalls  │ │              │ │                        │  │   │   │
│  │  │  │ ─successCalls│ │              │ │                        │  │   │   │
│  │  │  │ ─failedCalls │ │              │ │                        │  │   │   │
│  │  │  └──────────────┘ └──────────────┘ └────────────────────────┘  │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                                                         │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │                    Timer & Reporting Layer                       │   │   │
│  │  │  ┌──────────────────┐    ┌──────────────────────────────────┐  │   │   │
│  │  │  │  TelemetryTimer  │───▶│     FlushTelemetryData()         │  │   │   │
│  │  │  │ (1 hour default) │    │  ─SendHealthStats()              │  │   │   │
│  │  │  │                  │    │  ─SendApiErrorStats()            │  │   │   │
│  │  │  │ Cache Threshold  │    │  ─SendExternalServiceErrorStats()│  │   │   │
│  │  │  │ (1000 records)   │    │  ─ResetCounters()                │  │   │   │
│  │  │  └──────────────────┘    └──────────────────────────────────┘  │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                       │                                 │   │
│  │                                       ▼                                 │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │                      SendT2Event()                               │   │   │  │  │                                                                   │   │   │
  │  │  Individual metrics sent to T2:                                  │   │   │
  │  │  - AppGwBootstrapDuration_split                                 │   │   │
  │  │  - AppGwBootstrapPluginCount_split                              │   │   │
  │  │  - AppGwWebSocketConnections_split                              │   │   │
  │  │  - AppGwTotalCalls_split                                        │   │   │
  │  │  - AppGwSuccessfulCalls_split                                   │   │   │
  │  │  - AppGwFailedCalls_split                                       │   │   │
  │  │  - AppGwApiErrorCount_<ApiName>_split (per API)                │   │   │
  │  │  - AppGwExtServiceErrorCount_<Service>_split (per service)      │   │   │
  │  │                                                                   │   │   ││  │  │              Utils::Telemetry::sendMessage()                     │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
                    ┌─────────────────────────────────────┐
                    │         T2 Telemetry Server         │
                    │  ┌───────────────────────────────┐  │
                    │  │  t2_event_s(marker, payload)  │  │
                    │  └───────────────────────────────┘  │
                    └─────────────────────────────────────┘
```

## Component Description

### 1. AppGatewayTelemetry (Singleton)

The core telemetry aggregator class that:
- Uses singleton pattern for easy access from all components
- Aggregates telemetry data in thread-safe manner using `Core::CriticalSection` and `std::atomic`
- Manages periodic reporting via `TelemetryTimer`
- Reports to T2 server using `Utils::Telemetry::sendMessage()`

### 2. Data Structures

| Structure | Type | Description |
|-----------|------|-------------|
| `HealthStats` | struct with atomics | Tracks WebSocket connections, total/success/failed calls |
| `mApiErrorCounts` | `std::map<string, uint32_t>` | Maps API names to their failure counts |
| `mExternalServiceErrorCounts` | `std::map<string, uint32_t>` | Maps service names to their failure counts |

### 3. Telemetry Markers

All telemetry data is sent as **individual numeric METRICS** for statistical aggregation:

| Metric Name | Scenario | Description |
|-------------|----------|-------------|
| `AppGwBootstrapDuration_split` | Bootstrap Time | Duration in milliseconds |
| `AppGwBootstrapPluginCount_split` | Bootstrap Time | Number of plugins loaded |
| `AppGwWebSocketConnections_split` | Health Stats | Current active connections |
| `AppGwTotalCalls_split` | Health Stats | Total API calls in period |
| `AppGwSuccessfulCalls_split` | Health Stats | Successful calls in period |
| `AppGwFailedCalls_split` | Health Stats | Failed calls in period |
| `AppGwApiErrorCount_<ApiName>_split` | API Errors | Error count per API |
| `AppGwExtServiceErrorCount_<ServiceName>_split` | Service Errors | Error count per service |

Each metric is sent with aggregation data: `sum`, `count`, `min` (for future use), `max` (for future use), `unit`, `reporting_interval_sec`.

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TELEMETRY_DEFAULT_REPORTING_INTERVAL_SEC` | 3600 (1 hour) | Interval between telemetry reports |
| `TELEMETRY_DEFAULT_CACHE_THRESHOLD` | 1000 | Max records before forced flush |

## Thread Safety

- `HealthStats` uses `std::atomic<uint32_t>` for lock-free counter updates
- API error and external service error maps protected by `Core::CriticalSection`
- Timer operations are synchronized through the lock

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

## External Plugin Integration via COM-RPC

Other plugins (Badger, OttServices, etc.) can report telemetry to AppGateway using the `IAppGatewayTelemetry` interface via COM-RPC. The interface provides two methods:

### RecordTelemetryEvent

```cpp
Core::hresult RecordTelemetryEvent(const GatewayContext& context,
                                   const string& eventName,
                                   const string& eventData);
```

**Usage:** For event-based telemetry (API errors, service failures, etc.)

**Parameters:**
- `context`: Gateway context with app info (`appId`, `appInstanceId`)
- `eventName`: Event name - acts as T2 marker (e.g., `"AppGwPluginApiError_split"`, `"AppGwPluginExtServiceError_split"`)
- `eventData`: JSON string with event details (must include `"plugin"` field for generic markers)

**Note:** The system now uses **generic category-based markers** where the plugin name is included in the event data payload rather than the marker name itself. This avoids marker duplication across plugins.

**Example Usage from Badger Plugin (Direct COM-RPC):**
```cpp
// Get AppGateway telemetry interface
auto telemetry = service->QueryInterface<Exchange::IAppGatewayTelemetry>();
if (telemetry != nullptr) {
    Exchange::IAppGatewayTelemetry::GatewayContext ctx;
    ctx.appId = "Badger";
    ctx.appInstanceId = "";
    
    // Report API error using generic marker
    JsonObject data;
    data["plugin"] = "Badger";  // Plugin name in payload
    data["api"] = "GetUserProfile";
    data["error"] = "TIMEOUT";
    std::string eventData;
    data.ToString(eventData);
    
    telemetry->RecordTelemetryEvent(ctx, "AppGwPluginApiError_split", eventData);
    telemetry->Release();
}
```

**Example Usage from Badger Plugin (Using MACRO - Recommended):**
```cpp
// Using helper macro - much simpler!
AGW_REPORT_API_ERROR("GetUserProfile", AGW_ERROR_TIMEOUT);

// Single line replaces 15+ lines of COM-RPC boilerplate
// ✓ No manual interface query needed
// ✓ No GatewayContext creation
// ✓ No JSON construction
// ✓ Automatic plugin name insertion
// ✓ Automatic error checking
// ✓ Automatic resource cleanup
```

**Impact Comparison:**
- **Lines of Code**: 15+ lines → 1 line (93% reduction)
- **Error Handling**: Manual null checks → Automatic safety checks
- **Maintainability**: Plugin name hardcoded in multiple places → Centralized in one constant
- **Readability**: Complex JSON construction → Self-documenting macro call
- **Memory Safety**: Manual Release() → Automatic RAII cleanup

### RecordTelemetryMetric

```cpp
Core::hresult RecordTelemetryMetric(const GatewayContext& context,
                                    const string& metricName,
                                    const double metricValue,
                                    const string& metricUnit);
```

**Usage:** For numeric metrics that need aggregation (latency, bitrate, counts, etc.)

**Parameters:**
- `context`: Gateway context with app info (`appId`, `appInstanceId`)
- `metricName`: Metric name - acts as T2 marker (e.g., `"AppGwBadgerApiLatency_split"`, `"AppGwOttStreamingBitrate_split"`)
- `metricValue`: Numeric value
- `metricUnit`: Unit of measurement (e.g., `"ms"`, `"kbps"`, `"count"`)

**Aggregation:** Metrics are aggregated (sum, min, max, count, avg) and reported periodically.

**Example Usage from OttServices Plugin (Direct COM-RPC):**
```cpp
// Get AppGateway telemetry interface
auto telemetry = service->QueryInterface<Exchange::IAppGatewayTelemetry>();
if (telemetry != nullptr) {
    Exchange::IAppGatewayTelemetry::GatewayContext ctx;
    ctx.appId = "OttServices";
    ctx.appInstanceId = "";
    
    // Report streaming bitrate metric
    telemetry->RecordTelemetryMetric(ctx, "AppGwOttStreamingBitrate_split", 4500.0, "kbps");
    
    // Report API latency metric  
    telemetry->RecordTelemetryMetric(ctx, "AppGwOttApiLatency_split", 150.0, "ms");
    telemetry->Release();
}
```

**Example Usage from OttServices Plugin (Using MACRO - Recommended):**
```cpp
// Option 1: Auto-track API latency with scoped timer (RAII)
Core::hresult OttServices::GetAppPermissions(const JsonObject& params, JsonObject& response)
{
    AGW_SCOPED_API_TIMER(timer, "GetAppPermissions");
    
    // Your API implementation here
    // Latency automatically reported when timer goes out of scope
    // Generates metric: AppGwOttServices_GetAppPermissions_Latency_split
    
    return Core::ERROR_NONE;
}

// Option 2: Manually report external service latency
auto startTime = Core::Time::Now().Ticks();
auto grpcResult = thorPermissionClient->CheckPermission(request);
auto latencyMs = (Core::Time::Now().Ticks() - startTime) / Core::Time::TicksPerMillisecond;
AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_THOR_PERMISSION, latencyMs);
// Generates metric: AppGwOttServices_ThorPermissionService_Latency_split

// Option 3: Report custom metric
AGW_REPORT_METRIC("AppGwOttStreamingBitrate_split", 4500.0, "kbps");

// Each macro is one line vs 10+ lines of COM-RPC code
// ✓ Automatic interface management
// ✓ Automatic metric name composition (plugin + api/service)
// ✓ Type-safe parameters
// ✓ Scoped lifetime management for timers
```

**Impact Comparison:**
- **Latency Tracking**: Manual timing + COM-RPC (10+ lines) → `AGW_SCOPED_API_TIMER` (1 line, RAII)
- **Metric Naming**: Manual string composition → Automatic (plugin + api/service + suffix)
- **Resource Management**: Manual QueryInterface/Release → Automatic via macros
- **Code Readability**: Intent obscured by boilerplate → Intent clear and explicit
- **Type Safety**: String-based units → Compile-time constant validation (when using predefined constants)

### Event Name Conventions

For proper routing within AppGatewayTelemetry, use these naming patterns:

| Pattern | Description | Internal Handling |
|---------|-------------|-------------------|
| `*ApiError` | API errors | Tracked in API error stats |
| `*ExternalServiceError` | External service failures | Tracked in external service error stats |
| Other names | Generic events | Logged and counted |

## Dependencies

- `UtilsTelemetry.h` - T2 telemetry utility functions
- `UtilsAppGatewayTelemetry.h` - Telemetry helper macros for external plugins
- `Core::CriticalSection` - Thread synchronization
- `Core::TimerType<Core::IDispatch>` - Periodic timer
- `Core::ProxyType` - Smart pointer for timer dispatch

---

## Reference Implementation: Using Telemetry Macros

For engineers implementing telemetry in other plugins, use the `UtilsAppGatewayTelemetry.h` helper macros instead of directly calling the COM-RPC interface. This provides:

- **Simplified API** - Single-line macros for common operations
- **Automatic context handling** - No need to create GatewayContext manually
- **Error-safe** - Macros check availability before calling
- **Consistent naming** - Standardized event/metric name formats

### Quick Start

```cpp
#include "UtilsAppGatewayTelemetry.h"

// 1. Initialize in your plugin's Initialize()
AGW_TELEMETRY_INIT(mService, "YourPluginName");

// 2. Report errors/metrics throughout your code
AGW_REPORT_API_ERROR("GetSettings", "TIMEOUT");
AGW_REPORT_EXTERNAL_SERVICE_ERROR("ThorPermissionService", "CONNECTION_REFUSED");
AGW_REPORT_METRIC("AppGwYourPluginApiLatency_split", 150.5, "ms");

// 3. Deinitialize in your plugin's Deinitialize()
AGW_TELEMETRY_DEINIT();
```

### Available Macros

| Macro | Purpose | Parameters |
|-------|---------|------------|
| `AGW_TELEMETRY_INIT(service, pluginName)` | Initialize telemetry client | IShell*, plugin name constant (e.g., AGW_PLUGIN_BADGER) |
| `AGW_TELEMETRY_DEINIT()` | Release telemetry interface | None |
| `AGW_TELEMETRY_AVAILABLE()` | Check if telemetry is available | Returns bool |
| `AGW_REPORT_API_ERROR(apiName, error)` | Report API failure (uses generic marker) | API name, error details |
| `AGW_REPORT_EXTERNAL_SERVICE_ERROR(service, error)` | Report external service failure (uses generic marker) | Service name, error details |
| `AGW_REPORT_API_LATENCY(apiName, latencyMs)` | Report API latency metric | API name, latency in ms |
| `AGW_REPORT_SERVICE_LATENCY(serviceName, latencyMs)` | Report service latency metric | Service name, latency in ms |
| `AGW_REPORT_METRIC(name, value, unit)` | Report custom numeric metric | Metric name, value, unit |
| `AGW_REPORT_EVENT(name, data)` | Report custom event | Event name, JSON data |
| `AGW_SCOPED_API_TIMER(var, apiName)` | Auto-track API latency (RAII) | Variable name, API name |

### Complete Example: Badger Plugin

```cpp
// Badger.cpp - Complete telemetry integration example

#include "Badger.h"
#include <helpers/UtilsAppGatewayTelemetry.h>
#include <helpers/AppGatewayTelemetryMarkers.h>

// Define telemetry client for this plugin
AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_BADGER)

const string Badger::Initialize(PluginHost::IShell* service) {
    mService = service;
    mService->AddRef();

    // Initialize AppGateway telemetry client
    AGW_TELEMETRY_INIT(mService, AGW_PLUGIN_BADGER);

    return EMPTY_STRING;
}

void Badger::Deinitialize(PluginHost::IShell* service) {
    // Deinitialize telemetry before cleanup
    AGW_TELEMETRY_DEINIT();

    // ... rest of cleanup ...
}

std::string Badger::GetDeviceSessionId(const Exchange::GatewayContext& context, const string& appId) {
    // Track API latency using scoped timer
    AGW_SCOPED_API_TIMER(timer, "GetDeviceSessionId");
    
    string deviceSessionId = "app_session_id.not.set";

    if (!mDelegateFactory) {
        LOGERR("DelegateFactory not initialized.");
        timer.SetFailed(AGW_ERROR_NOT_AVAILABLE);
        return deviceSessionId;
    }
    
    auto lifecycle = mDelegateFactory->getDelegate<LifecycleDelegate>();
    if (!lifecycle) {
        LOGERR("LifecycleDelegate not available.");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_LIFECYCLE_DELEGATE, AGW_ERROR_NOT_AVAILABLE);
        timer.SetFailed(AGW_ERROR_NOT_AVAILABLE);
        return deviceSessionId;
    }
    
    if (lifecycle->GetDeviceSessionId(context, deviceSessionId) != Core::ERROR_NONE) {
        LOGERR("Failed to get device session ID");
        AGW_REPORT_API_ERROR("GetDeviceSessionId", AGW_ERROR_FETCH_FAILED);
        timer.SetFailed(AGW_ERROR_FETCH_FAILED);
        return deviceSessionId;
    }
    
    return deviceSessionId;
    // Timer automatically reports success latency on destruction
}

Core::hresult Badger::AuthorizeDataField(const std::string& appId, const char* field) {
    // Track API latency
    AGW_SCOPED_API_TIMER(apiTimer, "AuthorizeDataField");
    
    // ... check cache first ...
    
    // Track external service call latency
    auto serviceCallStart = std::chrono::steady_clock::now();
    
    Exchange::IOttServices* ottServices = GetOttServices();
    if (ottServices == nullptr) {
        LOGERR("OttServices interface not available");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE);
        apiTimer.SetFailed(AGW_ERROR_INTERFACE_UNAVAILABLE);
        return Core::ERROR_UNAVAILABLE;
    }

    RPC::IStringIterator* permissions = nullptr;
    if (ottServices->GetAppPermissions(appId, false, permissions) != Core::ERROR_NONE) {
        LOGERR("GetAppPermissions failed for appId=%s", appId.c_str());
        AGW_REPORT_API_ERROR("GetAppPermissions", AGW_ERROR_PERMISSION_DENIED);
        apiTimer.SetFailed(AGW_ERROR_PERMISSION_DENIED);
        return Core::ERROR_PRIVILIGED_REQUEST;
    }
    
    // Track external service call latency
    auto serviceCallEnd = std::chrono::steady_clock::now();
    auto serviceLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        serviceCallEnd - serviceCallStart).count();
    AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_SERVICES, static_cast<double>(serviceLatencyMs));

    // ... success path ...
    return Core::ERROR_NONE;
}
```

### Complete Example: OttServices Plugin

```cpp
// OttServicesImplementation.cpp - Complete telemetry integration example

#include "OttServicesImplementation.h"
#include "UtilsAppGatewayTelemetry.h"

string OttServicesImplementation::Initialize(PluginHost::IShell* service) {
    _service = service;

    // Initialize AppGateway telemetry
    AGW_TELEMETRY_INIT(_service, "OttServices");

    return string();
}

void OttServicesImplementation::Deinitialize(PluginHost::IShell* service) {
    AGW_TELEMETRY_DEINIT();
    // ... rest of cleanup ...
}

Core::hresult OttServicesImplementation::GetAppCIMAToken(const string& appId, string& token) const {
    // Example: Report token service failures
    if (!FetchSat(sat, satExpiry)) {
        LOGERR("FetchSat failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR("AuthService", "FETCH_SAT_FAILED");
        return Core::ERROR_UNAVAILABLE;
    }

    std::string err;
    if (!_token->GetPlatformToken(appId, xact, sat, token, expiresInSec, err)) {
        LOGERR("GetPlatformToken failed: %s", err.c_str());
        AGW_REPORT_EXTERNAL_SERVICE_ERROR("OttTokenService", err.c_str());
        return Core::ERROR_UNAVAILABLE;
    }

    return Core::ERROR_NONE;
}

Core::hresult OttServicesImplementation::GetAppPermissions(...) const {
    // Example: Report Thor Permission Service failures
    if (!_perms) {
        AGW_REPORT_EXTERNAL_SERVICE_ERROR("ThorPermissionService", "CLIENT_NOT_INITIALIZED");
        return Core::ERROR_UNAVAILABLE;
    }

    if (UpdatePermissions(appId, permissionsList) != Core::ERROR_NONE) {
        AGW_REPORT_EXTERNAL_SERVICE_ERROR("ThorPermissionService", "UPDATE_PERMISSIONS_FAILED");
        return Core::ERROR_UNAVAILABLE;
    }

    return Core::ERROR_NONE;
}
```

### Using Scoped Timer for Automatic Latency Tracking

```cpp
Core::hresult MyPlugin::SomeApiCall() {
    // Automatically tracks timing and reports success/failure
    AGW_SCOPED_API_TIMER(timer, "SomeApiCall");

    auto result = DoSomeWork();
    if (result != Core::ERROR_NONE) {
        timer.SetFailed("WORK_FAILED");  // Mark as failed
        return result;
    }

    // Timer automatically reports success with timing on scope exit
    return Core::ERROR_NONE;
}
```

### Best Practices

1. **Always initialize in Initialize()** - Call `AGW_TELEMETRY_INIT()` early in your plugin's initialization
2. **Always deinitialize in Deinitialize()** - Call `AGW_TELEMETRY_DEINIT()` to release resources
3. **Use descriptive service names** - e.g., `"ThorPermissionService"`, `"OttTokenService"`, `"AuthService"`
4. **Use descriptive error details** - e.g., `"CONNECTION_REFUSED"`, `"TIMEOUT"`, `"INVALID_RESPONSE"`
5. **Report at failure points** - Add telemetry calls wherever errors are logged
6. **Use consistent naming** - Follow the patterns in this document

### Service Name Registry

| Service Name | Description | Used By |
|--------------|-------------|---------|
| `ThorPermissionService` | Thor Permission gRPC service | OttServices |
| `OttTokenService` | OTT Token gRPC service | OttServices |
| `AuthService` | AuthService for SAT/xACT tokens | OttServices |
| `OttServices` | OttServices COM-RPC interface | Badger |
| `LaunchDelegate` | Launch Delegate for session management | Badger |
| `LifecycleDelegate` | Lifecycle delegate for device session | Badger |
| `PermissionService` | Internal permission check service | AppGateway |
| `AuthenticationService` | WebSocket authentication service | AppGateway |

