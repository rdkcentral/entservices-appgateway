# entservices-appgateway

This repository contains the Firebolt Gateway implementation as Thunder plugins for RDK (Reference Design Kit) devices. It provides a unified interface for applications to access device capabilities, system information, and user settings through a standardized API gateway pattern. This implementation deprecates the ripple gateway in the RDK Apps Manager Framework.

## Overview

The App Gateway acts as a bridge between Firebolt applications and the underlying Thunder framework services, providing:
- **Request Resolution**: Routes Firebolt API calls to appropriate Thunder plugins
- **Common Device APIs**: Unified access to device information, settings, and capabilities
- **Event Notifications**: Centralized event handling and distribution to applications
- **Permission Management**: Enforces permission-based access control for sensitive operations

## Architecture

The repository consists of three main Thunder plugins that work together:

### 1. AppGateway Plugin
The core routing and resolution plugin that:
- Resolves Firebolt API method calls to Thunder plugin endpoints
- Uses a JSON-based resolution configuration to map API methods to plugin implementations
- Handles request/response transformation between Firebolt and Thunder formats
- Supports both JSON-RPC and COM-RPC communication patterns
- Enforces permission groups for privileged operations

**Key Components:**
- `AppGatewayImplementation`: Core implementation handling request resolution
- `Resolver`: Loads and manages API method to plugin mappings from configuration
- `AppGatewayResponderImplementation`: Handles asynchronous responses and callbacks

### 2. AppGatewayCommon Plugin
Provides common device and user setting APIs:
- **Device Information**: Make, model, name, SKU, firmware version
- **Localization**: Country code, timezone, locale, language preferences
- **Network Status**: Internet connectivity information
- **User Settings**: Accessibility features (voice guidance, captions, audio descriptions), display settings (resolution, HDR, HDCP), audio preferences
- **Event Notifications**: System and application events

**Key Features:**
- Integrates with Thunder's System, Network, and DisplaySettings plugins
- Delegates platform-specific implementations through `SettingsDelegate`
- Supports both getter and setter operations with permission control

### 3. AppNotifications Plugin
Manages event notifications and subscriptions:
- Provides centralized event emission for application events
- Manages event listener registration and deregistration
- Supports event filtering and routing to subscribed applications
- Handles event context and metadata propagation

## Components and Dependencies

```
entservices-appgateway/
├── AppGateway/              # Core gateway plugin for request resolution
├── AppGatewayCommon/        # Common device APIs implementation
├── AppNotifications/        # Event notification management
├── Tests/                   # L1 and L2 test infrastructure
│   ├── L1Tests/            # Unit tests
│   └── L2Tests/            # Integration tests
├── helpers/                 # Shared utility libraries
├── cmake/                   # CMake build configuration
└── build_dependencies.sh    # Dependency build script
```

### Dependencies
- **Thunder Framework** (R4.4+): Core plugin framework
- **ThunderTools**: Build and development tools
- **entservices-apis**: Interface definitions
- **entservices-testframework**: Test infrastructure and mocks

## Building

### Prerequisites
```bash
sudo apt install -y \
    libsqlite3-dev libcurl4-openssl-dev \
    libsystemd-dev libboost-all-dev \
    libwebsocketpp-dev meson \
    cmake ninja-build
```

### Build Instructions

1. **Clone the repository:**
```bash
git clone https://github.com/rdkcentral/entservices-appgateway.git
cd entservices-appgateway
```

2. **Build dependencies** (Thunder, ThunderTools, APIs):
```bash
./build_dependencies.sh
```

3. **Build the plugins:**
```bash
mkdir build && cd build
cmake -G Ninja .. \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DPLUGIN_APPGATEWAY=ON \
    -DPLUGIN_APPGATEWAYCOMMON=ON \
    -DPLUGIN_APPNOTIFICATIONS=ON
ninja
sudo ninja install
```

### Build Options
- `PLUGIN_APPGATEWAY`: Build AppGateway plugin (default: OFF)
- `PLUGIN_APPGATEWAYCOMMON`: Build AppGatewayCommon plugin (default: OFF)
- `PLUGIN_APPNOTIFICATIONS`: Build AppNotifications plugin (default: OFF)
- `DISABLE_SECURITY_TOKEN`: Disable security token validation (default: OFF)
- `RDK_SERVICES_L1_TEST`: Build L1 unit tests (default: OFF)
- `RDK_SERVICE_L2_TEST`: Build L2 integration tests (default: OFF)

## Configuration

### Resolution Configuration
The AppGateway plugin uses a JSON configuration file to map Firebolt API methods to Thunder plugin endpoints:

**Location:** `/etc/app-gateway/resolution.base.json`

**Example:**
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
        }
    }
}
```

### Plugin Configuration
Each plugin has its own Thunder configuration file:
- `AppGateway.config`: Gateway plugin configuration
- `AppGatewayCommon.config`: Common APIs plugin configuration
- `AppNotifications.config`: Notifications plugin configuration

## Testing

### Running Tests Locally

**L1 Unit Tests:**
```bash
cmake .. -DRDK_SERVICES_L1_TEST=ON
ninja
ctest
```

**L2 Integration Tests:**
```bash
cmake .. -DRDK_SERVICE_L2_TEST=ON
ninja
ctest
```

### Using GitHub Actions (act)
```bash
# Download act tool
curl -SL https://raw.githubusercontent.com/nektos/act/master/install.sh | bash

# Run all tests
./bin/act -W .github/workflows/tests-trigger.yml -s GITHUB_TOKEN=<your-token>
```

For detailed testing instructions, see [Tests/README.md](Tests/README.md).

## API Examples

### Device Information
```javascript
// Get device name
const name = await device.name();

// Set device name (requires enhanced permissions)
await device.setName("Living Room TV");
```

### Localization
```javascript
// Get current locale
const locale = await localization.locale();

// Set locale
await localization.setLocale("en-US");
```

### User Settings
```javascript
// Get voice guidance settings
const voiceGuidance = await accessibility.voiceGuidance();

// Enable captions
await accessibility.closedCaptions(true);
```

## Development Workflow

1. **Code Changes**: Make changes to plugin implementations
2. **Build**: Rebuild affected plugins
3. **Test**: Run L1 unit tests for changed components
4. **Integration Test**: Run L2 tests to verify plugin interactions
5. **Documentation**: Update relevant documentation

## Project Structure

- **Interfaces**: COM-RPC interfaces defined in `entservices-apis` repository
- **Implementation**: Plugin implementations in respective directories
- **Helpers**: Shared utilities for logging, JSON handling, HTTP, WebSocket
- **Configuration**: JSON-based resolution and plugin configurations
- **Tests**: Comprehensive test coverage with mocks and fixtures

## Contributing

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on contributing to this project.

## License

This project is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## Documentation

- **[DOCS.md](DOCS.md)** - Complete documentation index and navigation guide
- [CHANGELOG.md](CHANGELOG.md) - Release notes and version history
- [Tests/README.md](Tests/README.md) - Detailed testing documentation
- [ARCHITECTURE.md](ARCHITECTURE.md) - Technical architecture and design details
- [DEVELOPMENT.md](DEVELOPMENT.md) - Developer setup and workflow guide
- [API.md](API.md) - Complete API reference with examples

## Support

For issues, questions, or contributions, please use the GitHub issue tracker.
