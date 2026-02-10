# App Gateway Design Documentation

## Overview

The App Gateway is a Thunder (WPEFramework) plugin that provides a Firebolt-compatible API gateway for applications running on RDK devices. It acts as an intermediary between applications and Thunder plugins, providing request routing, event handling, authentication, and notification management.

This implementation deprecates the legacy Ripple Gateway from the RDK Apps Managers Framework; Ripple Gateway support is being phased out and will be removed after the completion of the migration period described in the "Ripple Gateway Migration Guide" in the RDK Apps Managers Framework documentation.

## System Architecture

```
┌─────────────────┐
│   Application   │
│  (WebSocket/    │
│   JSON-RPC)     │
└────────┬────────┘
         │
         ↓
┌────────────────────────────────────────────────────┐
│             AppGateway Plugin                      │
│  ┌──────────────────────────────────────────────┐ │
│  │  AppGatewayResponderImplementation           │ │
│  │  - WebSocket Management                      │ │
│  │  - Request/Response Handling                 │ │
│  │  - Event Emission                            │ │
│  └────────┬───────────────────────────────────┬─┘ │
│           │                                   │   │
│           ↓                                   ↓   │
│  ┌────────────────────┐         ┌─────────────────┐│
│  │ AppGatewayImplementation│     │AppNotifications││
│  │ - Request Resolution     │     │- Event Routing ││
│  │ - Resolver Integration   │     │- Subscriptions││
│  │ - Context Management     │     └─────────────────┘│
│  └───────┬──────────────────┘                       │
│          │                                           │
│          ↓                                           │
│  ┌────────────────┐                                 │
│  │   Resolver     │                                 │
│  │  - Alias       │                                 │
│  │    Mapping     │                                 │
│  └────────────────┘                                 │
└───────────────┬────────────────────────────────────┘
                │
                ↓
    ┌───────────────────────┐
    │  AppGatewayCommon     │
    │  - Request Handler    │
    │  - Settings Delegate  │
    │  - Authentication     │
    │  - Lifecycle Mgmt     │
    └───────────┬───────────┘
                │
                ↓
    ┌───────────────────────┐
    │ Thunder Plugins       │
    │ (System, UserSettings,│
    │  Display, Audio, etc) │
    └───────────────────────┘
```

## Core Components

### 1. AppGateway Plugin

**File:** `AppGateway/AppGateway.h`, `AppGateway/AppGateway.cpp`

The main plugin class that:
- Inherits from `PluginHost::IPlugin` and `PluginHost::JSONRPC`
- Provides the entry point for the Thunder framework
- Aggregates the resolver and responder interfaces
- Manages plugin lifecycle (Initialize/Deinitialize)

**Key Responsibilities:**
- Plugin registration with Thunder
- Interface aggregation
- Connection management
- Component initialization and cleanup

### 2. AppGatewayResponderImplementation

**File:** `AppGateway/AppGatewayResponderImplementation.h`, `AppGateway/AppGatewayResponderImplementation.cpp`

Manages WebSocket connections and handles bidirectional communication with applications.

**Key Responsibilities:**
- WebSocket connection management via `WebSocketConnectionManager`
- Response delivery to applications
- Event emission to subscribed clients
- Request proxying from applications to Thunder
- Connection context management
- JSON-RPC compliance tracking (supports both legacy and RPC v2)

**Key Methods:**
- `Respond()` - Send response back to application
- `Emit()` - Push events to specific connection
- `Request()` - Forward application requests to Thunder
- `GetGatewayConnectionContext()` - Retrieve connection-specific context
- `Register()/Unregister()` - Manage connection status notifications

**Internal Components:**
- `AppIdRegistry` - Maps connection IDs to application IDs
- `CompliantJsonRpcRegistry` - Tracks JSON-RPC v2 compliant connections
- Job classes for asynchronous dispatch

### 3. AppGatewayImplementation

**File:** `AppGateway/AppGatewayImplementation.h`, `AppGateway/AppGatewayImplementation.cpp`

The core request resolution engine that processes incoming API calls.

**Key Responsibilities:**
- Request resolution and routing
- Context enrichment (session, app ID, permissions)
- COM-RPC vs JSON-RPC routing decisions
- Event registration handling
- Authentication integration
- Configuration management

**Key Methods:**
- `Resolve()` - Main entry point for request resolution
- `Configure()` - Configure resolution paths
- `InternalResolve()` - Internal resolution logic
- `ProcessComRpcRequest()` - Handle COM-RPC requests
- `PreProcessEvent()` - Event subscription preprocessing
- `HandleEvent()` - Event registration/deregistration

**Request Flow:**
1. Receive request with context (origin, method, params)
2. Update context with session info, app ID, authentication
3. Check resolver for method mapping
4. Route to COM-RPC or internal resolution
5. Return result to responder

