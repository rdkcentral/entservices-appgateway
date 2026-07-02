# Build System & Configuration Reference

> **Root build file:** `CMakeLists.txt`
> **Build system:** CMake (minimum version 3.3)
> **Framework:** WPEFramework (Thunder)

---

## 1. High-Level Purpose & Architecture

This document covers how all four Thunder plugins in `entservices-appgateway` are configured, compiled, and installed using CMake.

### Repository Build Structure

```
CMakeLists.txt              ← Root; conditionally includes each plugin subdirectory
├── AppGateway/CMakeLists.txt
├── AppGatewayCommon/CMakeLists.txt
├── AppNotifications/CMakeLists.txt
├── AppActions/CMakeLists.txt
├── Tests/L1Tests/CMakeLists.txt  (included when RDK_SERVICES_L1_TEST=ON)
├── Tests/L2Tests/CMakeLists.txt  (included when RDK_SERVICE_L2_TEST=ON)
└── cmake/                  ← Helper CMake scripts (CmakeHelperFunctions, etc.)
```

---

## 2. Root `CMakeLists.txt`

**File:** `CMakeLists.txt`

### Key Operations

1. Finds the `WPEFramework` package.
2. Appends `cmake/` to `CMAKE_MODULE_PATH` for helper scripts.
3. Sets `PRODUCT_CONFIG_DIR` to `/etc/entservices`.
4. Derives `STORAGE_DIRECTORY` from `${NAMESPACE}` (lowercased) — used for install paths.
5. Includes test subdirectories based on build flags.
6. Includes plugin subdirectories based on `PLUGIN_*` options.

### Plugin Inclusion Guards

```cmake
# CMakeLists.txt (excerpt)
if(PLUGIN_APPGATEWAY)
    add_subdirectory(AppGateway)
endif()

if(PLUGIN_APPNOTIFICATIONS)
    add_subdirectory(AppNotifications)
endif()

if(PLUGIN_APPGATEWAYCOMMON)
    add_subdirectory(AppGatewayCommon)
endif()

if(PLUGIN_APPACTIONS)
    add_subdirectory(AppActions)
endif()
```

---

## 3. Global Build Flags

These flags are set at the root level and affect all plugins.

| CMake Flag | Type | Default | Effect |
|---|---|---|---|
| `BUILD_ENABLE_TELEMETRY_LOGGING` | `BOOL` | OFF | Defines `ENABLE_TELEMETRY_LOGGING`; links `telemetry_msgsender` in AppGateway |
| `DISABLE_SECURITY_TOKEN` | `BOOL` | OFF | Defines `DISABLE_SECURITY_TOKEN`; bypasses auth token checks in AppGateway |
| `USE_THUNDER_R4` | `BOOL` | OFF | Defines `USE_THUNDER_R4`; switches to Thunder R4 API usage |
| `RDK_SERVICES_L1_TEST` | `BOOL` | OFF | Includes `Tests/L1Tests/` |
| `RDK_SERVICE_L2_TEST` | `BOOL` | OFF | Includes `Tests/L2Tests/` |

---

## 4. Per-Plugin CMake Configuration

### 4.1 AppGateway

**File:** `AppGateway/CMakeLists.txt`

**Output:** `libWPEFrameworkAppGateway.so`  
**Version defines:** `APPGATEWAY_MAJOR_VERSION`, `APPGATEWAY_MINOR_VERSION`, `APPGATEWAY_PATCH_VERSION` (1.0.0)

**Sources compiled:**
```
AppGateway.cpp
AppGatewayImplementation.cpp
AppGatewayResponderImplementation.cpp
AppGatewayTelemetry.cpp
Resolver.cpp
Module.cpp
```

**Include paths:**
- `../helpers` — shared utility headers (`WsManager.h`, `ContextUtils.h`, etc.)
- `./resolutions` — resolution config directory

**Plugin-specific flags:**

