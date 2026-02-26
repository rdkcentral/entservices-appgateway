# Compatibility package config for "Plugins"
#
# Purpose:
#   Some plugin CMakeLists in this repo call:
#       find_package(${NAMESPACE}Plugins REQUIRED)
#   which becomes:
#       find_package(Plugins REQUIRED)
#   when NAMESPACE is empty (common in CI / standalone L1Tests builds).
#
#   However, Thunder installs the canonical config as:
#       WPEFrameworkPluginsConfig.cmake
#
#   This file is provided ONLY for the L1Tests-only configure flow so that
#   find_package(Plugins) succeeds without modifying plugin CMakeLists.txt.
#
# Usage:
#   Tests/L1Tests/CMakeLists.txt prepends its local cmake/ directory to
#   CMAKE_PREFIX_PATH so CMake can resolve this package config.

# Load the canonical Thunder plugins package (installed under repo prefix).
find_package(WPEFrameworkPlugins REQUIRED CONFIG)

# Provide a compatible imported target name if callers expect Plugins::Plugins.
if(NOT TARGET Plugins::Plugins AND TARGET WPEFrameworkPlugins::WPEFrameworkPlugins)
    add_library(Plugins::Plugins ALIAS WPEFrameworkPlugins::WPEFrameworkPlugins)
endif()

set(Plugins_FOUND TRUE)
