# RDKEMW-22467: Firebolt VideoOutput APIs Implementation Plan

**Epic:** CPESP-9177 — Firebolt Video Output APIs
**Story:** RDKEMW-22467
**Sprint:** RDKE_Sprint_2608
**Story Points:** 8
**Assignee:** Sasikumar Janarthanan

---

## 1. Problem Statement

The Firebolt `VideoOutput` module defines a set of APIs for querying video output properties (resolution, HDCP, CEC state, port, refresh rate, color depth, dynamic range, etc.) and subscribing to change notifications. These APIs are **not yet implemented** in AppGateway.

Currently, apps must call low-level Thunder plugin APIs directly (`org.rdk.DisplaySettings`, `org.rdk.HdcpProfile`, `org.rdk.HdmiCecSource`, `DisplayInfo`), which:

- Exposes internal implementation details to apps
- Requires apps to perform complex response transformations
- Lacks a unified event model for video output changes
- Violates the Firebolt abstraction layer

---

## 2. Solution Overview

Implement **10 Firebolt VideoOutput API handlers** in AppGateway's `SystemDelegate` that:

1. **Map** incoming Firebolt requests to underlying Thunder plugin APIs
2. **Transform** Thunder responses to Firebolt-compliant types
3. **Subscribe** to Thunder change notifications and re-emit as Firebolt events

All new methods follow the **existing delegate pattern** already established in `SystemDelegate.h` (e.g., `GetScreenResolution`, `GetHdcp`, `GetHdr`, `GetAudio`).

### Plugins Involved

| Plugin | Callsign | Transport | Purpose |
|--------|----------|-----------|---------|
| `DisplayInfo` | `DisplayInfo` | COM-RPC / JSON-RPC | Display properties (framerate, color depth, colorspace, colorimetry, quantization range) |
| `org.rdk.DisplaySettings` | `org.rdk.DisplaySettings` | JSON-RPC | Resolution, video format, connected displays |
| `org.rdk.HdcpProfile` | `org.rdk.HdcpProfile` | JSON-RPC | HDCP authentication status |
| `org.rdk.HdmiCecSource` | `org.rdk.HdmiCecSource` | JSON-RPC | CEC source management (STB is a CEC source device) |

---

## 3. Existing Infrastructure (What We Already Have)

The following methods in `SystemDelegate.h` already implement similar patterns and can be referenced/reused:

| Existing Method | Thunder Plugin | Transport | Pattern |
|----------------|---------------|-----------|---------|
| `GetScreenResolution()` | `org.rdk.DisplaySettings.getCurrentResolution` | JSON-RPC | Query + transform `[w,h]` |
| `GetVideoResolution()` | Derived from `GetScreenResolution` | JSON-RPC | Re-query + infer UHD/FHD |
| `GetHdcp()` | `org.rdk.HdcpProfile.getHDCPStatus` | JSON-RPC | Query + map version string |
| `GetHdr()` | `org.rdk.DisplaySettings.getTVHDRCapabilities` | JSON-RPC | Query + parse bitmask |
| `GetAudio()` | `org.rdk.DisplaySettings.getAudioFormat` | JSON-RPC | Query + token matching |
| `GetDisplayEdid()` | `DisplayInfo` (IConnectionProperties::EDID) | **COM-RPC** | Query + Base64 encode |
| `GetDisplayColorimetry()` | `DisplayInfo` (IDisplayProperties::Colorimetry) | **COM-RPC** | Iterator + enum mapping |
| `GetDisplaySize()` | `DisplayInfo` (IConnectionProperties) | **COM-RPC** | Direct property access |
| `GetDisplayMaxResolution()` | `DisplayInfo` (IConnectionProperties) | **COM-RPC** | Direct property access |

### Existing Event Subscriptions

| Event Source | Thunder Notification | Firebolt Events Emitted |
|-------------|---------------------|------------------------|
| `org.rdk.DisplaySettings` | `resolutionChanged` | `Device.onScreenResolutionChanged`, `Device.onVideoResolutionChanged` |
| `org.rdk.HdcpProfile` | `onDisplayConnectionChanged` | `Device.onHdcpChanged`, `Device.onHdrChanged` |
| `org.rdk.DisplaySettings` | `audioFormatChanged` | `Device.onAudioChanged` |
| `org.rdk.System` (COM-RPC) | `OnFriendlyNameChanged` | `Device.onNameChanged`, `Device.onDeviceNameChanged` |
| `org.rdk.System` (COM-RPC) | `OnTimeZoneDSTChanged` | `Localization.onTimeZoneChanged` |
| `org.rdk.System` (COM-RPC) | `OnTerritoryChanged` | `Localization.onCountryChanged` |

---

## 4. APIs to Implement

### 4.1 API Summary Table

