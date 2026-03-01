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

# Provide a compatible target name if callers expect Definitions::Definitions.
#
# IMPORTANT:
# Do NOT use ALIAS here. In some CI/L1Tests-only configure flows the imported
# Thunder targets are not considered "globally visible", which makes
# `add_library(ALIAS ...)` fail during configure.
#
# Instead we provide an IMPORTED GLOBAL INTERFACE target that forwards usage
# requirements to the canonical target.
if(NOT TARGET Definitions::Definitions)
    add_library(Definitions::Definitions INTERFACE IMPORTED GLOBAL)
    if(TARGET WPEFrameworkDefinitions::WPEFrameworkDefinitions)
        set_target_properties(Definitions::Definitions PROPERTIES
            INTERFACE_LINK_LIBRARIES WPEFrameworkDefinitions::WPEFrameworkDefinitions
        )
    else()
        message(FATAL_ERROR "WPEFrameworkDefinitions::WPEFrameworkDefinitions target not found; cannot provide Definitions::Definitions compatibility target.")
    endif()
endif()

set(Definitions_FOUND TRUE)