### 4. Resolver

**File:** `AppGateway/Resolver.h`, `AppGateway/Resolver.cpp`

Provides method alias resolution and plugin routing configuration.

**Key Responsibilities:**
- Load and parse resolution configuration files
- Map Firebolt API methods to Thunder plugin methods
- Provide permission group information
- Support event metadata
- Enable context injection

**Configuration Structure:**
```json
{
  "resolutions": [
    {
      "method": "firebolt.method.name",
      "alias": "ThunderPlugin.method",
      "event": "eventName",
      "permissionGroup": "groupName",
      "includeContext": true,
      "additionalContext": {},
      "useComRpc": true
    }
  ]
}
```

**Key Methods:**
- `LoadConfig()` - Load resolution configuration
- `ResolveAlias()` - Get Thunder method for Firebolt method
- `HasComRpcRequestSupport()` - Check if method uses COM-RPC
- `HasEvent()` - Check if method has event support
- `HasIncludeContext()` - Check if context injection is needed
- `HasPermissionGroup()` - Get permission requirements
- `CallThunderPlugin()` - Direct Thunder plugin invocation

### 5. AppGatewayCommon

**File:** `AppGatewayCommon/AppGatewayCommon.h`, `AppGatewayCommon/AppGatewayCommon.cpp`

Provides common request handling and authentication services.

**Key Responsibilities:**
- Implements `IAppGatewayRequestHandler` for application requests
- Implements `IAppNotificationHandler` for event management
- Implements `IAppGatewayAuthenticator` for security
- Lifecycle management (ready, finished, close, state)
- System/device information retrieval
- User settings management
- Network status monitoring

**Key Interfaces:**

**IAppGatewayRequestHandler:**
- `HandleAppGatewayRequest()` - Process application requests

**IAppNotificationHandler:**
- `HandleAppEventNotifier()` - Manage event subscriptions

**IAppGatewayAuthenticator:**
- `Authenticate()` - Validate session and get app ID
- `GetSessionId()` - Retrieve session for app ID
- `CheckPermissionGroup()` - Verify app permissions

**Handler Methods:**
- Device: `GetDeviceMake()`, `GetDeviceName()`, `SetDeviceName()`, `GetDeviceSku()`
- Settings: `GetCountryCode()`, `SetCountryCode()`, `GetTimeZone()`, `SetTimeZone()`
- User Settings: `GetVoiceGuidance()`, `SetVoiceGuidance()`, `GetCaptions()`, etc.
- Network: `GetInternetConnectionStatus()`
- Lifecycle: `LifecycleReady()`, `LifecycleFinished()`, `LifecycleClose()`, `LifecycleState()`

**SettingsDelegate:**
Located in `AppGatewayCommon/delegate/`, handles interactions with Thunder plugins for settings management.

### 6. AppNotifications

**File:** `AppNotifications/AppNotificationsImplementation.h`, `AppNotifications/AppNotificationsImplementation.cpp`

Manages event subscriptions and notification routing.

**Key Responsibilities:**
- Subscribe to Thunder plugin events
- Maintain subscriber registry
- Route events to subscribed applications
- Manage subscription lifecycle
- Support both gateway and launch delegate destinations

**Internal Components:**

**SubscriberMap:**
- Maintains event subscription mappings
- Tracks which contexts are subscribed to which events
- Dispatches events to appropriate destinations
- Cleans up subscriptions on disconnect

**ThunderSubscriptionManager:**
- Manages subscriptions to Thunder plugin events
- Creates notification handlers for Thunder events
- Forwards Thunder events to AppNotifications
- Handles subscription lifecycle

**Key Methods:**
- `Subscribe()` - Register application for event
- `Unsubscribe()` - Unregister application from event
- `OnNotification()` - Receive event from Thunder
- `Dispatch()` - Route event to subscribers
- `CleanupNotifications()` - Remove subscriptions for disconnected clients

## Communication Flows

### Request Flow (Application → Thunder)

```
Application
  │
  │ WebSocket + JSON-RPC
  ↓
AppGatewayResponderImplementation
  │ Request()
  │ - Parse WebSocket message
  │ - Extract connectionId, requestId, method, params
  ↓
AppGatewayImplementation
  │ Resolve()
  │ - Build GatewayContext (connectionId, requestId)
  │ - Authenticate session
  │ - Check permissions
  │ - Enrich context
  ↓
Resolver
  │ ResolveAlias()
  │ - Map Firebolt method → Thunder method
  │ - Check if COM-RPC or JSON-RPC
  │ - Retrieve event metadata
  ↓
AppGatewayCommon OR Thunder Plugin
  │ HandleAppGatewayRequest() OR Direct COM-RPC
  │ - Execute business logic
  │ - Query Thunder services
  │ - Return result
  ↓
AppGatewayResponderImplementation
  │ Respond()
  │ - Send response to WebSocket connection
  ↓
Application
```

