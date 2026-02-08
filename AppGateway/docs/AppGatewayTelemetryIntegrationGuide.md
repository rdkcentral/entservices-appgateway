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
│  │    Badger    │   │ OttServices  │   │  YourPlugin  │                     │
│  │    Plugin    │   │    Plugin    │   │   (New)      │                     │
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
│          │  • AppGwTotalCalls_split                │                        │
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

### Step 2: Initialize Telemetry in Your Plugin

In your plugin's `Initialize()` method:

```cpp
uint32_t YourPlugin::Initialize(PluginHost::IShell* service)
{
    // ... your initialization code ...
    
    // Initialize telemetry client (queries IAppGatewayTelemetry interface)
    AGW_TELEMETRY_INIT(service);
    
    return Core::ERROR_NONE;
}
```

### Step 3: Report Errors at Failure Points

**Important:** Always use the predefined error constants from `AppGatewayTelemetryMarkers.h` instead of hardcoded strings.

```cpp
// Report API errors - use AGW_ERROR_* constants
AGW_REPORT_API_ERROR("GetData", AGW_ERROR_INVALID_REQUEST);

// Report external service errors - use AGW_SERVICE_* and AGW_ERROR_* constants
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_TIMEOUT);
```

### Step 4: Deinitialize on Shutdown

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

#### `AGW_TELEMETRY_INIT(service)`

Initializes the telemetry client by querying the `IAppGatewayTelemetry` interface from App Gateway.

| Parameter | Type | Description |
|-----------|------|-------------|
| `service` | `PluginHost::IShell*` | The service shell passed to `Initialize()` |

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

#### `AGW_REPORT_API_ERROR(api, error)`

Reports an API error event to App Gateway for optional immediate forensics. The plugin name is automatically included. This is OPTIONAL - AppGateway will still track the error count and send it as a metric (`AppGwApiErrorCount_<ApiName>_split`) periodically.

If enabled, the event uses the generic marker `AppGwPluginApiError_split` internally.

| Parameter | Type | Description |
|-----------|------|-------------|  
| `api` | `const char*` | Name of the API that failed |
| `error` | `const char*` | Error code or description |

**What it does:**
- Optional: Sends immediate event with full context to T2 (if enabled)
- Always: Increments error counter in AppGatewayTelemetry
- AppGateway sends periodic metric: `AppGwApiErrorCount_<ApiName>_split`

**Example:**
```cpp
AGW_REPORT_API_ERROR("AuthorizeDataField", "PERMISSION_DENIED");
```

#### `AGW_REPORT_EXTERNAL_SERVICE_ERROR(service, error)`

Reports an external service error event to App Gateway for optional immediate forensics. The plugin name is automatically included. This is OPTIONAL - AppGateway will still track the error count and send it as a metric (`AppGwExtServiceErrorCount_<ServiceName>_split`) periodically.

If enabled, the event uses the generic marker `AppGwPluginExtServiceError_split` internally.

| Parameter | Type | Description |
|-----------|------|-------------|  
| `service` | `const char*` | Name of the external service (use `AGW_SERVICE_*` constants) |
| `error` | `const char*` | Error code or description |

**What it does:**
- Optional: Sends immediate event with full context to T2 (if enabled)
- Always: Increments error counter in AppGatewayTelemetry
- AppGateway sends periodic metric: `AppGwExtServiceErrorCount_<ServiceName>_split`

**Example:**
```cpp
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, "CONNECTION_REFUSED");
```

#### `AGW_REPORT_API_LATENCY(apiName, latencyMs)`

Reports API call latency metric to App Gateway. Sends a metric with composite name: `AppGw<PluginName>_<ApiName>_Latency_split`. The plugin name is automatically included. AppGateway aggregates these metrics over the reporting period and sends statistical data to T2.

| Parameter | Type | Description |
|-----------|------|-------------|
| `apiName` | `const char*` | Name of the API |
| `latencyMs` | `double` | Latency in milliseconds |

