# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.8)

# ---------------------------------------------------------------------------
# Package discovery shims for L1Tests-only builds
#
# When configuring only Tests/L1Tests, plugin subdirectories (e.g. AppGateway)
# can call find_package(Plugins) / find_package(Definitions) when NAMESPACE is
# empty. Thunder's canonical package names are WPEFrameworkPlugins and
# WPEFrameworkDefinitions, so we provide local compatibility shims under
# Tests/L1Tests/cmake and ensure CMake can find them.
# ---------------------------------------------------------------------------
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# -----------------------------------------------------------------------------
# L1Tests build products
#
# Historically this directory only built a shared object (L1TestsIN) that is
# loadable by Thunder. For coverage we additionally need a *runnable* test
# executable for AppGateway: AppGatewayL1Test.
# -----------------------------------------------------------------------------
set(PLUGIN_NAME L1TestsIN)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# L1Tests-only configure support
#
# In the CI workflow we configure ONLY this directory:
#   cmake -S <repo>/Tests/L1Tests -B <build> ...
#
# Therefore, any plugin implementation we need for linkage must be pulled in
# explicitly from here.
# -----------------------------------------------------------------------------
#
# IMPORTANT:
# In the L1Tests-only build, the repository root CMakeLists.txt is NOT executed.
# Therefore, any global options/definitions normally set at repo root (like
# DISABLE_SECURITY_TOKEN) must be applied here *before* adding plugin
# subdirectories, otherwise plugin targets (e.g. AppGatewayCommon) will compile
# without the expected flags and can include SecurityAgent headers.
#
if(DISABLE_SECURITY_TOKEN)
    # Apply as a directory-level compile definition so all targets created under
    # this directory (including add_subdirectory()'d plugins) inherit it.
    add_compile_definitions(DISABLE_SECURITY_TOKEN)
endif()

if(USE_THUNDER_R4)
    # Keep Thunder interface selection consistent across tests and pulled-in
    # plugins (some helper headers use this define for interface differences).
    add_compile_definitions(USE_THUNDER_R4)
endif()

if(PLUGIN_APPGATEWAY)
    # AppGateway/CMakeLists.txt defines the implementation library target
    #   WPEFrameworkAppGateway
    # which AppGatewayL1Test must link against to resolve symbols.
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../AppGateway ${CMAKE_BINARY_DIR}/AppGateway)
endif()

if(PLUGIN_APPGATEWAYCOMMON)
    # Also allow building/staging AppGatewayCommon plugin shared library from
    # the same L1Tests-only build when requested.
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../AppGatewayCommon ${CMAKE_BINARY_DIR}/AppGatewayCommon)
endif()

# -----------------------------------------------------------------------------
# Thunder interface selection
#
# The L1Tests build (and especially the mocks in Tests/mocks) must match the
# WPEFramework headers available via CMAKE_PREFIX_PATH / install/usr/include.
#
# In Thunder R4 the PluginHost::IShell interface changed (method signatures and
# return types), and ServiceMock has an #ifdef USE_THUNDER_R4 split accordingly.
# If USE_THUNDER_R4 is not propagated as a compile definition, compilation fails
# with "marked override but does not override".
# -----------------------------------------------------------------------------
option(USE_THUNDER_R4 "Build L1 tests against Thunder (WPEFramework) R4 headers" OFF)

# Keep an easy-to-consume variable for consistent use below.
set(_L1TESTS_USE_THUNDER_R4 ${USE_THUNDER_R4})

