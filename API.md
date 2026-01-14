# API Documentation

This document describes the APIs provided by the App Gateway plugins.

## Overview

The App Gateway exposes Firebolt APIs through Thunder plugins. Applications interact with these APIs via JSON-RPC calls to the AppGateway plugin, which routes requests to the appropriate implementation plugins.

## API Endpoint

All API calls are made via Thunder's JSON-RPC interface:

**Base URL:** `http://<device-ip>:9998/jsonrpc/AppGateway`

**Request Format:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "<api.method>",
    "params": { ... }
}
```

**Response Format:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": { ... }
}
```

**Error Format:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32000,
        "message": "Error description"
    }
}
```

## Device APIs

### device.make
Get the device manufacturer name.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.make"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "Samsung"
}
```

**Permissions:** None required

---

### device.name
Get the user-friendly device name.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.name"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "Living Room TV"
}
```

**Permissions:** None required

---

### device.setName
Set the user-friendly device name.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.setName",
    "params": {
        "value": "Bedroom TV"
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### device.model
Get the device model identifier.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.model"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "UN65RU8000"
}
```

**Permissions:** None required

---

### device.sku
Get the device Stock Keeping Unit (SKU).

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.sku"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "RDK-X1-2023"
}
```

**Permissions:** None required

---

## Localization APIs

### localization.countryCode
Get the ISO 3166-1 alpha-2 country code.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.countryCode"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "US"
}
```

**Permissions:** None required

---

### localization.setCountryCode
Set the country code.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.setCountryCode",
    "params": {
        "value": "CA"
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### localization.locale
Get the BCP-47 locale identifier.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.locale"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "en-US"
}
```

**Permissions:** None required

---

### localization.setLocale
Set the locale.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.setLocale",
    "params": {
        "value": "es-MX"
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### localization.language
Get the presentation language (BCP-47 language code).

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.language"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "en"
}
```

**Permissions:** None required

---

### localization.timeZone
Get the IANA timezone identifier.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.timeZone"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "America/New_York"
}
```

**Permissions:** None required

---

### localization.setTimeZone
Set the timezone.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.setTimeZone",
    "params": {
        "value": "America/Los_Angeles"
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### Localization.addAdditionalInfo
Add additional localization information.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "Localization.addAdditionalInfo",
    "params": {
        "key": "region",
        "value": "North America"
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

## Accessibility APIs

### accessibility.voiceGuidance
Get voice guidance settings.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.voiceGuidance"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "enabled": true,
        "speed": 1.0
    }
}
```

**Permissions:** None required

---

### accessibility.setVoiceGuidance
Enable or disable voice guidance.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.setVoiceGuidance",
    "params": {
        "enabled": true
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### accessibility.voiceGuidanceSpeed
Get voice guidance speed.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.voiceGuidanceSpeed"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": 1.5
}
```

**Permissions:** None required

---

### accessibility.setVoiceGuidanceSpeed
Set voice guidance speed (0.5 - 2.0).

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.setVoiceGuidanceSpeed",
    "params": {
        "speed": 1.5
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### accessibility.closedCaptions
Get closed captions enabled state.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.closedCaptions"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** None required

---

### accessibility.setClosedCaptions
Enable or disable closed captions.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.setClosedCaptions",
    "params": {
        "enabled": true
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

### accessibility.closedCaptionsSettings
Get detailed closed captions settings including styling preferences.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.closedCaptionsSettings"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "enabled": true,
        "fontFamily": "monospace_serif",
        "fontSize": 1.0,
        "fontColor": "#FFFFFF",
        "fontEdge": "none",
        "fontOpacity": 100,
        "backgroundColor": "#000000",
        "backgroundOpacity": 75,
        "windowColor": "#000000",
        "windowOpacity": 0
    }
}
```

**Permissions:** None required

---

### accessibility.audioDescriptions
Get audio descriptions enabled state.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.audioDescriptions"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** None required

---

### accessibility.setAudioDescriptions
Enable or disable audio descriptions.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "accessibility.setAudioDescriptions",
    "params": {
        "enabled": true
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

## Display APIs

### device.screenResolution
Get the screen resolution.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.screenResolution"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": [1920, 1080]
}
```

**Permissions:** None required

---

### device.videoResolution
Get the current video output resolution.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.videoResolution"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": [3840, 2160]
}
```

**Permissions:** None required

---

### device.hdr
Get HDR capability.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.hdr"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "hdr10": true,
        "hdr10Plus": true,
        "dolbyVision": true,
        "hlg": true
    }
}
```

**Permissions:** None required

---

### device.hdcp
Get HDCP status.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.hdcp"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "supported": true,
        "version": "2.2"
    }
}
```

**Permissions:** None required

---

## Audio APIs