| # | Firebolt API | Firebolt Event | Thunder Plugin | Thunder Method | Transport | Notification | Default (no display) |
|---|---|---|---|---|---|---|---|
| 1 | `VideoOutput.resolution` | `onResolutionChanged` | `org.rdk.DisplaySettings` | `getCurrentResolution` | JSON-RPC | `resolutionChanged` | `{width:0, height:0}` |
| 2 | `VideoOutput.hdcp` | `onHdcpChanged` | `org.rdk.HdcpProfile` | `getHDCPStatus` | JSON-RPC | `onDisplayConnectionChanged` | `"none"` |
| 3 | `VideoOutput.cecActiveState` | `onCecActiveStateChanged` | `org.rdk.HdmiCecSource` | `getEnabled` + `getActiveSourceStatus` | JSON-RPC | `onActiveSourceStatusUpdated` | `"unsupported"` |
| 4 | `VideoOutput.port` | `onPortChanged` | `org.rdk.DisplaySettings` | `getConnectedVideoDisplays` | JSON-RPC | `connectedVideoDisplaysUpdated` | `"none"` |
| 5 | `VideoOutput.refreshRate` | `onRefreshRateChanged` | `DisplayInfo` | `FrameRate()` | **COM-RPC** | `updated` (POST_RESOLUTION_CHANGE) | `0` |
| 6 | `VideoOutput.colorDepth` | — | `DisplayInfo` | `ColourDepth()` | **COM-RPC** | Poll only | `0` |
| 7 | `VideoOutput.colorFormat` | — | `DisplayInfo` | `colorspace` | JSON-RPC | Poll only | `"none"` |
| 8 | `VideoOutput.colorimetry` | — | `DisplayInfo` | `getCurrentColorimetry` *(new)* | JSON-RPC | Re-poll on `HDMI_CHANGE` | `"none"` |
| 9 | `VideoOutput.dynamicRange` | — | `org.rdk.DisplaySettings` | `getVideoFormat` | JSON-RPC | `videoFormatChanged` | `"none"` |
| 10 | `VideoOutput.quantizationRange` | — | `DisplayInfo` | `quantizationrange` | JSON-RPC | Poll only | `"none"` |

---

## 5. Transformation Rules (per API)

### 5.1 VideoOutput.resolution

| Thunder `resolution` | Firebolt `{width, height}` |
|---------------------|---------------------------|
| `"480p"` / `"720x480"` | `{ "width": 720, "height": 480 }` |
| `"576p"` / `"720x576"` | `{ "width": 720, "height": 576 }` |
| `"720p"` / `"1280x720"` | `{ "width": 1280, "height": 720 }` |
| `"1080p"` / `"1920x1080"` | `{ "width": 1920, "height": 1080 }` |
| `"2160p"` / `"3840x2160"` | `{ "width": 3840, "height": 2160 }` |

### 5.2 VideoOutput.hdcp

| Condition | Firebolt `hdcp` |
|-----------|----------------|
| `isConnected: true` AND `isHDCPEnabled: true` AND `currentHDCPVersion: "2.2"` | `"hdcp2.2"` |
| `isConnected: true` AND `isHDCPEnabled: true` AND `currentHDCPVersion: "1.4"` | `"hdcp1.4"` |
| `isConnected: false` OR `isHDCPEnabled: false` | `"none"` |
| TV device (built-in display) | `"direct"` |

**Device Behaviour:**
- OTT/STB device: returns negotiated HDCP version, or `"none"` if no encrypted connection
- TV device: returns `"direct"`

### 5.3 VideoOutput.cecActiveState

| Thunder condition | Firebolt `cecActiveState` |
|-------------------|---------------------|
| `getEnabled: false` (CEC not enabled on TV) | `"unsupported"` |
| `getEnabled: true` AND `getActiveSourceStatus.status: true` | `"active"` |
| `getEnabled: true` AND `getActiveSourceStatus.status: false` | `"inactive"` |
| TV device | Error: `Wrong device class` |

**Device Behaviour:**
- OTT/STB device: `"active"` (TV on, STB is active source), `"inactive"` (TV off or not active), `"unsupported"` (CEC disabled)
- TV device: returns `Wrong device class` error

### 5.4 VideoOutput.port

| Thunder `connectedVideoDisplays` | Firebolt `port` |
|----------------------------------|----------------|
| Port name starts with `"hdmi"` (e.g. `"HDMI0"`) | `"hdmi"` |
| Empty array / no hot-plug | `"none"` |
| `"INTERNAL0"` (TV built-in panel) | `"internal"` |

**Device Behaviour:**
- OTT/STB device: `"hdmi"` if TV connected over HDMI; otherwise `"none"`
- TV device: `"internal"`

### 5.5 VideoOutput.refreshRate

| Thunder `FrameRateType` | Firebolt `refreshRate` (Hz) |
|------------------------|---------------------------|
| `FRAMERATE_UNKNOWN` / no TV attached | `0` |
| `FRAMERATE_23_976` | `23.976` |
| `FRAMERATE_24` | `24` |
| `FRAMERATE_25` | `25` |
| `FRAMERATE_29_97` | `29.97` |
| `FRAMERATE_30` | `30` |
| `FRAMERATE_50` | `50` |
| `FRAMERATE_59_94` | `59.94` |
| `FRAMERATE_60` | `60` |