| Flag | Default | Effect |
|---|---|---|
| `PLUGIN_APPGATEWAY_AUTOSTART` | `"false"` | Thunder config: autostart |
| `PLUGIN_APPGATEWAY_STARTUPORDER` | `""` | Thunder plugin startup order |
| `BUILD_CONFIG_PATH` | — | Compile-time path to build-specific resolution config |
| `VENDOR_CONFIG_PATH` | — | Compile-time path to vendor resolution config overlay |
| `ENABLE_APP_GATEWAY_AUTOMATION` | OFF | Enables automation test hooks; optionally sets `AUTOMATION_APP_ID` and `APP_GATEWAY_ENHANCED_LOGGING_INDICATOR` |
| `BUILD_ENABLE_TELEMETRY_LOGGING` | OFF | Links `telemetry_msgsender` library |

**Conditional link:**
```cmake
if(BUILD_ENABLE_TELEMETRY_LOGGING)
    find_library(TELEMETRY_MSGSENDER_LIB telemetry_msgsender)
    target_link_libraries(${MODULE_NAME} PRIVATE ${TELEMETRY_MSGSENDER_LIB})
endif()
```

**Install targets:**
- Library → `lib/${STORAGE_DIRECTORY}/plugins`
- Resolution file → `/etc/app-gateway/resolution.base.json`

**Config generation:** `write_config(AppGateway)` via `CmakeHelperFunctions`

---

### 4.2 AppGatewayCommon

**File:** `AppGatewayCommon/CMakeLists.txt`

**Output:** `libWPEFrameworkAppGatewayCommon.so`  
**Version defines:** `APPGATEWAYCOMMON_MAJOR_VERSION`, `APPGATEWAYCOMMON_MINOR_VERSION`, `APPGATEWAYCOMMON_PATCH_VERSION` (1.0.0)

**Sources compiled:**
```
AppGatewayCommon.cpp
Module.cpp
```

**Include paths:** `../helpers`

**Plugin-specific flags:**

| Flag | Default | Effect |
|---|---|---|
| `PLUGIN_APPGATEWAYCOMMON_AUTOSTART` | `"false"` | Thunder config: autostart |
| `PLUGIN_APPGATEWAYCOMMON_STARTUPORDER` | `""` | Plugin startup order |
| `ENABLE_FIREBOLT_TEXTTRACK` | OFF | Enables text-track Firebolt API support |

**Optional dependency:**
```cmake
find_library(NMPROXY_LIB NAMES ${NAMESPACE}NetworkManagerProxy
    PATHS ${CMAKE_SYSROOT}/usr/lib/wpeframework/proxystubs)
```

**Linked libraries:** `${NAMESPACE}Plugins`, `${NAMESPACE}Definitions`, `uuid`

**Compiler flags:** `-Wall -Werror` (treat all warnings as errors)

**Install target:** `lib/${STORAGE_DIRECTORY}/plugins`

---

### 4.3 AppNotifications

**File:** `AppNotifications/CMakeLists.txt`

**Output:** `libWPEFrameworkAppNotifications.so`  
**Version defines:** `APPNOTIFICATIONS_MAJOR_VERSION/MINOR/PATCH` (1.0.0)

**Sources compiled:**
```
AppNotificationsImplementation.cpp
AppNotifications.cpp
Module.cpp
```

**Include paths:** `../helpers`

**Plugin-specific flags:**

| Flag | Default | Effect |
|---|---|---|
| `PLUGIN_APPNOTIFICATIONS_AUTOSTART` | `"false"` | Thunder autostart |
| `PLUGIN_APPNOTIFICATIONS_STARTUPORDER` | `""` | Plugin startup order |

**Linked libraries:** `${NAMESPACE}Plugins`, `${NAMESPACE}Definitions`

**Compiler flags:** `-Wall -Werror`

**Install target:** `lib/${STORAGE_DIRECTORY}/plugins`

---

### 4.4 AppActions

**File:** `AppActions/CMakeLists.txt`

**Output:** Two separate libraries:

| Library | Sources | Install Path |
|---|---|---|
| `libWPEFrameworkAppActions.so` | `AppActions.cpp`, `AppActions.h`, `Module.cpp` | `${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins` |
| `libWPEFrameworkAppActionsImplementation.so` | `AppActionsImplementation.cpp`, `AppActionsImplementation.h`, `Module.cpp` | `lib/${STORAGE_DIRECTORY}/plugins` |

