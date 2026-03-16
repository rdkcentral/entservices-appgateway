# App Gateway Telemetry Integration Guide

## Overview

This guide is for developers of **external plugins** and **services** who need to report telemetry data to App Gateway for T2 (Telemetry 2.0) aggregation and reporting.

App Gateway provides a centralized telemetry collection mechanism that:
- **Events (Optional)**: Plugins can report individual API/service errors with full JSON context for immediate forensics
- **Metrics (Required)**: AppGateway aggregates error counts, latencies, and health stats, then periodically sends individual numeric metrics to T2
- Reports at configurable intervals (default: hourly) with statistical aggregation (sum, count, min, max, avg)
- Enables trending, alerting, and dashboard analytics

## Quick Links

- **[Sequence Diagrams](./sequence-diagrams/README.md)** - Visual flows for all telemetry scenarios
- **[Marker Reference](./AppGatewayTelemetryMarkers.md)** - Complete list of predefined markers and constants
- **[Architecture Overview](./AppGatewayTelemetry_Architecture.md)** - System architecture and design

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          App Gateway Ecosystem                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                     │
│  │ Plugin_Name_1│   │ Plugin_Name_2│   │  YourPlugin  │                     │
│  │              │   │              │   │   (New)      │                     │
│  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘                     │
│         │                  │                  │                             │
│         │  COM-RPC         │  COM-RPC         │  COM-RPC                    │
│         │  (Events/Metrics)│  (Events/Metrics)│  (Events/Metrics)           │
│         ▼                  ▼                  ▼                             │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │              IAppGatewayTelemetry Interface                 │            │
│  │  ┌─────────────────────────────────────────────────────────┐│            │
│  │  │ RecordTelemetryEvent(context, eventName, eventData)     ││            │
│  │  │  → Optional: Immediate error reporting with JSON        ││            │
│  │  │ RecordTelemetryMetric(context, metricName, value, unit) ││            │
│  │  │  → Required: Latency tracking (automatic aggregation)   ││            │
│  │  └─────────────────────────────────────────────────────────┘│            │
│  └─────────────────────────────────────────────────────────────┘            │
│                              │                                              │
│                              ▼                                              │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │         AppGatewayTelemetry (Aggregator + Reporter)         │            │
│  │  • Counts API errors by API name (increments counters)      │            │
│  │  • Counts external service errors by service name           │            │
│  │  • Aggregates latency metrics (sum, count, min, max)        │            │
│  │  • Tracks health stats (connections, calls)                 │            │
│  │  • Sends individual numeric metrics to T2 periodically      │            │
│  │    (default: 1 hour interval)                               │            │
│  └──────────────────────────┬──────────────────────────────────┘            │
│                             │                                               │
│                             ▼                                               │
│          ┌─────────────────────────────────────────┐                        │
│          │   T2 Service                            │                        │
│          │  • ENTS_INFO_AppGwTotalCalls                │                        │
│          │  • AppGwApiErrorCount_<Api>_split       │                        │
│          │  • AppGw<Plugin>_<Api>_Latency_split    │                        │
│          └─────────────────────────────────────────┘                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Quick Start

### Step 1: Include Required Headers

```cpp
#include "UtilsAppGatewayTelemetry.h"
#include "AppGatewayTelemetryMarkers.h"
```

### Step 2: Define Telemetry Client Instance

In your plugin's implementation file (`.cpp`), at the top level **before any namespace declarations**:

```cpp
#include "UtilsAppGatewayTelemetry.h"

AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_YOURPLUGIN)

namespace WPEFramework {
namespace Plugin {
    // ... your plugin implementation ...
}}
```

**Important:** This macro creates a plugin-specific telemetry client instance. Each plugin gets its own isolated instance to prevent cross-plugin data contamination.

### Step 3: Initialize Telemetry and Track Bootstrap

In your plugin's `Initialize()` method:

```cpp
uint32_t YourPlugin::Initialize(PluginHost::IShell* service)
{
    // ... your initialization code ...
    
    // Initialize telemetry client FIRST (queries IAppGatewayTelemetry interface)
    AGW_TELEMETRY_INIT(service);
    
    // Track bootstrap time - MUST come AFTER AGW_TELEMETRY_INIT
    AGW_RECORD_BOOTSTRAP_TIME();
    
    return Core::ERROR_NONE;
    // Bootstrap time automatically recorded when scope exits
}
```

