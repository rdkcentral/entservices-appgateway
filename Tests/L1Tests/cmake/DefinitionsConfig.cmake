# Compatibility package config for "Definitions"
#
# Purpose:
#   Some plugin CMakeLists in this repo call:
#       find_package(${NAMESPACE}Definitions REQUIRED)
#   which becomes:
#       find_package(Definitions REQUIRED)
#   when NAMESPACE is empty (common in CI / standalone L1Tests builds).
#
#   However, Thunder installs the canonical config as:
#       WPEFrameworkDefinitionsConfig.cmake
#
#   This file is provided ONLY for the L1Tests-only configure flow so that
#   find_package(Definitions) succeeds without modifying plugin CMakeLists.txt.
#
# Usage:
#   Tests/L1Tests/CMakeLists.txt prepends its local cmake/ directory to
#   CMAKE_PREFIX_PATH so CMake can resolve this package config.

# Load the canonical Thunder definitions package (installed under repo prefix).
find_package(WPEFrameworkDefinitions REQUIRED CONFIG)

# Provide a compatible imported target name if callers expect Definitions::Definitions.
if(NOT TARGET Definitions::Definitions AND TARGET WPEFrameworkDefinitions::WPEFrameworkDefinitions)
    add_library(Definitions::Definitions ALIAS WPEFrameworkDefinitions::WPEFrameworkDefinitions)
endif()

set(Definitions_FOUND TRUE)
