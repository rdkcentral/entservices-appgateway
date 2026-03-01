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

# Provide a compatible target name if callers expect Plugins::Plugins.
#
# IMPORTANT:
# Do NOT use ALIAS here. In some CI/L1Tests-only configure flows the imported
# Thunder targets are not considered "globally visible", which makes
# `add_library(ALIAS ...)` fail during configure.
#
# Instead we provide an IMPORTED GLOBAL INTERFACE target that forwards usage
# requirements to the canonical target.
if(NOT TARGET Plugins::Plugins)
    add_library(Plugins::Plugins INTERFACE IMPORTED GLOBAL)
    if(TARGET WPEFrameworkPlugins::WPEFrameworkPlugins)
        set_target_properties(Plugins::Plugins PROPERTIES
            INTERFACE_LINK_LIBRARIES WPEFrameworkPlugins::WPEFrameworkPlugins
        )
    else()
        message(FATAL_ERROR "WPEFrameworkPlugins::WPEFrameworkPlugins target not found; cannot provide Plugins::Plugins compatibility target.")
    endif()
endif()

set(Plugins_FOUND TRUE)