### Step 4: Report Errors at Failure Points

**Important:** Always use the predefined error constants from `AppGatewayTelemetryMarkers.h` instead of hardcoded strings.

```cpp
// Report API errors - use AGW_ERROR_* constants
AGW_REPORT_API_ERROR(context, "GetData", AGW_ERROR_INVALID_REQUEST);

// Report external service errors - use AGW_SERVICE_* and AGW_ERROR_* constants
AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_TIMEOUT);
```

### Step 5: Deinitialize on Shutdown

In your plugin's `Deinitialize()` method:

```cpp
void YourPlugin::Deinitialize(PluginHost::IShell* service)
{
    // Deinitialize telemetry client
    AGW_TELEMETRY_DEINIT();
    
    // ... your cleanup code ...
}
```

---

## Detailed API Reference

### Macros

#### `AGW_DEFINE_TELEMETRY_CLIENT(pluginName)`

Defines a plugin-specific telemetry client instance. This macro **MUST** be called once in each plugin's implementation file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `pluginName` | Constant | Plugin name constant from `AppGatewayTelemetryMarkers.h` (e.g., `AGW_PLUGIN_YOUR_PLUGIN`) |

**Placement:** Top of your plugin's `.cpp` file, **before** namespace declarations.

**Example:**
```cpp
#include "UtilsAppGatewayTelemetry.h"

AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_YOUR_PLUGIN)

namespace WPEFramework {
namespace Plugin {
    // ... implementation
}}
```

**Purpose:** Creates an isolated telemetry client instance for this plugin. Each plugin gets its own instance to prevent cross-plugin data contamination.

#### `AGW_TELEMETRY_INIT(service)`

Initializes the telemetry client by querying the `IAppGatewayTelemetry` interface from App Gateway.

| Parameter | Type | Description |
|-----------|------|-------------|
| `service` | `PluginHost::IShell*` | The service shell passed to `Initialize()` |

**Prerequisites:** `AGW_DEFINE_TELEMETRY_CLIENT` must be called first in the same compilation unit.

**Example:**
```cpp
AGW_TELEMETRY_INIT(service);
```

#### `AGW_TELEMETRY_DEINIT()`

Releases the telemetry client and cleans up resources.

**Example:**
```cpp
AGW_TELEMETRY_DEINIT();
```

#### `AGW_REPORT_API_ERROR(context, api, error)`

Reports an API error event to App Gateway for optional immediate forensics. The plugin name is automatically included. This is OPTIONAL - AppGateway will still track the error count and send it as a metric (`AppGwApiErrorCount_<ApiName>_split`) periodically.

| Parameter | Type | Description |
|-----------|------|-------------|  
| `context` | `const Exchange::GatewayContext&` | Gateway context (requestId, connectionId, appId) for request correlation |
- AppGateway sends periodic metric: `AppGwApiErrorCount_<ApiName>_split`

**Example:**
```cpp
AGW_REPORT_API_ERROR(context, "apiMethod1", "PERMISSION_DENIED");
```

#### `AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, service, error)`

Reports an external service error event to App Gateway for optional immediate forensics. The plugin name is automatically included. This is OPTIONAL - AppGateway will still track the error count and send it as a metric (`AppGwExtServiceErrorCount_<ServiceName>_split`) periodically.

| Parameter | Type | Description |
|-----------|------|-------------|  
| `context` | `const Exchange::GatewayContext&` | Gateway context (requestId, connectionId, appId) for request correlation |
- AppGateway sends periodic metric: `AppGwExtServiceErrorCount_<ServiceName>_split`

**Example:**
```cpp
AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_PERMISSION, "CONNECTION_REFUSED");
```

#### `AGW_RECORD_BOOTSTRAP_TIME()`

**RECOMMENDED for all plugins**

Creates an RAII-style timer that automatically measures bootstrap time from invocation until scope exit. Reports the bootstrap time to AppGateway, which aggregates all plugin bootstrap times.

**IMPORTANT:** Must be called AFTER `AGW_TELEMETRY_INIT()` as it requires the telemetry client to be initialized.