### Event Flow (Thunder → Application)

```
Thunder Plugin
  │ Event Triggered
  ↓
AppNotifications
  │ ThunderSubscriptionManager::HandleNotification()
  │ - Receive Thunder event
  │ - Extract event name and payload
  ↓
  │ SubscriberMap::EventUpdate()
  │ - Find subscribed contexts
  │ - For each subscriber:
  ↓
AppGatewayResponderImplementation
  │ Emit()
  │ - Send event to connection
  │ - Format as JSON-RPC notification
  ↓
Application
  │ Receive event via WebSocket
```

### Event Subscription Flow

```
Application
  │ Subscribe request (method name with ".listen" suffix)
  ↓
AppGatewayImplementation
  │ PreProcessEvent()
  │ - Detect ".listen" method
  │ - Extract event name
  │ - Determine listen/unlisten
  ↓
AppNotifications
  │ Subscribe() / Unsubscribe()
  │ - Update SubscriberMap
  │ - Subscribe to Thunder if first subscriber
  │ - Unsubscribe from Thunder if last subscriber
  ↓
Thunder Plugin
  │ Register notification handler
```

## Context Management

The `GatewayContext` structure carries request metadata throughout the system:

```cpp
struct GatewayContext {
    uint32_t connectionId;  // WebSocket connection identifier
    uint32_t requestId;     // Request identifier for response matching
    string appId;           // Authenticated application identifier
    string sessionId;       // Session token
    // Additional context fields injected by resolver
}
```

**Context Flow:**
1. Created in `AppGatewayResponderImplementation` from WebSocket connection
2. Enriched in `AppGatewayImplementation` with authentication info
3. Enhanced by `Resolver` with additional context if configured
4. Passed to handlers for authorization and processing
5. Used for response routing back to originating connection

## Authentication and Authorization

### Authentication Flow

1. Application connects with authentication token in WebSocket URL
2. `AppGatewayCommon::Authenticate()` validates token
3. Returns `appId` associated with session
4. `appId` stored in context for all subsequent requests

### Authorization Flow

1. Resolver configuration specifies `permissionGroup` for each method
2. Before execution, `CheckPermissionGroup(appId, permissionGroup)` is called
3. Decision made based on app capabilities and required permissions
4. Unauthorized requests are rejected with appropriate error

## Lifecycle Management

AppGatewayCommon provides lifecycle management for applications:

### Lifecycle 1.0
- `lifecycle.ready` - Application signals ready state
- `lifecycle.finished` - Application signals completion
- `lifecycle.close` - Request to close application
- `lifecycle.state` - Query application state

### Lifecycle 2.0
- `lifecycle2.state` - Enhanced state reporting
- `lifecycle2.close` - Enhanced close with parameters
- Extended state information and control

## Configuration

### Resolver Configuration

Resolution configuration files define the API mapping:

**Location:** `AppGateway/resolutions/`

**Example Entry:**
```json
{
  "method": "device.name",
  "alias": "AppGatewayCommon.getDeviceName",
  "permissionGroup": "device:info",
  "includeContext": false
}
```

**Example Event Entry:**
```json
{
  "method": "device.nameChanged",
  "event": "nameChanged",
  "alias": "AppGatewayCommon.deviceNameChanged"
}
```

### Plugin Configuration

Thunder plugin configuration files (`.config`):
- Connection parameters
- Service dependencies
- Runtime options

## Threading and Concurrency

### Job-based Asynchronous Execution

The system uses Thunder's `Core::IDispatch` job pattern for asynchronous execution:

**Job Types:**
- `RespondJob` - Asynchronous response delivery
- `EmitJob` - Asynchronous event emission
- `RequestJob` - Asynchronous request forwarding
- `WsMsgJob` - WebSocket message processing
- `EventRegistrationJob` - Event subscription processing
- `ConnectionStatusNotificationJob` - Connection status updates

**Pattern:**
```cpp
Core::ProxyType<Core::IDispatch> job = 
    JobClass::Create(parent, params...);
service->Submit(job);
```

### Thread Safety

- **Mutex Protection:** Critical sections protected with `std::mutex`
  - `SubscriberMap::mSubscriberMutex` - Subscription data
  - `AppIdRegistry::mAppIdMutex` - Connection mappings
  - `CompliantJsonRpcRegistry::mCompliantJsonRpcMutex` - RPC compliance tracking
  - `Resolver::mMutex` - Resolution configuration
  
- **Thunder Locks:** `Core::CriticalSection` for Thunder framework integration
  - `mConnectionStatusImplLock` - Connection status notifications

## Error Handling

### Error Codes