**What it does:**
- Sends metric to AppGatewayTelemetry with composite name
- AppGateway aggregates: sum, count, min, max
- Periodic metric: `AppGw<Plugin>_<Api>_Latency_split` (e.g., `AppGwBadger_GetDeviceSessionId_Latency_split`)

**Example:**
```cpp
AGW_REPORT_API_LATENCY("GetAppPermissions", 150.5);
```

#### `AGW_REPORT_SERVICE_LATENCY(serviceName, latencyMs)`

Reports external service call latency metric to App Gateway. Sends a metric with composite name: `AppGw<PluginName>_<ServiceName>_Latency_split`. AppGateway aggregates these metrics and sends statistical data to T2.

| Parameter | Type | Description |
|-----------|------|-------------|
| `serviceName` | `const char*` | Predefined service name from `AppGatewayTelemetryMarkers.h` |
| `latencyMs` | `double` | Latency in milliseconds |

**What it does:**
- Sends metric to AppGatewayTelemetry with composite name  
- AppGateway aggregates: sum, count, min, max
- Periodic metric: `AppGw<Plugin>_<Service>_Latency_split` (e.g., `AppGwOttServices_ThorPermissionService_Latency_split`)

**Example:**
```cpp
AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_TOKEN, 200.0);
```

#### `AGW_SCOPED_API_TIMER(varName, apiName)`

Creates an RAII-style timer that automatically tracks API latency. On destruction, it reports the elapsed time. If an error occurs, call `SetFailed(errorCode)` to mark the API call as failed.

| Parameter | Type | Description |
|-----------|------|-------------|
| `varName` | identifier | Variable name for the timer |
| `apiName` | `const char*` | Name of the API being timed |

**Example:**
```cpp
Core::hresult MyPlugin::GetData(const string& key, string& value) {
    AGW_SCOPED_API_TIMER(timer, "GetData");
    
    auto result = FetchFromCache(key, value);
    if (result != Core::ERROR_NONE) {
        timer.SetFailed(AGW_ERROR_NOT_FOUND);
        return result;
    }
    
    return Core::ERROR_NONE;
    // Timer automatically reports success latency on destruction
}
```

---

## Generic Markers

All markers are defined in `AppGatewayTelemetryMarkers.h`. The system uses **generic category-based markers** where the plugin name is included in the payload data rather than the marker name.

### Category Markers

| Marker | Description |
|--------|-------------|
| `AGW_MARKER_PLUGIN_API_ERROR` | API failures from any plugin (plugin name in payload) |
| `AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR` | External service failures from any plugin |
| `AGW_MARKER_PLUGIN_API_LATENCY` | API call latency metrics from any plugin |

### Plugin Name Constants

