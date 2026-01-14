# Development Guide

This guide provides detailed information for developers working on the entservices-appgateway project.

## Table of Contents
- [Development Environment Setup](#development-environment-setup)
- [Building the Project](#building-the-project)
- [Running and Debugging](#running-and-debugging)
- [Testing](#testing)
- [Code Style and Standards](#code-style-and-standards)
- [Common Development Tasks](#common-development-tasks)
- [Troubleshooting](#troubleshooting)

## Development Environment Setup

### System Requirements
- **OS**: Ubuntu 20.04 or later (or compatible Linux distribution)
- **RAM**: 4GB minimum, 8GB recommended
- **Disk**: 10GB free space for dependencies and builds
- **Compiler**: GCC 9+ or Clang 10+

### Required Tools
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    python3 \
    python3-pip \
    pkg-config \
    valgrind \
    lcov \
    clang \
    protobuf-compiler-grpc
```

### Required Libraries
```bash
sudo apt install -y \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    libsystemd-dev \
    libboost-all-dev \
    libwebsocketpp-dev \
    libgrpc-dev \
    libgrpc++-dev \
    libunwind-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libcunit1 \
    libcunit1-dev
```

### Python Dependencies
```bash
pip3 install jsonref
```

### Additional Build Tools
```bash
# Install Meson (for trower-base64 dependency)
sudo apt install -y meson

# Install act for local GitHub Actions testing
curl -SL https://raw.githubusercontent.com/nektos/act/master/install.sh | bash
```

## Building the Project

### Quick Start Build

1. **Clone the repository:**
```bash
git clone https://github.com/rdkcentral/entservices-appgateway.git
cd entservices-appgateway
```

2. **Build Thunder and dependencies:**
```bash
./build_dependencies.sh
```
This script will:
- Clone Thunder (R4.4.1), ThunderTools (R4.4.3), and entservices-apis
- Apply necessary patches
- Build and install all dependencies to `install/usr`

3. **Build the plugins:**
```bash
mkdir build && cd build
cmake -G Ninja .. \
    -DCMAKE_INSTALL_PREFIX="${PWD}/../install/usr" \
    -DCMAKE_MODULE_PATH="${PWD}/../install/tools/cmake" \
    -DPLUGIN_APPGATEWAY=ON \
    -DPLUGIN_APPGATEWAYCOMMON=ON \
    -DPLUGIN_APPNOTIFICATIONS=ON \
    -DBUILD_TYPE=Debug
ninja
ninja install
```

### Build Configuration Options

#### Plugin Selection
- `-DPLUGIN_APPGATEWAY=ON|OFF` - Build AppGateway plugin
- `-DPLUGIN_APPGATEWAYCOMMON=ON|OFF` - Build AppGatewayCommon plugin
- `-DPLUGIN_APPNOTIFICATIONS=ON|OFF` - Build AppNotifications plugin

#### Build Types
- `-DBUILD_TYPE=Debug` - Debug build with symbols
- `-DBUILD_TYPE=Release` - Optimized release build
- `-DBUILD_TYPE=RelWithDebInfo` - Release with debug info

#### Testing Options
- `-DRDK_SERVICES_L1_TEST=ON` - Enable L1 unit tests
- `-DRDK_SERVICE_L2_TEST=ON` - Enable L2 integration tests

#### Security Options
- `-DDISABLE_SECURITY_TOKEN=ON` - Disable security token validation (for testing)

#### Installation Paths
- `-DCMAKE_INSTALL_PREFIX=/usr` - System installation path
- `-DPRODUCT_CONFIG_DIR=/etc/entservices` - Configuration directory

### Incremental Builds

After making code changes:
```bash
cd build
ninja
ninja install
```

To rebuild a specific plugin:
```bash
ninja AppGateway
ninja install
```

### Clean Build
```bash
cd build
ninja clean
# Or for a complete clean:
cd ..
rm -rf build
mkdir build && cd build
# Re-run cmake and ninja
```

## Running and Debugging

### Running Thunder with Plugins

1. **Start Thunder framework:**
```bash
# Set library path to include installed plugins
export LD_LIBRARY_PATH="${PWD}/install/usr/lib:$LD_LIBRARY_PATH"

# Run Thunder
${PWD}/install/usr/bin/WPEFramework -c ${PWD}/install/usr/share/WPEFramework -b /tmp
```

2. **Activate plugins via Thunder controller:**
```bash
# Using curl
curl -X POST http://127.0.0.1:9998/Service/Controller/Activate \
    -H "Content-Type: application/json" \
    -d '{"callsign":"AppGateway"}'

curl -X POST http://127.0.0.1:9998/Service/Controller/Activate \
    -H "Content-Type: application/json" \
    -d '{"callsign":"AppGatewayCommon"}'
```

3. **Test API calls:**
```bash
# Get device name
curl -X POST http://127.0.0.1:9998/jsonrpc/AppGateway \
    -H "Content-Type: application/json" \
    -d '{
        "jsonrpc": "2.0",
        "id": 1,
        "method": "device.name"
    }'
```

### Debugging with GDB

**Debug a running Thunder process:**
```bash
# Find Thunder PID
ps aux | grep WPEFramework

# Attach GDB
sudo gdb -p <PID>

# Set breakpoints
(gdb) break AppGateway::Initialize
(gdb) continue
```

**Debug plugin from start:**
```bash
# Build with debug symbols
cmake .. -DBUILD_TYPE=Debug

# Run Thunder under GDB
gdb --args ${PWD}/install/usr/bin/WPEFramework -c ${PWD}/install/usr/share/WPEFramework

# Set breakpoints and run
(gdb) break AppGatewayImplementation.cpp:50
(gdb) run
```

### Debugging with Logging

Enable verbose logging by setting environment variables:
```bash
# Enable Thunder logging
export THUNDER_VERBOSE=ON

# Enable plugin-specific logging (if implemented)
export APPGATEWAY_LOG_LEVEL=DEBUG
```

Check logs:
```bash
# Thunder logs (if systemd service)
journalctl -u wpeframework -f

# Or direct output if running manually
```

### Using Valgrind for Memory Debugging

```bash
# Check for memory leaks
valgrind --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    ${PWD}/install/usr/bin/WPEFramework -c ${PWD}/install/usr/share/WPEFramework
```

## Testing

### L1 Unit Tests

**Build with tests enabled:**
```bash
cmake .. -DRDK_SERVICES_L1_TEST=ON
ninja
```

**Run all L1 tests:**
```bash
ctest --output-on-failure
# Or run specific test executable
./Tests/L1Tests/AppGatewayL1Test
```

**Run specific test suite:**
```bash
./Tests/L1Tests/AppGatewayL1Test --gtest_filter="ResolverTest.*"
```

### L2 Integration Tests

**Build with L2 tests:**
```bash
cmake .. -DRDK_SERVICE_L2_TEST=ON
ninja
```

**Run L2 tests:**
```bash
ctest --output-on-failure
# Or
./Tests/L2Tests/AppGatewayL2Test
```

### Using act for GitHub Actions Locally

**Run all tests:**
```bash
./bin/act -W .github/workflows/tests-trigger.yml \
    -s GITHUB_TOKEN=<your-github-token>
```

**Run specific workflow:**
```bash
./bin/act -W .github/workflows/L1-tests.yml
```

### Code Coverage

**Generate coverage report:**
```bash
# Build with coverage enabled
cmake .. -DBUILD_TYPE=Debug -DRDK_SERVICES_L1_TEST=ON
./cov_build.sh

# View coverage report
firefox coverage/index.html
```

### Writing Tests

**L1 Test Example:**
```cpp
#include <gtest/gtest.h>
#include "AppGateway.h"

TEST(AppGatewayTest, InitializeSuccess) {
    // Setup
    Plugin::AppGateway gateway;
    
    // Exercise
    auto result = gateway.Initialize(mockShell);
    
    // Verify
    EXPECT_EQ(result, "");
}
```

**L2 Test Example:**
```cpp
#include <gtest/gtest.h>
#include "ThunderTestFixture.h"

class AppGatewayL2Test : public ThunderTestFixture {
protected:
    void SetUp() override {
        ActivatePlugin("AppGateway");
    }
};

TEST_F(AppGatewayL2Test, DeviceNameAPI) {
    auto response = CallJsonRpc("AppGateway", "device.name");
    EXPECT_TRUE(response.has_value());
}
```

## Code Style and Standards

### C++ Standards
- **C++14** is the target standard
- Use modern C++ features appropriately
- Avoid raw pointers; prefer smart pointers

### Coding Conventions

**Naming:**
- Classes: `PascalCase` (e.g., `AppGateway`, `Resolver`)
- Methods: `PascalCase` (e.g., `Initialize`, `HandleRequest`)
- Variables: `camelCase` with `m` prefix for members (e.g., `mService`, `mResolver`)
- Constants: `UPPER_CASE` (e.g., `MAX_RETRIES`)

**Formatting:**
Use the provided `.clang-format` file:
```bash
clang-format -i <file>.cpp
```

**Header Guards:**
```cpp
#pragma once
```

### Thunder Plugin Patterns

**Plugin Structure:**
```cpp
class MyPlugin : public PluginHost::IPlugin {
public:
    MyPlugin();
    virtual ~MyPlugin();
    
    virtual const string Initialize(PluginHost::IShell* service) override;
    virtual void Deinitialize(PluginHost::IShell* service) override;
    virtual string Information() const override { return {}; }
    
    BEGIN_INTERFACE_MAP(MyPlugin)
        INTERFACE_ENTRY(PluginHost::IPlugin)
    END_INTERFACE_MAP
    
private:
    PluginHost::IShell* mService;
};
```

**Error Handling:**
```cpp
Core::hresult MyMethod(string& result) {
    if (error_condition) {
        return Core::ERROR_GENERAL;
    }
    result = "success";
    return Core::ERROR_NONE;
}
```

### Documentation
- Use Doxygen-style comments for public APIs
- Document complex algorithms
- Add inline comments for non-obvious code

**Example:**
```cpp
/**
 * @brief Resolves a Firebolt method to a Thunder plugin alias.
 * 
 * @param method The Firebolt method name (e.g., "device.name")
 * @return The Thunder plugin callsign, or empty string if not found
 */
std::string ResolveAlias(const std::string& method);
```

## Common Development Tasks

### Adding a New API Method

1. **Update resolution configuration** (`AppGateway/resolutions/resolution.base.json`):
```json
{
    "resolutions": {
        "myFeature.myMethod": {
            "alias": "org.rdk.AppGatewayCommon",
            "useComRpc": true,
            "permissionGroup": "org.rdk.permission.group.enhanced"
        }
    }
}
```

2. **Implement handler** in `AppGatewayCommon.cpp`:
```cpp
Core::hresult AppGatewayCommon::HandleAppGatewayRequest(
    const Exchange::GatewayContext &context,
    const string& method,
    const string &payload,
    string& result)
{
    if (method == "myFeature.myMethod") {
        return GetMyFeature(result);
    }
    // ... other methods
}

Core::hresult AppGatewayCommon::GetMyFeature(string& result) {
    // Implementation
    result = R"({"value": "example"})";
    return Core::ERROR_NONE;
}
```

3. **Add header declaration**:
```cpp
Core::hresult GetMyFeature(string& result);
```

4. **Write tests** in `Tests/L1Tests/`:
```cpp
TEST(AppGatewayCommonTest, GetMyFeature) {
    // Test implementation
}
```

### Adding a New Plugin

1. **Create plugin directory structure:**
```bash
mkdir MyPlugin
cd MyPlugin
touch MyPlugin.h MyPlugin.cpp Module.h Module.cpp CMakeLists.txt
```

2. **Implement plugin interface** (see Thunder Plugin Patterns above)

3. **Add CMake configuration:**
```cmake
set(PLUGIN_NAME MyPlugin)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)

add_library(${MODULE_NAME} SHARED
    MyPlugin.cpp
    Module.cpp
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${STORAGE_DIRECTORY}/plugins
)
```

4. **Update root CMakeLists.txt:**
```cmake
if(PLUGIN_MYPLUGIN)
    add_subdirectory(MyPlugin)
endif()
```

### Updating Dependencies

**Update Thunder version:**
```bash
# Edit build_dependencies.sh
# Change: git clone --branch R4.4.1 ...
# To:     git clone --branch R4.5.0 ...

# Rebuild dependencies
./build_dependencies.sh
```

**Update interface definitions:**
```bash
cd entservices-apis
git pull origin main
cd ..
./build_dependencies.sh
```

## Troubleshooting

### Common Build Issues

**Issue: "Could not find WPEFramework"**
```bash
# Solution: Set CMAKE_MODULE_PATH
cmake .. -DCMAKE_MODULE_PATH="${PWD}/../install/tools/cmake"
```

**Issue: "Undefined reference to Thunder symbols"**
```bash
# Solution: Ensure Thunder libraries are in LD_LIBRARY_PATH
export LD_LIBRARY_PATH="${PWD}/install/usr/lib:$LD_LIBRARY_PATH"
```

**Issue: Patch failures in build_dependencies.sh**
```bash
# Solution: Check Thunder/ThunderTools versions match expected
# May need to update patches in entservices-testframework
```

### Runtime Issues

**Issue: Plugin fails to load**
```bash
# Check plugin is built and installed
ls -la install/usr/lib/*/plugins/

# Check Thunder configuration
cat install/usr/share/WPEFramework/AppGateway.json

# Check Thunder logs for errors
journalctl -u wpeframework | grep AppGateway
```

**Issue: "Method not found" errors**
```bash
# Verify resolution configuration
cat /etc/app-gateway/resolution.base.json

# Check if method exists in config
jq '.resolutions["device.name"]' /etc/app-gateway/resolution.base.json
```

**Issue: Permission denied errors**
```bash
# Disable security token for testing
cmake .. -DDISABLE_SECURITY_TOKEN=ON
```

### Test Issues

**Issue: Tests segfault**
```bash
# Run under gdb to get stack trace
gdb --args ./Tests/L1Tests/AppGatewayL1Test
(gdb) run
(gdb) bt
```

**Issue: Mock not found**
```bash
# Ensure entservices-testframework is cloned and built
ls -la ../entservices-testframework/
```

### Debugging Tips

1. **Enable verbose Thunder logs:**
```bash
export THUNDER_VERBOSE=1
```

2. **Use LOG macros in code:**
```cpp
LOGINFO("Processing request: %s", method.c_str());
```

3. **Check plugin status:**
```bash
curl http://127.0.0.1:9998/Service/Controller/Status
```

4. **Monitor system logs:**
```bash
journalctl -f | grep -E "(WPEFramework|AppGateway)"
```

## Best Practices

### Development Workflow
1. Create feature branch from `develop`
2. Make small, focused commits
3. Write tests for new functionality
4. Run tests locally before pushing
5. Create pull request with clear description
6. Address review feedback

### Performance Considerations
- Avoid blocking calls in plugin main thread
- Use Thunder's worker pool for I/O operations
- Cache frequently accessed data
- Profile before optimizing

### Security Considerations
- Validate all input parameters
- Check permissions before sensitive operations
- Avoid logging sensitive data
- Use Thunder's security token validation

### Code Review Checklist
- [ ] Code follows style guidelines
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] No memory leaks (checked with Valgrind)
- [ ] Error handling is comprehensive
- [ ] Logging is appropriate

## Resources

- **Thunder Documentation**: https://rdkcentral.github.io/Thunder/
- **Firebolt Specification**: https://github.com/rdkcentral/firebolt-apis
- **RDK Central**: https://rdkcentral.com/
- **Issue Tracker**: https://github.com/rdkcentral/entservices-appgateway/issues

## Getting Help

- File issues on GitHub: https://github.com/rdkcentral/entservices-appgateway/issues
- Check existing documentation in `docs/` directory
- Review test cases for usage examples
- Consult Thunder documentation for framework questions
