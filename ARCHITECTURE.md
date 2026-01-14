# App Gateway Architecture

This document describes the architecture and design of the entservices-appgateway implementation.

## Overview

The App Gateway is a bridge between Firebolt applications and Thunder framework services. It provides a unified, standardized interface for applications while delegating the actual implementation to various Thunder plugins.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Firebolt Application                     │
└────────────────────┬────────────────────────────────────────┘
                     │ Firebolt API Calls
                     ↓
┌─────────────────────────────────────────────────────────────┐
│                      AppGateway Plugin                      │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              Request Resolution Layer                  │ │
│  │  • Parse Firebolt method calls                        │ │
│  │  • Resolve to Thunder plugin endpoints                │ │
│  │  • Apply permission checks                            │ │
│  │  • Transform request/response formats                 │ │
│  └───────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
        ↓            ↓            ↓
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ AppGateway   │ │ AppNotifi-   │ │   Thunder    │
│   Common     │ │  cations     │ │   System     │
└──────────────┘ └──────────────┘ └──────────────┘
        │            │            │
        └────────────┼────────────┘
                     ↓
        ┌─────────────────────────┐
        │  Platform/Device APIs   │
        └─────────────────────────┘
```

## Core Components

### 1. AppGateway Plugin

**Purpose:** Routes Firebolt API calls to appropriate Thunder plugin implementations.

**Key Classes:**
- `AppGateway`: Main plugin class implementing `IPlugin` and `JSONRPC` interfaces
- `AppGatewayImplementation`: Core resolution logic, implements `IAppGatewayResolver`
- `AppGatewayResponderImplementation`: Handles asynchronous responses, implements `IAppGatewayResponder`
- `Resolver`: Configuration-based method resolution engine

**Resolution Flow:**
1. Receive Firebolt API call (e.g., `device.name()`)
2. Look up method in resolution configuration
3. Extract target plugin alias (e.g., `org.rdk.AppGatewayCommon`)
4. Determine communication method (JSON-RPC or COM-RPC)
5. Check permission group requirements
6. Forward request to target plugin
7. Transform and return response

**Configuration-Driven Design:**
The resolver uses a JSON configuration file (`resolution.base.json`) to map Firebolt methods to Thunder plugins. This allows:
- Dynamic method routing without code changes
- Platform-specific customizations via overlay configs
- Permission enforcement at the gateway level
- Multiple communication protocol support

**Resolution Configuration Schema:**
```json
{
    "resolutions": {
        "<firebolt.method>": {
            "alias": "<thunder.plugin.callsign>",
            "useComRpc": true|false,
            "permissionGroup": "<permission.identifier>",
            "event": "<event.name>",
            "includeContext": true|false,
            "additionalContext": { ... }
        }
    }
}
```

### 2. AppGatewayCommon Plugin

**Purpose:** Implements common device information and user settings APIs.

**Key Classes:**
- `AppGatewayCommon`: Main plugin implementing `IAppGatewayRequestHandler` and `IAppNotificationHandler`
- `SettingsDelegate`: Platform-specific implementation bridge

**API Categories:**

#### Device Information APIs
- `device.make`: Manufacturer name
- `device.name`: User-friendly device name
- `device.sku`: Stock Keeping Unit identifier
- `device.model`: Device model information

#### Localization APIs
- `localization.countryCode`: ISO country code
- `localization.timeZone`: IANA timezone
- `localization.locale`: BCP-47 locale identifier
- `localization.language`: Preferred language
- `Localization.addAdditionalInfo`: Extended localization metadata

#### User Settings APIs
- **Accessibility:**
  - Voice guidance (enabled/disabled, speed, hints)
  - Audio descriptions
  - Closed captions with styling preferences
  - High contrast mode
  
- **Display:**
  - Screen resolution
  - Video resolution
  - HDR capabilities
  - HDCP status
  
- **Audio:**
  - Audio output configuration
  - Preferred audio languages
  
#### Network APIs
- Internet connection status

**Implementation Pattern:**
```cpp
Core::hresult HandleAppGatewayRequest(
    const Exchange::GatewayContext &context,
    const string& method,
    const string &payload,
    string& result)
{
    // Parse method name
    // Dispatch to appropriate handler
    // Execute through SettingsDelegate
    // Format and return result
}
```

### 3. AppNotifications Plugin

**Purpose:** Manages application events and notifications.

**Key Classes:**
- `AppNotifications`: Main plugin implementing `IPlugin` and `JSONRPC`
- `AppNotificationsImplementation`: Event emission and listener management

**Event Flow:**
1. Application subscribes to events via notification API
2. System events occur (network change, setting update, etc.)
3. AppGatewayCommon generates event
4. AppNotifications distributes to subscribed listeners
5. Applications receive event notifications

**Event Registration:**
```cpp
Core::hresult HandleAppEventNotifier(
    IEmitter *callback,
    const string& event,
    bool listen,
    bool& status)
{
    // Register/unregister event listener
    // Manage callback lifecycle
    // Route events to appropriate listeners
}
```

## Communication Patterns

### JSON-RPC Communication
Used for most gateway-to-plugin communication:
- Standard Thunder JSON-RPC protocol
- Method name: `<callsign>.<version>.<method>`
- Supports both synchronous and asynchronous patterns
- Example: `org.rdk.AppGatewayCommon.1.device.name`

### COM-RPC Communication
Used for high-performance or cross-process communication:
- Direct interface invocation via Thunder COM-RPC
- Type-safe interface definitions
- Lower overhead than JSON-RPC
- Indicated by `useComRpc: true` in resolution config

### Event Notification Pattern
- Publisher-subscriber model
- Events flow from plugins to applications
- Centralized event routing through AppNotifications
- Support for event filtering and context

## Permission System

### Permission Groups
Sensitive operations are protected by permission groups:
- `org.rdk.permission.group.enhanced`: System modification operations
- Additional groups can be defined per platform

### Permission Enforcement
1. Resolution configuration specifies required permission
2. Gateway checks caller's permission grants
3. Request rejected if permission not granted
4. Permission context passed to implementation plugins

## Data Flow Examples

### Simple Getter Request
```
Application → device.name()
    ↓