**Usage:**
```cpp
const string MyPlugin::Initialize(PluginHost::IShell* service) {
    // ... plugin initialization code ...
    
    // Initialize telemetry client FIRST
    AGW_TELEMETRY_INIT(service);
    
    // Track bootstrap time - MUST come AFTER AGW_TELEMETRY_INIT
    AGW_RECORD_BOOTSTRAP_TIME();
    
    return EMPTY_STRING;
} // Timer automatically records on scope exit
```

#### `AGW_TRACK_SERVICE_CALL(varName, context, serviceName)`

Creates an RAII-style tracker for external service calls. Automatically tracks service latency and reports it on destruction.

| Parameter | Type | Description |
|-----------|------|-------------|
| `varName` | identifier | Variable name for the tracker |
| `context` | `const Exchange::GatewayContext&` | Gateway context for request correlation |
| `serviceName` | `const char*` | Predefined service name from `AppGatewayTelemetryMarkers.h` |

**Example:**
```cpp
Core::hresult GetData() {
    AGW_TRACK_SERVICE_CALL(serviceTimer, context, AGW_SERVICE_EXTERNAL_SERVICE_1);
    auto result = externalService->FetchData();
    return result;
    // Service latency automatically recorded on scope exit
}
```

#### `AGW_TRACK_RESPONSE_PAYLOAD(context, payload)`

Tracks JSON-RPC 2.0 response payload. AppGateway parses the payload to determine success/failure and updates call statistics.

| Parameter | Type | Description |
|-----------|------|-------------|
| `context` | `const Exchange::GatewayContext&` | Gateway context for request correlation |
| `payload` | `const string&` | Raw JSON-RPC 2.0 response string |

**Example:**
```cpp
void ReturnMessageInSocket(uint32_t connectionId, const string& result, int requestId) {
    Exchange::GatewayContext context = {requestId, connectionId, appId};
    AGW_TRACK_RESPONSE_PAYLOAD(context, result);  // Parse and track
    SendToClient(result);
}
```

#### `AGW_TRACK_API_CALL(varName, context, apiName)`

** RECOMMENDED for all plugin API methods**

Creates an RAII-style tracker that automatically tracks API latency and success/error counts. On destruction, it reports comprehensive statistics including latency metrics. If an error occurs, call `SetFailed(errorCode)` to mark the API call as failed.

**This is the preferred way to track plugin method performance** as it provides:
- Automatic latency tracking (no manual timing needed)
- Success and error counts
- Separate success/error latency statistics
- Min/max/avg latency aggregation
- Success rate calculation

| Parameter | Type | Description |
|-----------|------|-------------|
| `varName` | identifier | Variable name for the tracker |
| `context` | `const Exchange::GatewayContext&` | Gateway context (requestId, connectionId, appId) for request correlation |
| `apiName` | `const char*` | Name of the API being timed |

**What it does:**
- Automatically starts timing when declared
- On destruction, automatically reports API latency
- Tracks both success and error latencies separately

**Example:**
```cpp
Core::hresult MyPlugin::GetData(const Exchange::GatewayContext& context, const string& key, string& value) {
    AGW_TRACK_API_CALL(tracker, context, "GetData");  // Automatic latency tracking
    
    auto result = FetchFromCache(key, value);
    if (result != Core::ERROR_NONE) {
        tracker.SetFailed(AGW_ERROR_NOT_FOUND);  // Mark as error
        return result;
    }
    
    return Core::ERROR_NONE;
    // tracker automatically reports success latency on destruction
}
```

#### `AGW_REPORT_METRIC(context, metricName, value, unit)`

Reports a custom numeric metric to AppGateway for aggregation (aggregated hourly).

| Parameter | Type | Description |
|-----------|------|-------------|
| `context` | `const Exchange::GatewayContext&` | Gateway context for request correlation |
| `metricName` | `const char*` | Custom metric name (use predefined marker from `AppGatewayTelemetryMarkers.h`) |
| `value` | `double` | Numeric value |
| `unit` | `const char*` | Unit from `AppGatewayTelemetryMarkers.h` (e.g., `AGW_UNIT_MEGABYTES`) |