### 5.6 VideoOutput.colorDepth

| Thunder `ColourDepthType` | Firebolt `colorDepth` (bits) |
|--------------------------|----------------------------|
| `COLORDEPTH_UNKNOWN` / display not connected | `0` |
| `COLORDEPTH_8_BIT` | `8` |
| `COLORDEPTH_10_BIT` | `10` |
| `COLORDEPTH_12_BIT` | `12` |

### 5.7 VideoOutput.colorFormat

| Thunder `colorspace` | Firebolt `colorFormat` |
|---------------------|----------------------|
| `FormatYcbcr420` | `"ycbcr420"` |
| `FormatYcbcr422` | `"ycbcr422"` |
| `FormatYcbcr444` | `"ycbcr444"` |
| `FormatRgb444` | `"rgb444"` |
| `FormatUnknown` / display not connected | `"none"` |

### 5.8 VideoOutput.colorimetry

| Thunder `colorimetry` | Firebolt `colorimetry` |
|----------------------|----------------------|
| `COLORIMETRY_BT709` (`dsDISPLAY_MATRIXCOEFFICIENT_BT_709`) | `"bt709"` |
| `COLORIMETRY_SMPTE170M` (`dsDISPLAY_MATRIXCOEFFICIENT_SMPTE_170M`) | `"smpte170m"` |
| `COLORIMETRY_XVYCC709` (`dsDISPLAY_MATRIXCOEFFICIENT_XvYCC_709`) | `"xvycc709"` |
| `COLORIMETRY_XVYCC601` (`dsDISPLAY_MATRIXCOEFFICIENT_eXvYCC_601`) | `"xvycc601"` |
| `COLORIMETRY_BT2020RGB_YCBCR` (`dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_NCL`) | `"bt2020rgb"` |
| `COLORIMETRY_BT2020YCCBCBRC` (`dsDISPLAY_MATRIXCOEFFICIENT_BT_2020_CL`) | `"bt2020ycc"` |
| `COLORIMETRY_OPRGB` | `"oprgb"` |
| `COLORIMETRY_UNKNOWN` / display not connected | `"none"` |

### 5.9 VideoOutput.dynamicRange

| Thunder `currentVideoFormat` | Firebolt `dynamicRange` |
|-----------------------------|------------------------|
| `"HDR10"` | `"hdr10"` |
| `"HDR10PLUS"` | `"hdr10plus"` |
| `"DV"` | `"dolbyVision"` |
| `"HLG"` | `"hlg"` |
| `"SDR"` | `"sdr"` |
| `"NONE"` / display not connected | `"none"` |

### 5.10 VideoOutput.quantizationRange

| Thunder `quantizationrange` | Firebolt `quantizationRange` |
|----------------------------|----------------------------|
| `QuantizationrangeLimited` | `"limited"` |
| `QuantizationrangeFull` | `"full"` |
| `QuantizationrangeUnknown` / display not connected | `"none"` |

---

## 6. Acceptance Criteria (from JIRA RDKEMW-22467)

| AC | API | Summary |
|----|-----|---------|
| AC1 | `VideoOutput.resolution` | Invoke `getCurrentResolution`, return `{width, height}` in pixels. Subscribe `resolutionChanged` → `onResolutionChanged` |
| AC2 | `VideoOutput.hdcp` | Invoke `getHDCPStatus`, map HDCP enum. TV → `"direct"`. Subscribe `onDisplayConnectionChanged` → `onHdcpChanged` |
| AC3 | `VideoOutput.cecActiveState` | Two-step: `getEnabled` then `getActiveSourceStatus`. TV → error. Subscribe `onActiveSourceStatusUpdated` → `onCecActiveStateChanged` |
| AC4 | `VideoOutput.port` | Invoke `getConnectedVideoDisplays`, derive port type. Subscribe `connectedVideoDisplaysUpdated` → `onPortChanged` |
| AC5 | `VideoOutput.refreshRate` | Call `DisplayInfo.FrameRate()` via ComRPC, map to Hz. Subscribe `updated` (POST_RESOLUTION_CHANGE) → `onRefreshRateChanged` |
| AC6 | `VideoOutput.colorDepth` | Call `DisplayInfo.ColourDepth()` via ComRPC, return bit-depth. Poll-only |
| AC7 | `VideoOutput.colorFormat` | Call `DisplayInfo.colorspace` JSON-RPC, map to Firebolt enum. Poll-only |
| AC8 | `VideoOutput.colorimetry` | Call `DisplayInfo.getCurrentColorimetry` JSON-RPC, map to Firebolt enum. Re-poll on `HDMI_CHANGE`. **Depends on CPESP-9177** ✅ Merged |
| AC9 | `VideoOutput.dynamicRange` | Invoke `getVideoFormat`, map to Firebolt enum. Subscribe `videoFormatChanged` |
| AC10 | `VideoOutput.quantizationRange` | Call `DisplayInfo.quantizationrange` JSON-RPC, map to Firebolt enum. Poll-only |
| AC11 | Error Handling | Return safe defaults on plugin unavailability. No raw Thunder errors to app |
| AC12 | L0 Unit Tests | All 10 handlers + delegate methods must have L0 coverage |