# ---------------------------------------------------------------------------
# L1Tests-only build support
#
# When configuring only Tests/L1Tests (Step 23c), the repo provides the needed
# CMake package configs under:
#   <repo>/install/usr/lib/cmake/WPEFrameworkPlugins/WPEFrameworkPluginsConfig.cmake
#
# However this CMakeLists historically asks for "${NAMESPACE}Plugins", which
# resolves to "Plugins" if NAMESPACE is empty. In standalone test builds this
# can fail because the package name is actually WPEFrameworkPlugins.
#
# We bridge this by mapping "Plugins" to WPEFrameworkPlugins *when needed*.
# This keeps existing flows working while allowing the L1Tests-only configure to
# succeed with -DCMAKE_PREFIX_PATH=<repo>/install/usr.
# ---------------------------------------------------------------------------
if(NOT DEFINED NAMESPACE)
    set(NAMESPACE "")
endif()

if("${NAMESPACE}" STREQUAL "")
    # Prefer the canonical Thunder package names.
    find_package(WPEFrameworkPlugins REQUIRED)
    find_package(WPEFrameworkDefinitions REQUIRED)

    # Provide compatibility variables so existing references like
    #   ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    # still work (they become Plugins::Plugins when NAMESPACE is empty).
    if(NOT TARGET Plugins::Plugins AND TARGET WPEFrameworkPlugins::WPEFrameworkPlugins)
        add_library(Plugins::Plugins ALIAS WPEFrameworkPlugins::WPEFrameworkPlugins)
    endif()

    # And the same for Definitions.
    if(NOT TARGET Definitions::Definitions AND TARGET WPEFrameworkDefinitions::WPEFrameworkDefinitions)
        add_library(Definitions::Definitions ALIAS WPEFrameworkDefinitions::WPEFrameworkDefinitions)
    endif()
else()
    # For non-empty namespaces keep the original behavior.
    find_package(${NAMESPACE}Plugins REQUIRED)
    find_package(${NAMESPACE}Definitions REQUIRED)
endif()

# -----------------------------------------------------------------------------
# Shared-object (legacy) test module sources
#   NOTE: Per request, keep this limited to test_UtilsFile.cpp for now.
# -----------------------------------------------------------------------------
#set(TEST_SRC
#    tests/test_UtilsFile.cpp
#)