**Example:**
```cpp
// Track plugin memory usage
size_t memoryUsageMB = GetTotalAppGwPluginMemoryUsage();
AGW_REPORT_METRIC(context, "<<Use_Already_Defined_MemoryUsage_Marker_Here>>", 
                  memoryUsageMB, AGW_UNIT_MEGABYTES);
```

#### `AGW_REPORT_EVENT(context, eventName, eventData)`

Reports a custom telemetry event to AppGateway (immediate T2 event).

| Parameter | Type | Description |
|-----------|------|-------------|
| `context` | `const Exchange::GatewayContext&` | Gateway context for request correlation |
| `eventName` | `const char*` | Event name (becomes T2 marker) |
| `eventData` | `const char*` | JSON string with event data |

**Example:**
```cpp
// Track token expiration
JsonObject tokenData;
tokenData["token_type"] = "AUTH";
tokenData["expiry_timestamp"] = expiryTime;
string eventDataStr;
tokenData.ToString(eventDataStr);
AGW_REPORT_EVENT(context, "TokenExpired", eventDataStr);
```

---

## Constants Reference

All telemetry markers, plugin names, service names, error codes, and units are defined in `AppGatewayTelemetryMarkers.h`.

**For complete reference, see:** [AppGatewayTelemetryMarkers.md](./AppGatewayTelemetryMarkers.md)

**Key Constants:**
- **Plugin Names:** `AGW_PLUGIN_*` (use with `AGW_DEFINE_TELEMETRY_CLIENT`)
- **Service Names:** `AGW_SERVICE_*` (use with `AGW_REPORT_EXTERNAL_SERVICE_ERROR`)
- **Error Codes:** `AGW_ERROR_*` (use for error reporting)
- **Units:** `AGW_UNIT_*` (use with `AGW_REPORT_METRIC`)

---

## Usage Examples

### Example 1: Simple API Error Reporting

```cpp
Core::hresult YourPluginImplementation::apiMethod1(const string& param1, ...) {
    // Create context for telemetry tracking
    Exchange::GatewayContext context{0, 0, param1};
    
    if (!_client) {
        LOGERR("External service client not initialized");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_EXTERNAL_SERVICE_1, AGW_ERROR_CLIENT_NOT_INITIALIZED);
        return Core::ERROR_UNAVAILABLE;
    }
    // ... rest of implementation
}
```

### Example 2: Automatic Latency Tracking with Scoped Timer

```cpp
std::string YourPlugin::apiMethod2(const Exchange::GatewayContext& context, const string& param1) {
    // Track API latency using scoped timer
    AGW_TRACK_API_CALL(tracker, context, "apiMethod2");
    
    string result = "default_value";

    if (!mExternalService) {
        LOGERR("External service not initialized.");
        tracker.SetFailed(AGW_ERROR_NOT_AVAILABLE);
        return result;
    }
    
    auto service = mExternalService->getService<ExternalService1>();
    if (!service) {
        LOGERR("ExternalService1 not available.");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_EXTERNAL_SERVICE_1, AGW_ERROR_NOT_AVAILABLE);
        tracker.SetFailed(AGW_ERROR_NOT_AVAILABLE);
        return result;
    }
    
    if (service->GetData(context, result) != Core::ERROR_NONE) {
        LOGERR("Failed to get data");
        AGW_REPORT_API_ERROR(context, "apiMethod2", AGW_ERROR_FETCH_FAILED);
        tracker.SetFailed(AGW_ERROR_FETCH_FAILED);
        return result;
    }
    
    return result;
    // tracker automatically reports success latency on destruction
}
```

### Example 3: Manual Service Latency Tracking