Uses Thunder framework error codes:
- `Core::ERROR_NONE` - Success
- `Core::ERROR_GENERAL` - General failure
- `Core::ERROR_INVALID_PARAMETER` - Invalid input
- `Core::ERROR_UNAVAILABLE` - Service not available
- `Core::ERROR_ILLEGAL_STATE` - Invalid state for operation

### Error Propagation

1. Errors originate from Thunder plugins or internal logic
2. Propagated through return codes (`Core::hresult`)
3. Converted to JSON-RPC error responses
4. Sent back to application via WebSocket

## JSON-RPC Support

### JSON-RPC 2.0 Compliance

The system supports both legacy and JSON-RPC 2.0 formats:

**Detection:** 
- Connections with `RPCV2=true` in authentication token are treated as compliant
- Tracked in `CompliantJsonRpcRegistry`

**Format Differences:**
- **Legacy:** Simpler format, custom error handling
- **RPC 2.0:** Standard JSON-RPC 2.0 format with `jsonrpc: "2.0"`, `id`, `method`, `params`

## Dependencies

### Thunder Framework Components
- `PluginHost::IShell` - Plugin shell interface
- `PluginHost::IPlugin` - Plugin base interface
- `PluginHost::JSONRPC` - JSON-RPC server functionality
- `Core::IDispatch` - Asynchronous job dispatch
- `Exchange::IAppGateway*` - Custom interfaces

### External Services
- Thunder plugins (System, Display, UserSettings, etc.)
- Authentication service
- Settings storage

## Extension Points

### Adding New Methods

1. Add resolution entry in `resolution.base.json`:
   ```json
   {
     "method": "new.method",
     "alias": "TargetPlugin.method",
     "permissionGroup": "group:permission"
   }
   ```

2. If custom handling needed, add handler in `AppGatewayCommon`:
   ```cpp
   Core::hresult HandleNewMethod(const GatewayContext&, const string& params, string& result);
   ```

3. Register in handler map:
   ```cpp
   {"new.method", &AppGatewayCommon::HandleNewMethod}
   ```

### Adding New Events

1. Add event entry in resolution configuration
2. Subscribe to Thunder event in `AppNotifications`
3. Map event to notification handler
4. Event automatically routed to subscribed applications

## Performance Considerations

### Optimization Strategies

1. **Asynchronous Processing:** All I/O and potentially blocking operations executed via job dispatch
2. **Connection Pooling:** WebSocket connections reused for multiple requests
3. **Event Batching:** Multiple events can be dispatched efficiently
4. **Lazy Initialization:** Services queried only when needed
5. **Lock Minimization:** Critical sections kept small

### Resource Management

- **Reference Counting:** COM-style AddRef/Release for interface lifecycle
- **RAII:** Automatic cleanup via destructors
- **Proxy Types:** Thunder's `Core::ProxyType` for managed pointers

## Testing

### Test Structure

**Location:** `Tests/`

- **L1 Tests:** Unit tests for individual components
- **Mock Files:** Located in `Tests/CopilotFiles/` with instructions for generating mocks
- **Test Infrastructure:** GTest-based testing framework

### Test Coverage Areas

1. Request resolution
2. Event subscription and dispatch
3. Authentication and authorization
4. Context management
5. WebSocket communication
6. Lifecycle management

## Logging

Uses Thunder's logging macros:
- `LOGINFO()` - Informational messages
- `LOGERR()` - Error messages
- `LOGDBG()` - Debug messages (when enhanced logging enabled)

**Enhanced Logging:**
Can be enabled for detailed WebSocket message tracing with connection and request IDs.

## Future Enhancements

### Potential Improvements

1. **Metrics and Monitoring:** Add telemetry for request latency, error rates
2. **Rate Limiting:** Protect against abuse from misbehaving applications
3. **Request Validation:** JSON schema validation for request parameters
4. **Caching:** Cache frequently accessed data (device info, settings)
5. **Load Balancing:** Support multiple gateway instances
6. **Enhanced Security:** Token refresh, scope validation, audit logging

## Related Documentation

- [README.md](README.md) - Project overview and build instructions
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines
- [CHANGELOG.md](CHANGELOG.md) - Version history and changes
- [Tests/CopilotFiles/copilot-instructions-mock.md](Tests/CopilotFiles/copilot-instructions-mock.md) - Mock generation guide
- [Tests/CopilotFiles/l1_tests.instructions.md](Tests/CopilotFiles/l1_tests.instructions.md) - L1 testing guide

## Glossary

- **Thunder/WPEFramework:** RDK's plugin framework
- **Firebolt:** RDK's application API standard
- **COM-RPC:** Component Object Model Remote Procedure Call
- **Gateway Context:** Request metadata structure
- **Resolver:** Component mapping Firebolt to Thunder methods
- **Responder:** Component handling WebSocket communication
- **Handler:** Component processing specific API requests
- **Alias:** Thunder method name mapped from Firebolt method