---

## 7. Implementation Steps

### Step 1: Add Resolution Entries (`resolution.base.json`)

Add 15 new entries to `AppGateway/resolutions/resolution.base.json`:

```json
"videooutput.resolution": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.onresolutionchanged": {
    "alias": "org.rdk.AppGatewayCommon",
    "event": "VideoOutput.onResolutionChanged"
},
"videooutput.hdcp": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.onhdcpchanged": {
    "alias": "org.rdk.AppGatewayCommon",
    "event": "VideoOutput.onHdcpChanged"
},
"videooutput.cecactivestate": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.oncecactivestatechanged": {
    "alias": "org.rdk.AppGatewayCommon",
    "event": "VideoOutput.onCecActiveStateChanged"
},
"videooutput.port": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.onportchanged": {
    "alias": "org.rdk.AppGatewayCommon",
    "event": "VideoOutput.onPortChanged"
},
"videooutput.refreshrate": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.onrefreshratechanged": {
    "alias": "org.rdk.AppGatewayCommon",
    "event": "VideoOutput.onRefreshRateChanged"
},
"videooutput.colordepth": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.colorformat": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.colorimetry": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.dynamicrange": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
},
"videooutput.quantizationrange": {
    "alias": "org.rdk.AppGatewayCommon",
    "useComRpc": true
}
```

---

### Step 2: Add New Callsign & Event Constants (`SystemDelegate.h`)

```cpp
// New callsign
#ifndef HDMICECSOURCE_CALLSIGN
#define HDMICECSOURCE_CALLSIGN "org.rdk.HdmiCecSource"
#endif

// New VideoOutput event constants
static constexpr const char* EVENT_ON_VO_RESOLUTION_CHANGED        = "VideoOutput.onResolutionChanged";
static constexpr const char* EVENT_ON_VO_HDCP_CHANGED              = "VideoOutput.onHdcpChanged";
static constexpr const char* EVENT_ON_VO_CEC_ACTIVE_STATE_CHANGED  = "VideoOutput.onCecActiveStateChanged";
static constexpr const char* EVENT_ON_VO_PORT_CHANGED              = "VideoOutput.onPortChanged";
static constexpr const char* EVENT_ON_VO_REFRESH_RATE_CHANGED      = "VideoOutput.onRefreshRateChanged";
```

---

### Step 3: Implement Delegate Methods (`SystemDelegate.h`)

Each method follows the same pattern: acquire link/interface → invoke Thunder API → transform response → return Firebolt value.