```cpp
Core::hresult YourPlugin::apiMethod3(const Exchange::GatewayContext& context, const std::string& param1, const char* requiredField) {
    // API name can be a string (method-specific) but error codes should use constants
    AGW_TRACK_API_CALL(apiTracker, context, "apiMethod3");
    
    // ... check cache first ...
    
    // Track external service call latency
    auto serviceCallStart = std::chrono::steady_clock::now();
    
    Exchange::IExternalService* externalService = GetExternalService();
    if (externalService == nullptr) {
        LOGERR("External service interface not available");
        // Use predefined service and error constants
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_EXTERNAL_SERVICE_1, AGW_ERROR_INTERFACE_UNAVAILABLE);
        apiTracker.SetFailed(AGW_ERROR_INTERFACE_UNAVAILABLE);
        return Core::ERROR_UNAVAILABLE;
    }
    
    RPC::IStringIterator* dataIterator = nullptr;
    if (externalService->GetData(param1, false, dataIterator) != Core::ERROR_NONE) {
        LOGERR("GetData failed");
        // Use predefined error constant
        AGW_REPORT_API_ERROR(context, "GetData", AGW_ERROR_PERMISSION_DENIED);
        apiTracker.SetFailed(AGW_ERROR_PERMISSION_DENIED);
        return Core::ERROR_PRIVILIGED_REQUEST;
    }
    
    // Track external service call latency
    auto serviceCallEnd = std::chrono::steady_clock::now();
    auto serviceLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        serviceCallEnd - serviceCallStart).count();
    AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_EXTERNAL_SERVICE_1, static_cast<double>(serviceLatencyMs));
    
    // ... process data ...
    
    return Core::ERROR_NONE;
}
```

### Example 4: Combined Event and Metric Reporting

```cpp
Core::hresult YourPluginImplementation::GetAuthToken(const string& appId, string& token) {
    // Create context for telemetry tracking
    Exchange::GatewayContext context{0, 0, appId};
    
    // Check cache first
    const std::string cacheKey = std::string("platform:") + appId;
    if (_tokenCache.Get(cacheKey, token)) {
        return Core::ERROR_NONE;  // Cache hit - fast path
    }
    
    // Track token service call latency
    auto tokenServiceStart = std::chrono::steady_clock::now();
    
    // Fetch authentication tokens
    std::string authToken1, authToken2;
    uint64_t token1Expiry = 0, token2Expiry = 0;
    
    if (!FetchAuthToken1(authToken1, token1Expiry)) {
        LOGERR("FetchAuthToken1 failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_AUTH, AGW_ERROR_AUTH_TOKEN_FETCH_FAILED);
        return Core::ERROR_UNAVAILABLE;
    }
    
    if (!FetchAuthToken2(appId, authToken2, token2Expiry)) {
        LOGERR("FetchAuthToken2 failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_AUTH, AGW_ERROR_AUTH_TOKEN_FETCH_FAILED);
        return Core::ERROR_UNAVAILABLE;
    }
    
    // Get platform token
    std::string err;
    uint32_t expiresInSec = 0;
    const bool ok = _tokenService->GetPlatformToken(appId, authToken2, authToken1, token, expiresInSec, err);
    if (!ok) {
        LOGERR("GetPlatformToken failed: %s", err.c_str());
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_EXTERNAL_SERVICE_2, err.c_str());
        return Core::ERROR_UNAVAILABLE;
    }
    
    // Report service latency metric
    auto tokenServiceEnd = std::chrono::steady_clock::now();
    auto tokenLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        tokenServiceEnd - tokenServiceStart).count();
    AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_OTT_TOKEN, static_cast<double>(tokenLatencyMs));
    
    // Cache the token
    // ...
    
    return Core::ERROR_NONE;
}
```

### Example 5: Custom Metric Reporting

```cpp
// In YourPlugin.cpp: Track failure count
static uint32_t failureCount = 0;
// ... inside failure handling ...
++failureCount;
AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, failureCount, AGW_UNIT_COUNT);

// In AnotherPlugin.cpp: Track service failure count
static uint32_t serviceFailureCount = 0;
// ... inside failure handling ...
++serviceFailureCount;
AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, serviceFailureCount, AGW_UNIT_COUNT);
```

---

## Best Practices

### 1. Report at the Point of Failure

Place telemetry calls immediately after detecting a failure:

```cpp
// ✓ Good - report at failure point using predefined constants
if (!service->Connect()) {
    LOGERR("Connection failed");
    AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_REFUSED);
    return Core::ERROR_UNAVAILABLE;
}

// ✗ Bad - reporting too late or in wrong location
```

### 2. Always Use Predefined Constants

**Always use predefined constants** for services and error codes. Only use string literals if no appropriate constant exists.