set(TEST_LIB
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

set(TEST_INC ../../helpers)

# -----------------------------------------------------------------------------
# AppGateway-specific L1 tests (executable)
# -----------------------------------------------------------------------------
set(APPGATEWAY_L1TEST_SRC "")
set(APPGATEWAY_L1TEST_INC "")
set(APPGATEWAY_L1TEST_LIB "")

if(PLUGIN_APPGATEWAY)
    message(STATUS "AppGateway L1 tests: ENABLED (PLUGIN_APPGATEWAY=ON)")

    # Source(s)
    list(APPEND APPGATEWAY_L1TEST_SRC tests/test_AppGateway.cpp)

    # Includes needed by test_AppGateway.cpp (plus common test headers)
    list(APPEND APPGATEWAY_L1TEST_INC
        ../../helpers
        ../../AppGateway
        ../../AppGatewayCommon
    )

    # Libraries:
    #  - Link against the AppGateway implementation itself to resolve symbols used by the tests
    #    (Resolver, AppGatewayImplementation, etc.)
    #  - Also link against the Thunder Plugins collection for core Thunder symbols.
    #
    # Note: In the L1Tests-only build (Step 23), PLUGIN_APPGATEWAY is ON but the
    # AppGateway subdirectory is *not* necessarily added unless we do it explicitly
    # from the Tests/L1Tests configuration (see below).
    # Link against the in-tree AppGateway plugin target. In this repo the plugin's
    # CMake target name resolves to:
    #   MODULE_NAME = ${NAMESPACE}${PLUGIN_NAME}
    # and with empty NAMESPACE that becomes simply:
    #   AppGateway
    #
    # The previous value "WPEFrameworkAppGateway" is not an in-tree target here,
    # so CMake treated it as a raw linker item and emitted `-lWPEFrameworkAppGateway`,
    # which fails because the built shared library is `libAppGateway.so`.
    list(APPEND APPGATEWAY_L1TEST_LIB
        AppGateway
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    )
else()
    message(STATUS "AppGateway L1 tests: DISABLED (set -DPLUGIN_APPGATEWAY=ON to enable)")
endif()

#########################################################################################
# add_plugin_test_ex: Macro to add plugin tests, it will append to TEST_SRC, TEST_INC,
#                     and TEST_LIB. Args are positional.
#                     arg1: test file list as **string**
#                     arg2: include dir list as **string**
#                     arg3: plugin libs to link list as **string**
#
# sample invocation :
#    add_plugin_test_ex(PLUGIN_NAME
#                       "test/file1.cpp;test/file2.cpp"
#                       "../../inc_dir1;../../inc_dir2"
#                       "${NAMESPACE}PluginName;${NAMESPACE}PluginNameImplementation")
# ----------------------------------------- OR ------------------------------------------
# list(APPEND PLUGIN_NAME_SRC test/file1.cpp)
# list(APPEND PLUGIN_NAME_SRC test/file2.cpp)
#
# list(APPEND PLUGIN_NAME_INC ../../inc_dir1)
# list(APPEND PLUGIN_NAME_INC ../../inc_dir2)
#
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginName)
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginNameImplementation)
#
#    add_plugin_test_ex(PLUGIN_NAME
#                       "${PLUGIN_NAME_SRC}"
#                       "${PLUGIN_NAME_INC}"
#                       "${PLUGIN_NAME_LIB}")
#
# NOTE: Alternatively test can choose to update `TEST_SRC`, `TEST_INC` & `TEST_LIB`
#       directly (like in the case of Miracast)
#########################################################################################
macro(add_plugin_test_ex plugin_opt plugin_test_sources_str plugin_includes_str plugin_libs_str)
    # Check if the plugin option is enabled
    if(${plugin_opt})
        message(STATUS "${plugin_opt}=ON")

        string(REPLACE ";" ";" srclist "${plugin_test_sources_str}")
        string(REPLACE ";" ";" inclist "${plugin_includes_str}")
        string(REPLACE ";" ";" liblist "${plugin_libs_str}")

        foreach(item IN LISTS srclist)
            # Add each test source file
            list(APPEND TEST_SRC ${item})
        endforeach()

        foreach(item IN LISTS inclist)
            # Add each include directory
            list(APPEND TEST_INC ${item})
        endforeach()

        foreach(item IN LISTS liblist)
            # Add each libraries to link
            list(APPEND TEST_LIB ${item})
        endforeach()
    else()
        message(STATUS "${plugin_opt}=OFF")
    endif()
endmacro()

# helper to add plugin test
macro(add_plugin_test plugin_name test_files)
    # Convert plugin name to uppercase for the option variable
    string(TOUPPER "${plugin_name}" plugin_option)
    set(plugin_opt "PLUGIN_${plugin_option}")

    add_plugin_test_ex(${plugin_opt} "${test_files}" "../../${plugin_name}" "${NAMESPACE}${plugin_name}")
endmacro()

# -----------------------------------------------------------------------------
# Legacy shared object
#
# In "L1Tests-only" configurations we may intentionally have no legacy test
# sources enabled (TEST_SRC empty). CMake errors if we create a library target
# without sources ("No SOURCES given to target"). Guard it so AppGatewayL1Test
# can still be built and coverage can run.
# -----------------------------------------------------------------------------
if(TEST_SRC)
    add_library(${MODULE_NAME} SHARED ${TEST_SRC})

    if(RDK_SERVICES_L1_TEST)
        target_link_options(${MODULE_NAME} PRIVATE "-Wl,-wrap,opendir")
    endif(RDK_SERVICES_L1_TEST)

    target_link_directories(${MODULE_NAME} PUBLIC
        ${CMAKE_INSTALL_PREFIX}/lib
        ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins
    )

    target_link_libraries(${MODULE_NAME} ${TEST_LIB})

    target_include_directories(${MODULE_NAME}
        PUBLIC
        ${TEST_INC}
        ${CMAKE_INSTALL_PREFIX}/include
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_SOURCE_DIR}/Tests/mocks
        ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
    )

    target_compile_definitions(${MODULE_NAME} PRIVATE
        UNIT_TEST
        # Avoid pulling in SecurityTokenUtil.h / SecurityAgent headers in CI L1 builds.
        DISABLE_SECURITY_TOKEN
    )

    if(_L1TESTS_USE_THUNDER_R4)
        target_compile_definitions(${MODULE_NAME} PRIVATE USE_THUNDER_R4)
    endif()

    install(TARGETS ${MODULE_NAME} DESTINATION lib)