#### 3a. `GetVideoOutputResolution()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputResolution(std::string &result)
{
    result = "{\"width\":0,\"height\":0}";
    auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputResolution: DisplaySettings link unavailable");
        return Core::ERROR_NONE;  // Return safe default per AC11
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getCurrentResolution", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("w") && response.HasLabel("h")) {
        int w = response["w"].Number();
        int h = response["h"].Number();
        result = "{\"width\":" + std::to_string(w) + ",\"height\":" + std::to_string(h) + "}";
    }
    return Core::ERROR_NONE;
}
```

#### 3b. `GetVideoOutputHdcp()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputHdcp(std::string &result)
{
    result = "\"none\"";
    // TODO: TV device detection → return "\"direct\""
    auto link = AcquireLink(HDCPPROFILE_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputHdcp: HdcpProfile link unavailable");
        return Core::ERROR_NONE;
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getHDCPStatus", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("HDCPStatus")) {
        JsonObject status = response["HDCPStatus"].Object();
        bool isConnected = status["isConnected"].Boolean();
        bool isHDCPEnabled = status["isHDCPEnabled"].Boolean();
        if (isConnected && isHDCPEnabled) {
            std::string version = status["currentHDCPVersion"].String();
            if (version == "2.2") result = "\"hdcp2.2\"";
            else if (version == "1.4") result = "\"hdcp1.4\"";
        }
    }
    return Core::ERROR_NONE;
}
```

#### 3c. `GetVideoOutputCecActiveState()` — JSON-RPC (Two-Step)

```cpp
Core::hresult GetVideoOutputCecActiveState(std::string &result)
{
    result = "\"unsupported\"";
    // TODO: TV device detection → return error "Wrong device class"
    auto link = AcquireLink(HDMICECSOURCE_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputCecActiveState: HdmiCecSource link unavailable");
        return Core::ERROR_NONE;
    }
    // Step 1: Check if CEC is enabled
    JsonObject params, enabledResp;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getEnabled", params, enabledResp);
    if (Core::ERROR_NONE != rc || !enabledResp["enabled"].Boolean()) {
        return Core::ERROR_NONE;  // "unsupported"
    }
    // Step 2: Check active source status
    JsonObject statusResp;
    rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getActiveSourceStatus", params, statusResp);
    if (Core::ERROR_NONE == rc) {
        result = statusResp["status"].Boolean() ? "\"active\"" : "\"inactive\"";
    }
    return Core::ERROR_NONE;
}
```

#### 3d. `GetVideoOutputPort()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputPort(std::string &result)
{
    result = "\"none\"";
    auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputPort: DisplaySettings link unavailable");
        return Core::ERROR_NONE;
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getConnectedVideoDisplays", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("connectedVideoDisplays")) {
        auto displays = response["connectedVideoDisplays"].Array();
        if (displays.Length() > 0) {
            std::string portName = displays[0].String();
            std::string lowerPort = portName;
            std::transform(lowerPort.begin(), lowerPort.end(), lowerPort.begin(), ::tolower);
            if (lowerPort.find("hdmi") != std::string::npos) {
                result = "\"hdmi\"";
            } else if (lowerPort.find("internal") != std::string::npos) {
                result = "\"internal\"";
            }
        }
    }
    return Core::ERROR_NONE;
}
```

#### 3e. `GetVideoOutputRefreshRate()` — COM-RPC

```cpp
Core::hresult GetVideoOutputRefreshRate(std::string &result)
{
    result = "0";
    if (nullptr == _shell) return Core::ERROR_UNAVAILABLE;
    auto* displayProps = _shell->QueryInterfaceByCallsign<Exchange::IDisplayProperties>(DISPLAYINFO_CALLSIGN);
    if (nullptr == displayProps) {
        LOGWARN("GetVideoOutputRefreshRate: IDisplayProperties unavailable");
        return Core::ERROR_NONE;
    }
    Exchange::IDisplayProperties::FrameRateType rate;
    if (Core::ERROR_NONE == displayProps->FrameRate(rate)) {
        switch (rate) {
            case Exchange::IDisplayProperties::FRAMERATE_23_976: result = "23.976"; break;
            case Exchange::IDisplayProperties::FRAMERATE_24:     result = "24"; break;
            case Exchange::IDisplayProperties::FRAMERATE_25:     result = "25"; break;
            case Exchange::IDisplayProperties::FRAMERATE_29_97:  result = "29.97"; break;
            case Exchange::IDisplayProperties::FRAMERATE_30:     result = "30"; break;
            case Exchange::IDisplayProperties::FRAMERATE_50:     result = "50"; break;
            case Exchange::IDisplayProperties::FRAMERATE_59_94:  result = "59.94"; break;
            case Exchange::IDisplayProperties::FRAMERATE_60:     result = "60"; break;
            default: result = "0"; break;
        }
    }
    displayProps->Release();
    return Core::ERROR_NONE;
}
```

#### 3f. `GetVideoOutputColorDepth()` — COM-RPC

```cpp
Core::hresult GetVideoOutputColorDepth(std::string &result)
{
    result = "0";
    if (nullptr == _shell) return Core::ERROR_UNAVAILABLE;
    auto* displayProps = _shell->QueryInterfaceByCallsign<Exchange::IDisplayProperties>(DISPLAYINFO_CALLSIGN);
    if (nullptr == displayProps) {
        LOGWARN("GetVideoOutputColorDepth: IDisplayProperties unavailable");
        return Core::ERROR_NONE;
    }
    Exchange::IDisplayProperties::ColourDepthType depth;
    if (Core::ERROR_NONE == displayProps->ColourDepth(depth)) {
        switch (depth) {
            case Exchange::IDisplayProperties::COLORDEPTH_8_BIT:  result = "8"; break;
            case Exchange::IDisplayProperties::COLORDEPTH_10_BIT: result = "10"; break;
            case Exchange::IDisplayProperties::COLORDEPTH_12_BIT: result = "12"; break;
            default: result = "0"; break;
        }
    }
    displayProps->Release();
    return Core::ERROR_NONE;
}
```

#### 3g. `GetVideoOutputColorFormat()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputColorFormat(std::string &result)
{
    result = "\"none\"";
    auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputColorFormat: DisplayInfo link unavailable");
        return Core::ERROR_NONE;
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "colorspace", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("cs")) {
        std::string cs = response["cs"].String();
        if (cs == "FormatYcbcr420")      result = "\"ycbcr420\"";
        else if (cs == "FormatYcbcr422") result = "\"ycbcr422\"";
        else if (cs == "FormatYcbcr444") result = "\"ycbcr444\"";
        else if (cs == "FormatRgb444")   result = "\"rgb444\"";
    }
    return Core::ERROR_NONE;
}
```

#### 3h. `GetVideoOutputColorimetry()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputColorimetry(std::string &result)
{
    result = "\"none\"";
    auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputColorimetry: DisplayInfo link unavailable");
        return Core::ERROR_NONE;
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getCurrentColorimetry", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("colorimetry")) {
        std::string c = response["colorimetry"].String();
        if (c == "ColorimetryBt709")                result = "\"bt709\"";
        else if (c == "ColorimetrySmpte170M")       result = "\"smpte170m\"";
        else if (c == "ColorimetryXvycc709")        result = "\"xvycc709\"";
        else if (c == "ColorimetryXvycc601")        result = "\"xvycc601\"";
        else if (c == "ColorimetryBt2020rgbYcbcr")  result = "\"bt2020rgb\"";
        else if (c == "ColorimetryBt2020yccbcbrc")  result = "\"bt2020ycc\"";
        else if (c == "ColorimetryOprgb")           result = "\"oprgb\"";
    }
    return Core::ERROR_NONE;
}
```

#### 3i. `GetVideoOutputDynamicRange()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputDynamicRange(std::string &result)
{
    result = "\"none\"";
    auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputDynamicRange: DisplaySettings link unavailable");
        return Core::ERROR_NONE;
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "getVideoFormat", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("currentVideoFormat")) {
        std::string fmt = response["currentVideoFormat"].String();
        if (fmt == "HDR10")          result = "\"hdr10\"";
        else if (fmt == "HDR10PLUS") result = "\"hdr10plus\"";
        else if (fmt == "DV")        result = "\"dolbyVision\"";
        else if (fmt == "HLG")       result = "\"hlg\"";
        else if (fmt == "SDR")       result = "\"sdr\"";
    }
    return Core::ERROR_NONE;
}
```

#### 3j. `GetVideoOutputQuantizationRange()` — JSON-RPC

```cpp
Core::hresult GetVideoOutputQuantizationRange(std::string &result)
{
    result = "\"none\"";
    auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
    if (nullptr == link) {
        LOGERR("GetVideoOutputQuantizationRange: DisplayInfo link unavailable");
        return Core::ERROR_NONE;
    }
    JsonObject params, response;
    uint32_t rc = link->Invoke<JsonObject, JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "quantizationrange", params, response);
    if (Core::ERROR_NONE == rc && response.HasLabel("qr")) {
        std::string qr = response["qr"].String();
        if (qr == "QuantizationrangeLimited")    result = "\"limited\"";
        else if (qr == "QuantizationrangeFull")  result = "\"full\"";
    }
    return Core::ERROR_NONE;
}
```

---

### Step 4: Wire Methods to ComRPC Dispatch (`AppGatewayCommonImplementation.cpp`)

Add handler dispatch for each new `videooutput.*` method in the existing ComRPC handler map:

```cpp
{"videooutput.resolution",       [this](std::string &r) { return _systemDelegate->GetVideoOutputResolution(r); }},
{"videooutput.hdcp",             [this](std::string &r) { return _systemDelegate->GetVideoOutputHdcp(r); }},
{"videooutput.cecactivestate",   [this](std::string &r) { return _systemDelegate->GetVideoOutputCecActiveState(r); }},
{"videooutput.port",             [this](std::string &r) { return _systemDelegate->GetVideoOutputPort(r); }},
{"videooutput.refreshrate",      [this](std::string &r) { return _systemDelegate->GetVideoOutputRefreshRate(r); }},
{"videooutput.colordepth",       [this](std::string &r) { return _systemDelegate->GetVideoOutputColorDepth(r); }},
{"videooutput.colorformat",      [this](std::string &r) { return _systemDelegate->GetVideoOutputColorFormat(r); }},
{"videooutput.colorimetry",      [this](std::string &r) { return _systemDelegate->GetVideoOutputColorimetry(r); }},
{"videooutput.dynamicrange",     [this](std::string &r) { return _systemDelegate->GetVideoOutputDynamicRange(r); }},
{"videooutput.quantizationrange",[this](std::string &r) { return _systemDelegate->GetVideoOutputQuantizationRange(r); }},
```

---

### Step 5: Add Event Subscriptions

#### 5a. Extend Existing Subscriptions

The following Thunder notifications already have subscriptions. Extend the existing handlers to **also** emit VideoOutput events:

| Existing Thunder Notification | Existing Firebolt Event | **NEW** Additional Firebolt Event |
|------------------------------|------------------------|----------------------------------|
| `resolutionChanged` (DisplaySettings) | `Device.onScreenResolutionChanged` | `VideoOutput.onResolutionChanged` |
| `onDisplayConnectionChanged` (HdcpProfile) | `Device.onHdcpChanged` | `VideoOutput.onHdcpChanged` |

In the existing `resolutionChanged` handler:
```cpp
// ...existing emit for Device.onScreenResolutionChanged...
// NEW: Also emit VideoOutput.onResolutionChanged with {width, height}
EmitEvent(EVENT_ON_VO_RESOLUTION_CHANGED, resolutionPayload);
```

In the existing `onDisplayConnectionChanged` handler:
```cpp
// ...existing emit for Device.onHdcpChanged...
// NEW: Also emit VideoOutput.onHdcpChanged with mapped HDCP value
EmitEvent(EVENT_ON_VO_HDCP_CHANGED, hdcpPayload);
```

#### 5b. New Subscriptions Required

| Thunder Plugin | Thunder Event | Firebolt Event | Setup Method |
|---------------|--------------|----------------|-------------|
| `org.rdk.HdmiCecSource` | `onActiveSourceStatusUpdated` | `VideoOutput.onCecActiveStateChanged` | `SetupHdmiCecSourceSubscription()` — **NEW** |
| `org.rdk.DisplaySettings` | `connectedVideoDisplaysUpdated` | `VideoOutput.onPortChanged` | Extend existing DisplaySettings subscription |
| `DisplayInfo` | `updated` (POST_RESOLUTION_CHANGE) | `VideoOutput.onRefreshRateChanged` | Extend existing DisplayInfo subscription or **NEW** |

#### 5c. New HdmiCecSource Subscription

```cpp
void SetupHdmiCecSourceSubscription()
{
    auto link = AcquireLink(HDMICECSOURCE_CALLSIGN);
    if (nullptr == link) {
        LOGWARN("SetupHdmiCecSourceSubscription: HdmiCecSource link unavailable");
        return;
    }
    link->Subscribe<JsonObject>(SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS,
        "onActiveSourceStatusUpdated",
        [this](const JsonObject& params) {
            // Re-query cecActiveState and emit event
            std::string cecResult;
            GetVideoOutputCecActiveState(cecResult);
            EmitEvent(EVENT_ON_VO_CEC_ACTIVE_STATE_CHANGED, cecResult);
        });
}
```

---

### Step 6: Write L0 Unit Tests (`Tests/L1Tests/`)

| # | Test Case | What It Validates |
|---|-----------|-------------------|
| 1 | `GetVideoOutputResolution_1080p` | Maps `"1080p"` → `{width:1920, height:1080}` |
| 2 | `GetVideoOutputResolution_2160p` | Maps `"2160p"` → `{width:3840, height:2160}` |
| 3 | `GetVideoOutputResolution_NoDisplay` | Returns `{width:0, height:0}` when link unavailable |
| 4 | `GetVideoOutputHdcp_Hdcp22` | Maps `currentHDCPVersion:"2.2"` → `"hdcp2.2"` |
| 5 | `GetVideoOutputHdcp_Hdcp14` | Maps `currentHDCPVersion:"1.4"` → `"hdcp1.4"` |
| 6 | `GetVideoOutputHdcp_NotConnected` | `isConnected:false` → `"none"` |
| 7 | `GetVideoOutputHdcp_HdcpDisabled` | `isHDCPEnabled:false` → `"none"` |
| 8 | `GetVideoOutputHdcp_TVDevice` | TV device → `"direct"` |
| 9 | `GetVideoOutputCecActiveState_Active` | `getEnabled:true` + `status:true` → `"active"` |
| 10 | `GetVideoOutputCecActiveState_Inactive` | `getEnabled:true` + `status:false` → `"inactive"` |
| 11 | `GetVideoOutputCecActiveState_Unsupported` | `getEnabled:false` → `"unsupported"` |
| 12 | `GetVideoOutputCecActiveState_TVDevice` | TV device → error |
| 13 | `GetVideoOutputPort_HDMI` | `["HDMI0"]` → `"hdmi"` |
| 14 | `GetVideoOutputPort_Internal` | `["INTERNAL0"]` → `"internal"` |
| 15 | `GetVideoOutputPort_NoDisplay` | `[]` → `"none"` |
| 16 | `GetVideoOutputRefreshRate_60Hz` | `FRAMERATE_60` → `60` |
| 17 | `GetVideoOutputRefreshRate_23976` | `FRAMERATE_23_976` → `23.976` |
| 18 | `GetVideoOutputRefreshRate_NoTV` | Unknown → `0` |
| 19 | `GetVideoOutputColorDepth_10bit` | `COLORDEPTH_10_BIT` → `10` |
| 20 | `GetVideoOutputColorDepth_Unknown` | `COLORDEPTH_UNKNOWN` → `0` |
| 21 | `GetVideoOutputColorFormat_Ycbcr420` | `FormatYcbcr420` → `"ycbcr420"` |
| 22 | `GetVideoOutputColorFormat_Rgb444` | `FormatRgb444` → `"rgb444"` |
| 23 | `GetVideoOutputColorFormat_Unknown` | `FormatUnknown` → `"none"` |
| 24 | `GetVideoOutputColorimetry_Bt709` | `ColorimetryBt709` → `"bt709"` |
| 25 | `GetVideoOutputColorimetry_Bt2020rgb` | `ColorimetryBt2020rgbYcbcr` → `"bt2020rgb"` |
| 26 | `GetVideoOutputColorimetry_Unknown` | `ColorimetryUnknown` → `"none"` |
| 27 | `GetVideoOutputDynamicRange_HDR10` | `"HDR10"` → `"hdr10"` |
| 28 | `GetVideoOutputDynamicRange_DV` | `"DV"` → `"dolbyVision"` |
| 29 | `GetVideoOutputDynamicRange_SDR` | `"SDR"` → `"sdr"` |
| 30 | `GetVideoOutputDynamicRange_None` | `"NONE"` → `"none"` |
| 31 | `GetVideoOutputQuantizationRange_Limited` | `QuantizationrangeLimited` → `"limited"` |
| 32 | `GetVideoOutputQuantizationRange_Full` | `QuantizationrangeFull` → `"full"` |
| 33 | `GetVideoOutputQuantizationRange_Unknown` | `QuantizationrangeUnknown` → `"none"` |
| 34 | `OnResolutionChanged_EventEmitted` | Event dispatched on `resolutionChanged` |
| 35 | `OnHdcpChanged_EventEmitted` | Event dispatched on `onDisplayConnectionChanged` |
| 36 | `OnCecActiveStateChanged_EventEmitted` | Event dispatched on `onActiveSourceStatusUpdated` |
| 37 | `OnPortChanged_EventEmitted` | Event dispatched on `connectedVideoDisplaysUpdated` |
| 38 | `OnRefreshRateChanged_EventEmitted` | Event dispatched on `updated` (POST_RESOLUTION_CHANGE) |

---

## 8. Architecture Flow

```
┌─────────────┐      ┌──────────────┐      ┌────────────────────┐      ┌──────────────────┐
│   Firebolt   │      │  AppGateway  │      │  AppGatewayCommon  │      │  Thunder Plugins │
│   App        │      │  (Resolver)  │      │  (SystemDelegate)  │      │                  │
└──────┬──────┘      └──────┬───────┘      └────────┬───────────┘      └────────┬─────────┘
       │                    │                       │                           │
       │ VideoOutput.hdcp   │                       │                           │
       │───────────────────>│                       │                           │
       │                    │ resolve → useComRpc   │                           │
       │                    │──────────────────────>│                           │
       │                    │                       │ getHDCPStatus (JSON-RPC)  │
       │                    │                       │─────────────────────────->│
       │                    │                       │                           │
       │                    │                       │ {HDCPStatus:{...}}        │
       │                    │                       │<──────────────────────────│
       │                    │                       │                           │
       │                    │  "hdcp2.2"            │ Transform → Firebolt     │
       │                    │<──────────────────────│                           │
       │  "hdcp2.2"         │                       │                           │
       │<───────────────────│                       │                           │
       │                    │                       │                           │
       │                    │                       │ onDisplayConnectionChanged│
       │                    │                       │<──────────────────────────│
       │                    │                       │                           │
       │                    │                       │ Re-query + transform      │
       │  onHdcpChanged     │  Dispatch event       │                           │
       │<───────────────────│<──────────────────────│                           │