Use these constants when defining the telemetry client:

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_PLUGIN_APPGATEWAY` | `"AppGateway"` | App Gateway main plugin |
| `AGW_PLUGIN_BADGER` | `"Badger"` | Badger plugin |
| `AGW_PLUGIN_OTTSERVICES` | `"OttServices"` | OttServices plugin |

### Service Name Constants

Use these constants when reporting external service errors:

| Constant | Value | Description |
|----------|-------|-------------|
| `AGW_SERVICE_THOR_PERMISSION` | `"ThorPermissionService"` | Thor Permission gRPC service |
| `AGW_SERVICE_OTT_TOKEN` | `"OttTokenService"` | OTT Token gRPC service |
| `AGW_SERVICE_AUTH` | `"AuthService"` | Auth Service (COM-RPC) |
| `AGW_SERVICE_AUTH_METADATA` | `"AuthMetadataService"` | Auth metadata collection |
| `AGW_SERVICE_OTT_SERVICES` | `"OttServices"` | OttServices interface |
| `AGW_SERVICE_LAUNCH_DELEGATE` | `"LaunchDelegate"` | Launch Delegate interface |
| `AGW_SERVICE_LIFECYCLE_DELEGATE` | `"LifecycleDelegate"` | Lifecycle Delegate interface |
| `AGW_SERVICE_PERMISSION` | `"PermissionService"` | Internal permission service |
| `AGW_SERVICE_AUTHENTICATION` | `"AuthenticationService"` | WebSocket authentication |

### Predefined Error Codes

**IMPORTANT:** Always use these predefined error constants instead of hardcoded strings. This ensures consistency across all telemetry reporting and enables proper aggregation in analytics.

| Constant | Value | When to Use |
|----------|-------|-------------|
| `AGW_ERROR_INTERFACE_UNAVAILABLE` | `"INTERFACE_UNAVAILABLE"` | COM-RPC interface is not available |
| `AGW_ERROR_INTERFACE_NOT_FOUND` | `"INTERFACE_NOT_FOUND"` | COM-RPC interface could not be found |
| `AGW_ERROR_CLIENT_NOT_INITIALIZED` | `"CLIENT_NOT_INITIALIZED"` | Service client not properly initialized |
| `AGW_ERROR_CONNECTION_REFUSED` | `"CONNECTION_REFUSED"` | Connection to service was refused |
| `AGW_ERROR_CONNECTION_TIMEOUT` | `"CONNECTION_TIMEOUT"` | Connection to service timed out |
| `AGW_ERROR_TIMEOUT` | `"TIMEOUT"` | Operation timed out |
| `AGW_ERROR_PERMISSION_DENIED` | `"PERMISSION_DENIED"` | Permission check failed |
| `AGW_ERROR_INVALID_REQUEST` | `"INVALID_REQUEST"` | Invalid parameters or request format |
| `AGW_ERROR_INVALID_RESPONSE` | `"INVALID_RESPONSE"` | Service returned invalid/unexpected response |
| `AGW_ERROR_FETCH_FAILED` | `"FETCH_FAILED"` | Failed to fetch/retrieve data |
| `AGW_ERROR_UPDATE_FAILED` | `"UPDATE_FAILED"` | Failed to update/store data |
| `AGW_ERROR_NOT_AVAILABLE` | `"NOT_AVAILABLE"` | Service or resource not available |
| `AGW_ERROR_GENERAL` | `"GENERAL_ERROR"` | General/unspecified error (use only when no specific constant applies) |

**Note:** API names (e.g., "GetData", "SetSetting") can be string literals as they identify specific method names. Only error codes and service names should use predefined constants.

### Metric Units

| Constant | Value |
|----------|-------|
| `AGW_UNIT_MILLISECONDS` | `"ms"` |
| `AGW_UNIT_SECONDS` | `"sec"` |
| `AGW_UNIT_COUNT` | `"count"` |
| `AGW_UNIT_BYTES` | `"bytes"` |
| `AGW_UNIT_PERCENT` | `"percent"` |

---

## Usage Examples

### Example 1: Simple API Error Reporting

```cpp
Core::hresult OttServicesImplementation::GetAppPermissions(const string& appId, ...) {
    if (!_perms) {
        LOGERR("PermissionsClient not initialized");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_CLIENT_NOT_INITIALIZED);
        return Core::ERROR_UNAVAILABLE;
    }
    // ... rest of implementation
}
```

### Example 2: Automatic Latency Tracking with Scoped Timer

```cpp
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
```

### Example 3: Manual Service Latency Tracking

```cpp
Core::hresult Badger::AuthorizeDataField(const std::string& appId, const char* requiredDataField) {
    // API name can be a string (method-specific) but error codes should use constants
    AGW_SCOPED_API_TIMER(apiTimer, "AuthorizeDataField");
    
    // ... check cache first ...
    
    // Track external service call latency
    auto serviceCallStart = std::chrono::steady_clock::now();
    
    Exchange::IOttServices* ottServices = GetOttServices();
    if (ottServices == nullptr) {
        LOGERR("OttServices interface not available");
        // Use predefined service and error constants
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE);
        apiTimer.SetFailed(AGW_ERROR_INTERFACE_UNAVAILABLE);
        return Core::ERROR_UNAVAILABLE;
    }
    
    RPC::IStringIterator* permissionsIterator = nullptr;
    if (ottServices->GetAppPermissions(appId, false, permissionsIterator) != Core::ERROR_NONE) {
        LOGERR("GetAppPermissions failed");
        // Use predefined error constant
        AGW_REPORT_API_ERROR("GetAppPermissions", AGW_ERROR_PERMISSION_DENIED);
        apiTimer.SetFailed(AGW_ERROR_PERMISSION_DENIED);
        return Core::ERROR_PRIVILIGED_REQUEST;
    }
    
    // Track external service call latency
    auto serviceCallEnd = std::chrono::steady_clock::now();
    auto serviceLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        serviceCallEnd - serviceCallStart).count();
    AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_SERVICES, static_cast<double>(serviceLatencyMs));
    
    // ... process permissions ...
    
    return Core::ERROR_NONE;
}
```

### Example 4: Combined Event and Metric Reporting

```cpp
Core::hresult OttServicesImplementation::GetAppCIMAToken(const string& appId, string& token) {
    // Check cache first
    const std::string cacheKey = std::string("platform:") + appId;
    if (_tokenCache.Get(cacheKey, token)) {
        return Core::ERROR_NONE;  // Cache hit - fast path
    }
    
    // Track token service call latency
    auto tokenServiceStart = std::chrono::steady_clock::now();
    
    // Fetch SAT and xACT
    std::string sat, xact;
    uint64_t satExpiry = 0, xactExpiry = 0;
    
    if (!FetchSat(sat, satExpiry)) {
        LOGERR("FetchSat failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, AGW_ERROR_FETCH_FAILED);
        return Core::ERROR_UNAVAILABLE;
    }
    
    if (!FetchXact(appId, xact, xactExpiry)) {
        LOGERR("FetchXact failed");
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, AGW_ERROR_FETCH_FAILED);
        return Core::ERROR_UNAVAILABLE;
    }
    
    // Get platform token
    std::string err;
    uint32_t expiresInSec = 0;
    const bool ok = _token->GetPlatformToken(appId, xact, sat, token, expiresInSec, err);
    if (!ok) {
        LOGERR("GetPlatformToken failed: %s", err.c_str());
        AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_TOKEN, err.c_str());
        return Core::ERROR_UNAVAILABLE;
    }
    
    // Report service latency metric
    auto tokenServiceEnd = std::chrono::steady_clock::now();
    auto tokenLatencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        tokenServiceEnd - tokenServiceStart).count();
    AGW_REPORT_SERVICE_LATENCY(AGW_SERVICE_OTT_TOKEN, static_cast<double>(tokenLatencyMs));
    
    // Cache the token
    // ...
    
    return Core::ERROR_NONE;
}
```

### Example 5: Custom Metric Reporting (Badger, OttServices)

```cpp
// In Badger.cpp: Track PermissionGroup failure count
static uint32_t permissionGroupFailureCount = 0;
// ... inside failure handling ...
++permissionGroupFailureCount;
AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, permissionGroupFailureCount, AGW_UNIT_COUNT);

