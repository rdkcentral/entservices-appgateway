# App Gateway API Reference

This document provides detailed information about the interfaces and APIs exposed by the App Gateway system.

## Table of Contents

- [Core Interfaces](#core-interfaces)
- [Request/Response Format](#requestresponse-format)
- [Context Structure](#context-structure)
- [Authentication](#authentication)
- [Common API Methods](#common-api-methods)
- [Event Subscription](#event-subscription)
- [Error Codes](#error-codes)
- [Best Practices](#best-practices)

## Core Interfaces

See [DESIGN.md](../DESIGN.md) for detailed interface descriptions.

## Request/Response Format

### JSON-RPC 2.0 Request

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.name",
    "params": {}
}
```

### JSON-RPC 2.0 Response (Success)

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "name": "My Device"
    }
}
```

### JSON-RPC 2.0 Response (Error)

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32603,
        "message": "Internal error"
    }
}
```

## Context Structure

The `GatewayContext` carries request metadata throughout the system.

## Authentication

Applications connect via WebSocket with authentication token:

```
ws://gateway-host:port/appgateway?token=<auth-token>&RPCV2=true
```

## Common API Methods

### Device Information
- `device.name` - Get device name
- `device.setName` - Set device name
- `device.make` - Get manufacturer
- `device.sku` - Get SKU

### Localization
- `localization.countryCode` - Get/set country code
- `localization.timeZone` - Get/set time zone

### Accessibility
- `accessibility.voiceGuidance` - Get/set voice guidance
- `accessibility.captions` - Get/set captions

### Lifecycle
- `lifecycle.ready` - Signal app ready
- `lifecycle.finished` - Signal app finished
- `lifecycle.close` - Request close
- `lifecycle.state` - Get app state

## Event Subscription

Subscribe by appending `.listen` to method name:

```json
{
    "method": "device.nameChanged.listen",
    "params": { "listen": true }
}
```

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | ERROR_NONE | Success |
| 1 | ERROR_GENERAL | General failure |
| 22 | ERROR_INVALID_PARAMETER | Invalid input |

## Best Practices

1. Maintain single WebSocket connection
2. Use unique request IDs
3. Subscribe selectively to events
4. Handle errors gracefully

## Related Documentation

- [DESIGN.md](../DESIGN.md) - System architecture
- [README.md](../README.md) - Project overview