### device.audio
Get audio output configuration.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.audio"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "stereo": true,
        "dolbyDigital5.1": true,
        "dolbyDigital5.1+": true,
        "dolbyAtmos": true,
        "dolbyDigitalPlus": true
    }
}
```

**Permissions:** None required

---

### localization.preferredAudioLanguages
Get preferred audio language list.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.preferredAudioLanguages"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": ["en", "es"]
}
```

**Permissions:** None required

---

### localization.setPreferredAudioLanguages
Set preferred audio languages (ISO 639-2 codes).

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "localization.setPreferredAudioLanguages",
    "params": {
        "value": ["en", "fr", "es"]
    }
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": true
}
```

**Permissions:** `org.rdk.permission.group.enhanced`

---

## Network APIs

### device.network
Get internet connection status.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.network"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "state": "connected",
        "type": "wifi"
    }
}
```

**Possible states:** `"connected"`, `"disconnected"`, `"connecting"`
**Possible types:** `"wifi"`, `"ethernet"`, `"hybrid"`

**Permissions:** None required

---

## System Information APIs

### device.version.firmware
Get device firmware version.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "device.version.firmware"
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": "2.0.1-beta.5"
}
```

**Permissions:** None required

---

## Event Notifications

Applications can subscribe to events to receive notifications when system state changes.

### Subscribing to Events

**Subscribe Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "AppNotifications.register",
    "params": {
        "event": "device.onNameChanged"
    }
}
```

**Subscribe Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": 0
}
```

### Unsubscribing from Events

**Unsubscribe Request:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "AppNotifications.unregister",
    "params": {
        "event": "device.onNameChanged"
    }
}
```

### Event Notifications

When an event occurs, subscribers receive a notification:

**Event Notification:**
```json
{
    "jsonrpc": "2.0",
    "method": "device.onNameChanged",
    "params": {
        "value": "New Device Name"
    }
}
```

### Available Events

- `device.onNameChanged` - Device name was changed
- `localization.onCountryCodeChanged` - Country code was changed
- `localization.onLocaleChanged` - Locale was changed
- `localization.onTimeZoneChanged` - Timezone was changed
- `accessibility.onVoiceGuidanceChanged` - Voice guidance settings changed
- `accessibility.onClosedCaptionsChanged` - Closed captions state changed
- `accessibility.onAudioDescriptionsChanged` - Audio descriptions state changed
- `network.onInternetStatusChange` - Internet connection status changed

---

## Error Codes

| Code | Message | Description |
|------|---------|-------------|
| -32000 | General error | Generic error condition |
| -32001 | Unavailable | Service or resource unavailable |
| -32002 | Invalid parameters | Request parameters are invalid |
| -32003 | Permission denied | Insufficient permissions for operation |
| -32004 | Invalid state | Operation not valid in current state |
| -32600 | Invalid request | JSON-RPC request is invalid |
| -32601 | Method not found | Method does not exist |
| -32602 | Invalid params | Invalid method parameters |
| -32603 | Internal error | Internal JSON-RPC error |

**Example Error Response:**
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32003,
        "message": "Permission denied: org.rdk.permission.group.enhanced required"
    }
}
```

---

## Testing APIs with curl

### Basic GET request:
```bash
curl -X POST http://localhost:9998/jsonrpc/AppGateway \
    -H "Content-Type: application/json" \
    -d '{
        "jsonrpc": "2.0",
        "id": 1,
        "method": "device.name"
    }'
```

### SET request with parameters:
```bash
curl -X POST http://localhost:9998/jsonrpc/AppGateway \
    -H "Content-Type: application/json" \
    -d '{
        "jsonrpc": "2.0",
        "id": 1,
        "method": "device.setName",
        "params": {
            "value": "My Device"
        }
    }'
```

### Subscribe to events:
```bash
curl -X POST http://localhost:9998/jsonrpc/AppNotifications \
    -H "Content-Type: application/json" \
    -d '{
        "jsonrpc": "2.0",
        "id": 1,
        "method": "AppNotifications.register",
        "params": {
            "event": "device.onNameChanged"
        }
    }'
```

---

## Permission System

Some APIs require elevated permissions to execute. Applications must be granted the appropriate permission group by the platform.

### Permission Groups

**org.rdk.permission.group.enhanced:**
Required for operations that modify system settings:
- Setting device name
- Changing localization settings
- Modifying accessibility settings
- Changing user preferences

**No permission required:**
Most getter/read operations do not require special permissions.

### Permission Errors

When an application lacks required permissions, the API returns error code `-32003`:

```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32003,
        "message": "Permission denied"
    }
}
```

---

## See Also

- [ARCHITECTURE.md](ARCHITECTURE.md) - Detailed architecture documentation
- [DEVELOPMENT.md](DEVELOPMENT.md) - Development guide
- [README.md](README.md) - Project overview