// In OttServicesImplementation.cpp: Track gRPC failure count
static uint32_t grpcFailureCount = 0;
// ... inside failure handling ...
++grpcFailureCount;
AGW_REPORT_METRIC(AGW_MARKER_PLUGIN_METRIC, grpcFailureCount, AGW_UNIT_COUNT);
```

---

## Adding Markers for a New Plugin

If you're integrating a new plugin, you need to add your plugin's markers to `AppGatewayTelemetryMarkers.h`.

### Step 1: Add Plugin Section

Add a new section in `AppGatewayTelemetryMarkers.h`:

```cpp
//=============================================================================
// YOURPLUGIN MARKERS
// Used by YourPlugin when reporting telemetry to AppGateway
//=============================================================================

// Note: Use the GENERIC category-based markers instead of plugin-specific ones:
// - AGW_MARKER_PLUGIN_API_ERROR ("AppGwPluginApiError_split")
// - AGW_MARKER_PLUGIN_EXT_SERVICE_ERROR ("AppGwPluginExtServiceError_split")
// Plugin name is included in the event payload, not in the marker name.

// For metrics, follow the naming pattern AppGw<Plugin>_<Api/Service>_<Type>_split:
// Example latency metrics (auto-generated by macros):
// - AppGwYourPlugin_GetSettings_Latency_split
// - AppGwYourPlugin_YourService_Latency_split