**Version defines:** `APPACTIONS_MAJOR_VERSION/MINOR/PATCH` (1.0.0)

**Plugin-specific flags:**

| Flag | Default | Effect |
|---|---|---|
| `PLUGIN_APPACTIONS_AUTOSTART` | `"false"` | Thunder autostart |
| `PLUGIN_APPACTIONS_STARTUPORDER` | `""` | Plugin startup order |

**Linked libraries (both):** `${NAMESPACE}Plugins`, `${NAMESPACE}Definitions`, `uuid`

**Compiler flags:** `-Wall -Werror`

---

## 5. Thunder Configuration Files

Each plugin ships two configuration artifacts generated by CMake:

### `.config` — CMake Configuration Script

**Purpose:** Read by `write_config()` helper to generate the Thunder JSON config file.

**Example (`AppGateway/AppGateway.config`):**
```cmake
set(autostart ${PLUGIN_APPGATEWAY_AUTOSTART})
set(preconditions Platform)
set(callsign "org.rdk.AppGateway")
```

### `.conf.in` — Thunder Config Template

**Purpose:** Template file (processed by CMake) that generates the runtime Thunder plugin configuration JSON installed on the device.

**Key fields set by `.config`:**

| Field | Description |
|---|---|
| `callsign` | Thunder plugin identifier string |
| `autostart` | Whether Thunder starts the plugin automatically |
| `preconditions` | Dependencies that must be satisfied before start |
| `startuporder` | Numeric startup ordering hint |

---

## 6. Shared Helpers (`helpers/`)

The `helpers/` directory contains header-only (or lightly implemented) utilities shared across all plugins. They are included via `target_include_directories(${MODULE_NAME} PRIVATE ../helpers)`.

### Key Helper Files

| File | Purpose |
|---|---|
| `WsManager.h` | `WebSocketConnectionManager` — manages the Thunder WebSocket channel used by `AppGatewayResponderImplementation` |
| `ContextUtils.h` | `ContextUtils` static helper — converts between `AppNotificationContext` and `GatewayContext`; `IsOriginGateway()` origin check |
| `UtilsLogging.h` | `LOGINFO()`, `LOGERR()`, `LOGDBG()` macros wrapping Thunder trace |
| `UtilsController.h` | Thunder controller plugin interaction helpers |
| `UtilsCallsign.h` | Callsign parsing/formatting helpers |
| `UtilsJsonRpc.h` | JSON-RPC formatting / parsing utilities |
| `UtilsJsonrpcDirectLink.h` | Direct JSON-RPC link utilities |
| `UtilsFirebolt.h` | Firebolt-specific utilities |
| `StringUtils.h` | String manipulation helpers |
| `ObjectUtils.h` | JSON object manipulation helpers |
| `PluginInterfaceBuilder.h` | Helpers for constructing plugin interface maps |
| `AppGatewayTelemetryMarkers.h` | Telemetry marker constant definitions (included by `AppGatewayTelemetry.h`) |
| `UtilsAppGatewayTelemetry.h` | Telemetry helper utilities |
| `tptimer.h` | Timer utility wrapper |
| `WebSocketLink.h` | Low-level WebSocket link abstraction used by `WsManager.h` |
| `StreamJSONOneShot.h` | Single-shot JSON stream wrapper (works around a Thunder JSON bug) |
| `BaseEventDelegate.h` | Base class for event delegate patterns |
| `cSettings.h` | Settings helper utilities |

---

## 7. `resolution.base.json` — Method Resolution Table

**File:** `AppGateway/resolutions/resolution.base.json`  
**Installed to:** `/etc/app-gateway/resolution.base.json`

This JSON file is the **central routing table** of the AppGateway system. It maps every supported Firebolt API method name to a Thunder routing target.

### Schema