AppGateway: Look up "device.name" → org.rdk.AppGatewayCommon
    ↓
AppGatewayCommon.HandleAppGatewayRequest("device.name")
    ↓
SettingsDelegate.GetDeviceName()
    ↓
Thunder System Plugin → deviceName
    ↓
Return: "Living Room TV"
```

### Setter with Permissions
```
Application → device.setName("Bedroom TV")
    ↓
AppGateway: Check "org.rdk.permission.group.enhanced"
    ↓ (if authorized)
AppGatewayCommon.HandleAppGatewayRequest("device.setName", "Bedroom TV")
    ↓
SettingsDelegate.SetDeviceName("Bedroom TV")
    ↓
Thunder System Plugin → persist change
    ↓
Emit deviceNameChanged event
    ↓
Return: success
```

### Event Notification
```
Network Status Change (Thunder Network Plugin)
    ↓
AppGatewayCommon: onInternetStatusChange event
    ↓
AppNotifications: Broadcast to subscribers
    ↓
Applications: Receive network.onInternetStatusChange callback
```

## Configuration Management

### Resolution Configuration Layering
Multiple configuration files can be layered:
1. **Base configuration** (`resolution.base.json`): Default mappings
2. **Build configuration** (optional): Build-time customizations
3. **Vendor configuration** (optional): Vendor-specific overrides

The resolver merges these configurations, with later configs overriding earlier ones.

### Runtime Configuration
- Configuration loaded at plugin initialization
- Can be reloaded without restarting Thunder
- Validates configuration schema on load
- Falls back to defaults on parse errors

## Threading Model

### Plugin Threading
- Thunder manages plugin lifecycle on main thread
- Plugins can spawn worker threads for long operations
- Event notifications dispatched on Thunder worker pool
- Synchronization via mutexes for shared state

### Request Handling
- Synchronous requests block caller until complete
- Asynchronous requests return immediately with callback
- Worker threads handle I/O and blocking operations
- Results marshaled back to caller's context

## Error Handling

### Error Codes
Uses Thunder's `Core::hresult` error codes:
- `Core::ERROR_NONE`: Success
- `Core::ERROR_GENERAL`: Generic failure
- `Core::ERROR_UNAVAILABLE`: Service not available
- `Core::ERROR_ILLEGAL_STATE`: Invalid state for operation
- `Core::ERROR_PRIVILIGED_REQUEST`: Permission denied

### Error Propagation
1. Implementation layer returns error code
2. Gateway transforms to Firebolt error format
3. Application receives structured error response

## Extension Points

### Adding New APIs
1. Define method in resolution configuration
2. Implement handler in appropriate plugin (or create new plugin)
3. Update interface definitions if needed
4. Add tests for new functionality

### Custom Implementations
- Platform vendors can override `SettingsDelegate` implementations
- Custom plugins can be referenced in resolution config
- Vendor-specific resolution overlays supported

### Protocol Extensions
- New communication protocols can be added to resolver
- Custom marshaling logic for specialized data types
- Event routing extensions via AppNotifications

## Security Considerations

### Input Validation
- All input parameters validated at gateway layer
- Type checking before forwarding to implementation
- Sanitization of string inputs to prevent injection

### Permission Enforcement
- Checked at gateway before delegation
- Cannot be bypassed by direct plugin access
- Audited via Thunder's security logging

### Token Validation
- Optional security token validation (can be disabled for testing)
- Tokens validated against Thunder's security framework
- Per-request token authentication

## Performance Considerations

### Resolution Caching
- Configuration parsed once at startup
- Resolution mappings cached in memory
- O(1) lookup for method resolution

### Communication Overhead
- COM-RPC preferred for high-frequency calls
- JSON-RPC uses optimized parsing (Thunder's JSON library)
- Event notifications batched when possible

### Resource Management
- Plugin resources released on deinitialization
- Event listeners cleaned up on application disconnect
- Bounded memory usage for request queues

## Testing Strategy

### Unit Tests (L1)
- Mock Thunder interfaces
- Test individual plugin components
- Validate resolution logic
- Permission checking tests

### Integration Tests (L2)
- Full plugin stack testing
- Thunder framework integration
- End-to-end API flows
- Event notification tests

### Test Fixtures
- Common mock implementations in `entservices-testframework`
- Reusable test utilities
- Consistent test patterns across plugins

## Future Enhancements

### Planned Features
- Dynamic plugin discovery
- Hot-reload of resolution configuration
- Enhanced telemetry and metrics
- GraphQL interface option

### Optimization Opportunities
- Connection pooling for plugin communication
- Lazy plugin loading
- Request batching
- Event filtering optimizations