// Only add custom metric constants if needed:
/**
 * @brief YourPlugin custom metric example
 * @details Example custom metric (use AGW_REPORT_METRIC macro with this constant)
 * @unit count/percent/custom
 */
#define AGW_METRIC_YOURPLUGIN_CACHE_HIT_RATE        "AppGwYourPluginCacheHitRate_split"
```

### Step 2: Add Service Names (if new services)

If your plugin interacts with new external services, add them:

```cpp
/**
 * @brief Your New Service
 * @details Description of the service
 */
#define AGW_SERVICE_YOUR_SERVICE                    "YourServiceName"
```

### Naming Convention

All markers must follow this pattern:

```
AppGw<PluginName>_<Api/Service>_<Type>_split
```

| Component | Description | Examples |
|-----------|-------------|----------|
| `AppGw` | App Gateway prefix (camelCase, mandatory) | - |
| `<PluginName>` | Name of your plugin | `Badger`, `OttServices`, `YourPlugin` |
| `<Api/Service>` | API method or service name | `GetSettings`, `ThorPermissionService` |
| `<Type>` | Type of metric (for latency/metrics only) | `Latency` |
| `_split` | T2 suffix for structured format (mandatory) | - |

**Event Markers (Generic):**
- `AppGwPluginApiError_split` - Used by ALL plugins for API errors
- `AppGwPluginExtServiceError_split` - Used by ALL plugins for service errors

**Metric Markers (Plugin-specific, auto-generated by macros):**
- `AppGw<Plugin>_<Api>_Latency_split` - API latency (e.g., `AppGwBadger_GetSettings_Latency_split`)
- `AppGw<Plugin>_<Service>_Latency_split` - Service latency (e.g., `AppGwOttServices_ThorPermissionService_Latency_split`)
- `AppGw<MetricName>_split` - Custom metrics (e.g., `AppGwBootstrapDuration_split`)

---

## Complete Integration Example

Here's a complete example for a hypothetical `FbSettings` plugin:

### 1. Add Plugin Constant to `AppGatewayTelemetryMarkers.h`

```cpp
//=============================================================================
// FBSETTINGS PLUGIN
//=============================================================================

/**
 * @brief FbSettings plugin identifier
 */
#define AGW_PLUGIN_FBSETTINGS                       "FbSettings"

// Note: No need to define individual markers!
// Macros will auto-generate:
// - AppGwPluginApiError_split (generic marker for all API errors)
// - AppGwPluginExtServiceError_split (generic marker for all service errors)
// - AppGwFbSettings_<ApiName>_Latency_split (auto-composed latency metrics)
```

### 2. Integrate in Plugin Implementation

```cpp
// FbSettingsImplementation.cpp

#include "FbSettingsImplementation.h"
#include "UtilsAppGatewayTelemetry.h"
#include "AppGatewayTelemetryMarkers.h"

namespace WPEFramework {
namespace Plugin {

    // Define telemetry client (typically in class header or at top of implementation)
    AGW_DEFINE_TELEMETRY_CLIENT(AGW_PLUGIN_FBSETTINGS)

    uint32_t FbSettingsImplementation::Initialize(PluginHost::IShell* service)
    {
        _service = service;
        
        // Initialize telemetry
        AGW_TELEMETRY_INIT(service);
        
        // Initialize settings backend
        if (!InitializeBackend()) {
            // Use predefined error constant when available
            AGW_REPORT_EXTERNAL_SERVICE_ERROR("SettingsBackend", AGW_ERROR_GENERAL);
            // Auto-reports to: AppGwPluginExtServiceError_split
            // Payload: {"plugin":"FbSettings","service":"SettingsBackend","error":"GENERAL_ERROR"}
            return Core::ERROR_UNAVAILABLE;
        }
        
        return Core::ERROR_NONE;
    }

    void FbSettingsImplementation::Deinitialize(PluginHost::IShell* service)
    {
        AGW_TELEMETRY_DEINIT();
        // ... cleanup ...
    }