else()
    message(STATUS "Skipping ${MODULE_NAME} (no legacy L1 test sources enabled)")
endif()

# Always write the L1Tests plugin config JSON used by the test runtime.
# Use the non-deprecated invocation form expected by FindConfigGenerator.cmake.
write_config(PLUGINS ${PLUGIN_NAME})

# -----------------------------------------------------------------------------
# AppGatewayL1Test executable (required by the workflow/script)
# -----------------------------------------------------------------------------
if(PLUGIN_APPGATEWAY)
    # GoogleTest/GoogleMock:
    # The CI flow (Step 21) installs googletest into the repo-local prefix:
    #   <repo>/install/usr
    # and provides a CMake package at:
    #   <prefix>/lib/cmake/GTest
    #
    # In the L1Tests-only build (Step 23), the caller is expected to pass:
    #   -DCMAKE_PREFIX_PATH=<repo>/install/usr
    #
    # Using the imported targets ensures include paths (for <gmock/gmock.h>)
    # and link libraries are correctly propagated.
    find_package(GTest REQUIRED)

    add_executable(AppGatewayL1Test ${APPGATEWAY_L1TEST_SRC})

    target_include_directories(AppGatewayL1Test
        PRIVATE
        ${APPGATEWAY_L1TEST_INC}
        ${CMAKE_install_prefix}/include

        # Repo-level mocks live at <repo>/Tests/mocks. In a L1Tests-only configure,
        # CMAKE_SOURCE_DIR points at <repo>/Tests/L1Tests, so we must include both:
        #  - ${CMAKE_SOURCE_DIR}/Tests/mocks        (works for full-tree configs)
        #  - ${CMAKE_SOURCE_DIR}/../mocks           (works for L1Tests-only configs)
        ${CMAKE_SOURCE_DIR}/Tests/mocks
        ${CMAKE_SOURCE_DIR}/../mocks

        ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
    )

    # IMPORTANT:
    # Do NOT link AppGatewayL1Test via "-lWPEFrameworkAppGateway" or plugin install paths.
    # In the L1Tests-only build we want to link directly against the in-tree CMake
    # target (WPEFrameworkAppGateway) so all implementation objects (Resolver,
    # AppGatewayImplementation, etc.) are on the link line and symbols resolve.
    #
    # Plugin staging of the resulting .so files is handled by the workflow script
    # (it copies from the build tree into install/usr/lib/wpeframework/plugins).

    target_link_libraries(AppGatewayL1Test PRIVATE
        ${APPGATEWAY_L1TEST_LIB}
        GTest::gmock
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_compile_definitions(AppGatewayL1Test PRIVATE
        UNIT_TEST
        # Avoid pulling in SecurityTokenUtil.h / SecurityAgent headers in CI L1 builds.
        DISABLE_SECURITY_TOKEN
    )

    if(_L1TESTS_USE_THUNDER_R4)
        target_compile_definitions(AppGatewayL1Test PRIVATE USE_THUNDER_R4)
    endif()

    # Install to the location the script checks first: install/usr/bin/AppGatewayL1Test
    install(TARGETS AppGatewayL1Test RUNTIME DESTINATION bin)
endif()