```

---

## 9. Files to Modify — Summary

| File | Changes |
|------|---------|
| `AppGateway/resolutions/resolution.base.json` | Add 15 `videooutput.*` resolution entries |
| `AppGatewayCommon/delegate/SystemDelegate.h` | Add 10 `GetVideoOutput*()` methods + callsign + event constants + new subscriptions |
| `AppGatewayCommon/AppGatewayCommonImplementation.cpp` | Wire 10 new methods to ComRPC handler dispatch map |
| `Tests/L1Tests/` | 38 L0 unit tests covering all APIs + events |

---

## 10. Blocking Dependencies

| Dependency | Status | Impact |
|-----------|--------|--------|
| `VideoOutput.colorimetry` (AC8) — requires `DisplayInfo.getCurrentColorimetry` Thunder API | ✅ Merged (08/05) | Can proceed |
| `org.rdk.HdmiCecSource` plugin availability | ✅ Available | Needed for `cecActiveState` |
| `DisplayInfo` COM-RPC interfaces (`IDisplayProperties`, `IConnectionProperties`) | ✅ Available | Needed for refreshRate, colorDepth |

---

## 11. Coding Guidelines Compliance

| Guideline | How We Comply |
|-----------|--------------|
| **YODA notation** | All `nullptr` checks use `nullptr == ptr` form |
| **Critical logging** | All errors logged with `LOGERR()` before returning error codes |
| **Recoverable errors** | Optional/missing data logged with `LOGWARN()` |
| **COM-RPC for inter-plugin** | All new queries use `QueryInterfaceByCallsign` or `AcquireLink` |
| **On-demand interface acquisition** | COM-RPC interfaces acquired per-call and released immediately |
| **No public member variables** | All new state is private with accessor methods |
| **Safe defaults** | All methods return Firebolt-compliant defaults when Thunder is unavailable (AC11) |

---

## 12. Summary

- **10 new Firebolt VideoOutput APIs** to implement
- **15 resolution entries** in `resolution.base.json` (10 methods + 5 events)
- **Follows existing patterns** in `SystemDelegate.h` — minimal architectural changes
- **Mixed transport**: JSON-RPC for DisplaySettings/HdcpProfile/HdmiCecSource, COM-RPC for DisplayInfo
- **5 APIs with event subscriptions**, 5 poll-only
- **38 unit tests** covering positive, negative, and edge cases
- **Zero app-side changes** — apps call Firebolt VideoOutput APIs, AppGateway handles everything
- **Safe error handling** — all APIs return defaults when Thunder plugins are unavailable