    uint32_t FbSettingsImplementation::GetSetting(const string& key, string& value)
    {
        // Auto-track latency with scoped timer
        AGW_SCOPED_API_TIMER(timer, "GetSetting");
        // Auto-reports to: AppGwFbSettings_GetSetting_Latency_split when function exits
        
        if (key.empty()) {
            // Always use predefined error constants
            AGW_REPORT_API_ERROR("GetSetting", AGW_ERROR_INVALID_REQUEST);
            // Auto-reports to: AppGwPluginApiError_split
            // Auto-increments: AppGwApiErrorCount_GetSetting_split
            return Core::ERROR_BAD_REQUEST;
        }

        if (!_backend->Fetch(key, value)) {
            // Use predefined error constant
            AGW_REPORT_EXTERNAL_SERVICE_ERROR("SettingsBackend", AGW_ERROR_FETCH_FAILED);
            // Auto-increments: AppGwExtServiceErrorCount_SettingsBackend_split
            return Core::ERROR_UNAVAILABLE;
        }

        return Core::ERROR_NONE;
    }

    uint32_t FbSettingsImplementation::SetSetting(const string& key, const string& value)
    {
        AGW_SCOPED_API_TIMER(timer, "SetSetting");
        
        if (key.empty()) {
            AGW_REPORT_API_ERROR("SetSetting", AGW_ERROR_INVALID_REQUEST);
            return Core::ERROR_BAD_REQUEST;
        }

        auto start = std::chrono::steady_clock::now();
        
        if (!_backend->Store(key, value)) {
            AGW_REPORT_EXTERNAL_SERVICE_ERROR("SettingsBackend", AGW_ERROR_UPDATE_FAILED);
            return Core::ERROR_UNAVAILABLE;
        }

        auto end = std::chrono::steady_clock::now();
        double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
        AGW_REPORT_SERVICE_LATENCY("SettingsBackend", latencyMs);
        // Auto-reports to: AppGwFbSettings_SettingsBackend_Latency_split

        return Core::ERROR_NONE;
    }

} // namespace Plugin
} // namespace WPEFramework
```

---

## Best Practices

### 1. Report at the Point of Failure

Place telemetry calls immediately after detecting a failure:

```cpp
// ✓ Good - report at failure point using predefined constants
if (!service->Connect()) {
    LOGERR("Connection failed");
    AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_REFUSED);
    return Core::ERROR_UNAVAILABLE;
}

// ✗ Bad - reporting too late or in wrong location
```

### 2. Always Use Predefined Constants

**Always use predefined constants** for services and error codes. Only use string literals if no appropriate constant exists.

```cpp
// ✓ BEST - uses predefined constants for both service and error
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, AGW_ERROR_CONNECTION_TIMEOUT);

// ✓ Acceptable - specific error when predefined constant doesn't exist
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_AUTH, "INVALID_TOKEN_FORMAT");

// ✗ BAD - hardcoded strings when constants exist
AGW_REPORT_EXTERNAL_SERVICE_ERROR("AuthService", "CONNECTION_TIMEOUT");
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
    AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_OTT_SERVICES, AGW_ERROR_INTERFACE_UNAVAILABLE);
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
AGW_REPORT_API_ERROR("GetData", AGW_ERROR_GENERAL);

// ✓ BEST - uses both service and error constants
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_CONNECTION_REFUSED);

// ✗ BAD - hardcoded error string when constant exists
AGW_REPORT_API_ERROR("GetData", "FAILED");
```

### 5. Include Context in Logging

The telemetry macros don't replace logging—use both:

```cpp
// ✓ Good - log with context, then report telemetry using constants
LOGERR("GetAppPermissions failed for appId='%s': %s", appId.c_str(), error.c_str());
AGW_REPORT_EXTERNAL_SERVICE_ERROR(AGW_SERVICE_THOR_PERMISSION, AGW_ERROR_PERMISSION_DENIED);
```

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

1. Review existing implementations in `Badger` and `OttServices` plugins
2. Check `AppGatewayTelemetryMarkers.h` for all available markers
3. Contact the App Gateway team for new marker requests

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-01-31 | Initial release |