```cpp
// ✓ BEST - uses predefined constants for both service and error
AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_TIMEOUT);

// ✓ Acceptable - specific error when predefined constant doesn't exist
AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_AUTH, "INVALID_TOKEN_FORMAT");

// ✗ BAD - hardcoded strings when constants exist
AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, "AuthService", "CONNECTION_TIMEOUT");
```

**Available Error Constants:** See `AppGatewayTelemetryMarkers.h` for:
- `AGW_ERROR_CONNECTION_TIMEOUT`
- `AGW_ERROR_CONNECTION_REFUSED`
- `AGW_ERROR_INTERFACE_UNAVAILABLE`
- `AGW_ERROR_TIMEOUT`
- `AGW_ERROR_PERMISSION_DENIED`
- `AGW_ERROR_INVALID_REQUEST`
- `AGW_ERROR_FETCH_FAILED`
- And more...

### 3. Don't Over-Report

Report significant errors, not every minor issue:

```cpp
// ✓ Good - report service unavailability using constants
if (!interface) {
    AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE);
}

// ✗ Bad - don't report expected/handled conditions
if (cache.empty()) {
    // This is normal, don't report as error
    RefreshCache();
}
```

### 4. Use Predefined Constants and Simplified Macros

Use the simplified macros with predefined constants:

```cpp
// ✓ BEST - uses predefined error constant
AGW_REPORT_API_ERROR(context, "GetData", AGW_ERROR_GENERAL);

// ✓ BEST - uses both service and error constants
AGW_REPORT_EXTERNAL_SERVICE_ERROR(context, AGW_SERVICE_PERMISSION, AGW_ERROR_CONNECTION_REFUSED);

// ✗ BAD - hardcoded error string when constant exists
AGW_REPORT_API_ERROR(context, "GetData", "FAILED");
```

### 4. Track Bootstrap Time

Always use `AGW_RECORD_BOOTSTRAP_TIME()` **immediately after** `AGW_TELEMETRY_INIT()` in your Initialize() method to track plugin startup performance.

---

## Adding New Plugins

To add a new plugin:

1. **Define plugin constant** in `AppGatewayTelemetryMarkers.h`:
   ```cpp
   #define AGW_PLUGIN_YOURPLUGIN  "YourPlugin"
   ```

2. **Use the constant** in your plugin implementation:
   ```cpp
   AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_YOURPLUGIN)
   ```

3. **Add service constants** (if using new external services):
   ```cpp
   #define AGW_SERVICE_YOUR_SERVICE  "YourServiceName"
   ```

**Note:** Markers are auto-generated by the macros - no need to manually define individual markers.

For details on marker naming conventions, see [AppGatewayTelemetryMarkers.md](./AppGatewayTelemetryMarkers.md).

#### Understanding the Telemetry Types

The system tracks three types of statistics:

| Telemetry Type | Source | T2 Marker | Purpose | Tracks |
|----------------|--------|-----------|---------|---------|
| **API Method Stats** | `AGW_TRACK_API_CALL` | `ENTS_INFO_AppGwApiMethod` | Per-API method tracking | Success/error counts, success/error latency (min/max/avg) |
| **API Latency Stats** | `AGW_REPORT_API_LATENCY` | `ENTS_INFO_AppGwApiLatency` | Generic API latency | Latency only (min/max/avg) |
| **Service Latency Stats** | `AGW_REPORT_SERVICE_LATENCY` | `ENTS_INFO_AppGwServiceLatency` | External service latency | Latency only (min/max/avg) |

#### Rules for Latency Tracking

1. **For your plugin's own API methods**: Use `AGW_TRACK_API_CALL` **only**
   - Automatically tracks both success and error cases with latency
   - Reports comprehensive statistics including success rate
   - No need to manually report latency

2. **For external service calls**: Use `AGW_REPORT_SERVICE_LATENCY`
   - Tracks latency of calls to external services (gRPC, REST APIs, etc.)
   - Separate from your plugin's API methods

3. **Never use both for the same operation**

#### Examples