```json
{
  "resolutions": {
    "<firebolt.method.name>": {
      "alias":           "<Thunder callsign or callsign.method>",
      "useComRpc":       true | false,
      "permissionGroup": "<optional permission group string>",
      "event":           "<optional event name>",
      "includeContext":  true | false,
      "additionalContext": { ... },
      "versionedEvent":  true | false
    }
  }
}
```

### Field Reference

| Field | Type | Required | Description |
|---|---|---|---|
| `alias` | string | Yes | Target Thunder callsign. For JSON-RPC routing use `callsign.method`. For COM-RPC use just the callsign (e.g., `org.rdk.AppGatewayCommon`). |
| `useComRpc` | bool | No (default: `false`) | If `true`, dispatched via COM-RPC to `IAppGatewayRequestHandler`. If `false`, dispatched via direct JSON-RPC to the target callsign. |
| `permissionGroup` | string | No | If set, `CheckPermissionGroup(appId, permissionGroup)` is called before dispatch. Unauthorized calls are rejected. |
| `event` | string | No | Event name for subscription-type methods. Present only in entries that map to an event. |
| `includeContext` | bool | No (default: `false`) | If `true`, `GatewayContext` is injected into the request params before dispatch. |
| `additionalContext` | object | No | Static JSON object merged into the injected context. |
| `versionedEvent` | bool | No (default: `false`) | If `true`, appends version suffix (e.g., `.v8`) to the event name. |

### Representative Entries

```json
{
  "resolutions": {
    "device.name": {
      "alias": "org.rdk.AppGatewayCommon",
      "useComRpc": true
    },
    "device.setName": {
      "alias": "org.rdk.AppGatewayCommon",
      "useComRpc": true,
      "permissionGroup": "org.rdk.permission.group.enhanced"
    },
    "Localization.addAdditionalInfo": {
      "alias": "org.rdk.AppGatewayCommon",
      "useComRpc": true
    },
    "localization.timeZone": {
      "alias": "org.rdk.AppGatewayCommon",
      "useComRpc": true
    },
    "voiceguidance.setEnabled": {
      "alias": "org.rdk.AppGatewayCommon",
      "useComRpc": true,
      "permissionGroup": "org.rdk.permission.group.enhanced"
    }
  }
}
```

> **Note:** The file at `AppGateway/resolutions/resolution.base.json` contains 536 lines (~100+ method entries). The examples above are representative; consult the actual file for the full list.

### Adding a New Method

1. Add the JSON entry in `resolution.base.json`.
2. If `useComRpc: true` — ensure the method key matches a handler in `AppGatewayCommon`'s `handlers` map.
3. If `useComRpc: false` — ensure the `alias` points to a valid Thunder plugin callsign/method.
4. Set `permissionGroup` if the method requires elevated permissions.
5. Set `event` and optionally `versionedEvent` if the entry is a subscription method.

---

## 8. Complete Build Example

```bash
mkdir build && cd build

cmake .. \
  -DNAMESPACE=WPEFramework \
  -DPLUGIN_APPGATEWAY=ON \
  -DPLUGIN_APPGATEWAYCOMMON=ON \
  -DPLUGIN_APPNOTIFICATIONS=ON \
  -DPLUGIN_APPACTIONS=ON \
  -DBUILD_ENABLE_TELEMETRY_LOGGING=OFF \
  -DDISABLE_SECURITY_TOKEN=OFF \
  -DUSE_THUNDER_R4=OFF \
  -DRDK_SERVICES_L1_TEST=ON

make -j$(nproc)
make install
```

For T2 telemetry support:
```bash
cmake .. \
  -DBUILD_ENABLE_TELEMETRY_LOGGING=ON \
  # ... (telemetry_msgsender must be present in sysroot)
```

---

## 9. C++ Standard & Compiler Requirements

All plugins are built with:
- **C++ standard:** C++11 (`CXX_STANDARD 11`, `CXX_STANDARD_REQUIRED YES`)
- **Warnings:** `-Wall -Werror` (AppGatewayCommon, AppNotifications, AppActions)
- **Link check:** `-Wl,-z,defs` (all plugins) — fails build on any unresolved symbol

---

*Back to [README.md](../README.md)*