```cpp
// ✓ CORRECT - Use AGW_TRACK_API_CALL for plugin's own methods
Core::hresult MyPlugin::GetData(const Exchange::GatewayContext& context, const string& key, string& value) {
    AGW_TRACK_API_CALL(tracker, context, "GetData");  // ← Tracks latency automatically
    
    auto result = FetchFromDatabase(key, value);
    if (result != Core::ERROR_NONE) {
        tracker.SetFailed(AGW_ERROR_FETCH_FAILED);
        return result;
    }
    
    return Core::ERROR_NONE;
    // ✓ tracker reports success latency automatically
}

// ✗ WRONG - Do NOT add manual latency reporting
Core::hresult MyPlugin::GetData(const Exchange::GatewayContext& context, const string& key, string& value) {
    AGW_TRACK_API_CALL(tracker, context, "GetData");  // ← Already tracks latency
    
    auto start = std::chrono::steady_clock::now();
    auto result = FetchFromDatabase(key, value);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    
    AGW_REPORT_API_LATENCY(context, "GetData", duration);  // ✗ DUPLICATE! Scoped timer already reports this
    
    return result;
}

// ✓ CORRECT - Use AGW_REPORT_SERVICE_LATENCY for external service calls
Core::hresult MyPlugin::CallExternalService(const Exchange::GatewayContext& context) {
    auto start = std::chrono::steady_clock::now();
    
    auto result = mGrpcClient->MakeRequest();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    
    AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_PERMISSION, duration);  // ✓ Correct - external service
    
    return result;
}

// ✓ CORRECT - Nested scenario: Plugin method calls external service
Core::hresult MyPlugin::GetUserPermissions(const Exchange::GatewayContext& context, const string& userId) {
    AGW_TRACK_API_CALL(apiTracker, context, "GetUserPermissions");  // ← Tracks plugin's API method
    
    // Call external service and track its latency separately
    auto serviceStart = std::chrono::steady_clock::now();
    auto result = mPermissionClient->CheckPermissions(userId);
    auto serviceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - serviceStart).count();
    
    AGW_REPORT_SERVICE_LATENCY(context, AGW_SERVICE_PERMISSION, serviceDuration);  // ✓ External service timing
    
    if (result != Core::ERROR_NONE) {
        apiTracker.SetFailed(AGW_ERROR_PERMISSION_DENIED);
        return result;
    }
    
    return Core::ERROR_NONE;
    // ✓ apiTracker tracks total GetUserPermissions latency (including service call)
    // ✓ Service latency tracked separately for service-specific metrics
}
```

#### Summary

- **Plugin API methods** → `AGW_TRACK_API_CALL` only (automatic latency + success/error tracking)
- **External service calls** → `AGW_REPORT_SERVICE_LATENCY` (manual timing for specific service visibility)
- **Never report the same latency twice through different mechanisms**

---

## Troubleshooting

### Telemetry Not Being Reported

1. **Check if App Gateway is running**: The `IAppGatewayTelemetry` interface is only available when App Gateway plugin is active.

2. **Verify initialization**: Ensure `AGW_TELEMETRY_INIT(service)` is called in `Initialize()`.

3. **Check logs**: Look for initialization warnings:
   ```
   [WARN] AppGatewayTelemetry: Failed to acquire IAppGatewayTelemetry interface
   ```

### Interface Query Fails

If telemetry initialization fails, the macros will safely do nothing. Check:

1. App Gateway plugin is loaded before your plugin
2. Plugin activation order in configuration
3. COM-RPC connectivity between plugins

### Markers Not Recognized in T2

1. Ensure markers end with `_split` suffix
2. Verify marker is defined in `AppGatewayTelemetryMarkers.h`
3. Check T2 configuration includes the new marker

---

## Support

For questions or issues with telemetry integration:

1. Review existing plugin implementations for reference
2. Check `AppGatewayTelemetryMarkers.h` for all available markers
3. Contact the App Gateway team for new marker requests

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-01-31 | Initial release |
| 1.1 | 2026-02-18 | Updated all macros to require `context` parameter. Removed product-specific references. |
| 1.2 | 2026-03-02 | Simplified guide. Added AGW_RECORD_BOOTSTRAP_TIME, AGW_TRACK_SERVICE_CALL, AGW_TRACK_RESPONSE_PAYLOAD, AGW_REPORT_METRIC, AGW_REPORT_EVENT. Removed deprecated content and duplication with Markers.md. Corrected AGW_RECORD_BOOTSTRAP_TIME order (must come after AGW_TELEMETRY_INIT). |
