cmake_minimum_required(VERSION 3.16)

project(appgateway_l0test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(APPGW_TEST_ENABLE_WARNINGS "Enable additional compiler warnings for appgateway_l0test" ON)
option(APPGW_ENABLE_COVERAGE "Enable coverage flags for appgateway_l0test" ON)
option(APPGW_L0_ENABLE_APPGATEWAY "Build AppGateway L0 test target" ON)
option(APPGW_L0_ENABLE_APPGATEWAYCOMMON "Build AppGatewayCommon L0 test target (when sources are added)" OFF)
option(APPGW_L0_ENABLE_APPNOTIFICATIONS "Build AppNotifications L0 test target (when sources are added)" OFF)

if(NOT DEFINED PREFIX OR "${PREFIX}" STREQUAL "")
    if(DEFINED ENV{PREFIX} AND NOT "$ENV{PREFIX}" STREQUAL "")
        set(PREFIX "$ENV{PREFIX}")
    else()
        set(PREFIX "${CMAKE_SOURCE_DIR}/../../install/usr")
    endif()
endif()
set(PREFIX "${PREFIX}" CACHE STRING "Install prefix to locate Thunder/WPEFramework artifacts (include/lib/plugins)" FORCE)

message(STATUS "Using PREFIX='${PREFIX}'")

set(APPGW_APPGATEWAY_L0_SOURCES
    ${CMAKE_SOURCE_DIR}/../../AppGateway/Module.cpp
    ${CMAKE_SOURCE_DIR}/../../AppGateway/AppGateway.cpp
    ${CMAKE_SOURCE_DIR}/../../AppGateway/Resolver.cpp
    ${CMAKE_SOURCE_DIR}/../../AppGateway/AppGatewayImplementation.cpp
    ${CMAKE_SOURCE_DIR}/../../AppGateway/AppGatewayResponderImplementation.cpp
    ${CMAKE_SOURCE_DIR}/../../AppGateway/AppGatewayTelemetry.cpp
    AppGateway/AppGatewayTest.cpp
    AppGateway/AppGateway_Init_DeinitTests.cpp
    AppGateway/AppGateway_JsonRpcResolveTests.cpp
    AppGateway/Resolver_Configure_And_ResolveTests.cpp
    AppGateway/Responder_BehaviorTests.cpp
    AppGateway/AppGatewayResponderImplementation_Tests.cpp
    AppGateway/ContextUtils_ConversionTests.cpp
    AppGateway/AppGatewayImplementation_BranchTests.cpp
    common/L0Bootstrap.cpp
)
set(APPGW_L0_PRIMARY_TARGET "")

if(APPGW_L0_ENABLE_APPGATEWAY)
    add_executable(appgateway_l0test ${APPGW_APPGATEWAY_L0_SOURCES})
    set(APPGW_L0_PRIMARY_TARGET appgateway_l0test)
else()
    message(STATUS "APPGW_L0_ENABLE_APPGATEWAY=OFF; skipping appgateway_l0test target")
endif()

if(APPGW_L0_ENABLE_APPGATEWAYCOMMON)
    message(WARNING "APPGW_L0_ENABLE_APPGATEWAYCOMMON=ON but no L0 AppGatewayCommon source set is defined yet. Add Tests/L0Tests/AppGatewayCommon sources and target wiring.")
endif()

if(APPGW_L0_ENABLE_APPNOTIFICATIONS)
    message(WARNING "APPGW_L0_ENABLE_APPNOTIFICATIONS=ON but no L0 AppNotifications source set is defined yet. Add Tests/L0Tests/AppNotifications sources and target wiring.")
endif()

if(APPGW_L0_PRIMARY_TARGET STREQUAL "")
    message(FATAL_ERROR "No L0 target enabled. Enable at least one APPGW_L0_ENABLE_* option.")
endif()

target_compile_definitions(${APPGW_L0_PRIMARY_TARGET} PRIVATE
    MODULE_NAME=AppGateway_L0Test
    APPGATEWAY_MAJOR_VERSION=1
    APPGATEWAY_MINOR_VERSION=0
    APPGATEWAY_PATCH_VERSION=0
    LEGACY_FIREBOLT_VERSION="0"
    RDK8_FIREBOLT_VERSION="8"
    ENABLE_DEBUG_FOR_CONNECTION="enableDebugForConnection"
    DISABLE_DEBUG_FOR_CONNECTION="disableDebugForConnection"
)

if(APPGW_ENABLE_COVERAGE)
    target_compile_options(${APPGW_L0_PRIMARY_TARGET} PRIVATE -O0 -g --coverage)
    target_link_options(${APPGW_L0_PRIMARY_TARGET} PRIVATE --coverage)
endif()

if(APPGW_TEST_ENABLE_WARNINGS)
    target_compile_options(${APPGW_L0_PRIMARY_TARGET} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AppGateway
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/../../AppGateway
    ${CMAKE_SOURCE_DIR}/../../helpers
)

if(EXISTS "${PREFIX}/lib")
    target_link_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE "${PREFIX}/lib")
endif()
if(EXISTS "${PREFIX}/lib/wpeframework/plugins")
    target_link_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE "${PREFIX}/lib/wpeframework/plugins")
endif()
if(EXISTS "${PREFIX}/lib/plugins")
    target_link_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE "${PREFIX}/lib/plugins")
endif()

# Prefer namespace-based package targets (repo convention), fallback to direct libs for standalone CI layouts.
set(APPGW_NAMESPACE "")
if(DEFINED NAMESPACE AND NOT "${NAMESPACE}" STREQUAL "")
    set(APPGW_NAMESPACE "${NAMESPACE}")
elseif(DEFINED ENV{NAMESPACE} AND NOT "$ENV{NAMESPACE}" STREQUAL "")
    set(APPGW_NAMESPACE "$ENV{NAMESPACE}")
endif()

set(APPGW_HAVE_CORE_TARGET FALSE)
set(APPGW_HAVE_MESSAGING_TARGET FALSE)
set(APPGW_HAVE_PLUGINS_TARGET FALSE)
set(APPGW_HAVE_WEBSOCKET_TARGET FALSE)

if(NOT "${APPGW_NAMESPACE}" STREQUAL "")
    find_package(${APPGW_NAMESPACE}Core QUIET)
    find_package(${APPGW_NAMESPACE}Messaging QUIET)
    find_package(${APPGW_NAMESPACE}Plugins QUIET)
    find_package(${APPGW_NAMESPACE}WebSocket QUIET)

    if(TARGET ${APPGW_NAMESPACE}Core::${APPGW_NAMESPACE}Core)
        target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${APPGW_NAMESPACE}Core::${APPGW_NAMESPACE}Core)
        set(APPGW_HAVE_CORE_TARGET TRUE)
    endif()
    if(TARGET ${APPGW_NAMESPACE}Messaging::${APPGW_NAMESPACE}Messaging)
        target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${APPGW_NAMESPACE}Messaging::${APPGW_NAMESPACE}Messaging)
        set(APPGW_HAVE_MESSAGING_TARGET TRUE)
    endif()
    if(TARGET ${APPGW_NAMESPACE}Plugins::${APPGW_NAMESPACE}Plugins)
        target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${APPGW_NAMESPACE}Plugins::${APPGW_NAMESPACE}Plugins)
        set(APPGW_HAVE_PLUGINS_TARGET TRUE)
    endif()
    if(TARGET ${APPGW_NAMESPACE}WebSocket::${APPGW_NAMESPACE}WebSocket)
        target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${APPGW_NAMESPACE}WebSocket::${APPGW_NAMESPACE}WebSocket)
        set(APPGW_HAVE_WEBSOCKET_TARGET TRUE)
    endif()
endif()

# Fallback include path discovery for environments without namespace package configs.
if(NOT (APPGW_HAVE_CORE_TARGET AND APPGW_HAVE_MESSAGING_TARGET AND APPGW_HAVE_PLUGINS_TARGET AND APPGW_HAVE_WEBSOCKET_TARGET))
    target_include_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE
        ${PREFIX}/include
        ${PREFIX}/include/WPEFramework
    )

    if(CMAKE_PREFIX_PATH)
        foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
            if(EXISTS "${_prefix}/include")
                target_include_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE "${_prefix}/include")
            endif()
            if(EXISTS "${_prefix}/include/WPEFramework")
                target_include_directories(${APPGW_L0_PRIMARY_TARGET} PRIVATE "${_prefix}/include/WPEFramework")
            endif()
        endforeach()
    endif()
endif()

if(NOT APPGW_HAVE_CORE_TARGET)
    find_library(WPEFRAMEWORK_CORE_LIB NAMES WPEFrameworkCore ThunderCore HINTS "${PREFIX}/lib")
endif()
if(NOT APPGW_HAVE_MESSAGING_TARGET)
    find_library(WPEFRAMEWORK_MESSAGING_LIB NAMES WPEFrameworkMessaging ThunderMessaging HINTS "${PREFIX}/lib")
endif()
if(NOT APPGW_HAVE_PLUGINS_TARGET)
    find_library(WPEFRAMEWORK_PLUGINS_LIB NAMES WPEFrameworkPlugins ThunderPlugins HINTS "${PREFIX}/lib")
endif()
if(NOT APPGW_HAVE_WEBSOCKET_TARGET)
    find_library(WPEFRAMEWORK_WEBSOCKET_LIB NAMES WPEFrameworkWebSocket ThunderWebSocket HINTS "${PREFIX}/lib")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(DEFINED WPEFRAMEWORK_CORE_LIB AND WPEFRAMEWORK_CORE_LIB)
    target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${WPEFRAMEWORK_CORE_LIB})
endif()
if(DEFINED WPEFRAMEWORK_MESSAGING_LIB AND WPEFRAMEWORK_MESSAGING_LIB)
    target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${WPEFRAMEWORK_MESSAGING_LIB})
endif()
if(DEFINED WPEFRAMEWORK_PLUGINS_LIB AND WPEFRAMEWORK_PLUGINS_LIB)
    target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${WPEFRAMEWORK_PLUGINS_LIB})
endif()
if(DEFINED WPEFRAMEWORK_WEBSOCKET_LIB AND WPEFRAMEWORK_WEBSOCKET_LIB)
    target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE ${WPEFRAMEWORK_WEBSOCKET_LIB})
endif()

target_link_libraries(${APPGW_L0_PRIMARY_TARGET} PRIVATE
    Threads::Threads
    m
    dl
)

# Keep runtime search paths compatible across namespace/layout variants.
set(APPGW_RPATH_DIRS
    "${PREFIX}/lib"
    "${PREFIX}/lib/plugins"
    "${PREFIX}/lib/wpeframework"
    "${PREFIX}/lib/wpeframework/plugins"
    "${PREFIX}/lib/thunder"
    "${PREFIX}/lib/thunder/plugins"
)

if(NOT "${APPGW_NAMESPACE}" STREQUAL "")
    string(TOLOWER "${APPGW_NAMESPACE}" APPGW_STORAGE_DIRECTORY)
    list(APPEND APPGW_RPATH_DIRS
        "${PREFIX}/lib/${APPGW_STORAGE_DIRECTORY}"
        "${PREFIX}/lib/${APPGW_STORAGE_DIRECTORY}/plugins"
    )
endif()

list(REMOVE_DUPLICATES APPGW_RPATH_DIRS)

set(APPGW_BUILD_RPATH "$ORIGIN")
foreach(_rpath_dir IN LISTS APPGW_RPATH_DIRS)
    set(APPGW_BUILD_RPATH "${APPGW_BUILD_RPATH};${_rpath_dir}")
endforeach()

set(APPGW_INSTALL_RPATH "${APPGW_RPATH_DIRS}")

set_target_properties(${APPGW_L0_PRIMARY_TARGET} PROPERTIES
    BUILD_RPATH "${APPGW_BUILD_RPATH}"
    INSTALL_RPATH "${APPGW_INSTALL_RPATH}"
)

add_custom_target(run
    DEPENDS ${APPGW_L0_PRIMARY_TARGET}
    COMMAND $<TARGET_FILE:${APPGW_L0_PRIMARY_TARGET}>
    COMMENT "Running ${APPGW_L0_PRIMARY_TARGET}"
    VERBATIM
)

find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)

if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E echo "lcov/genhtml not found. Install lcov to generate coverage report."
        COMMENT "Coverage tools not found; skipping coverage data generation."
        VERBATIM
    )
else()
    add_custom_target(coverage
        COMMAND "${LCOV_EXECUTABLE}" -c -o coverage.info -d "${CMAKE_BINARY_DIR}"
        COMMAND "${GENHTML_EXECUTABLE}" -o coverage coverage.info
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/.."
        COMMENT "Generating coverage report with lcov/genhtml"
        VERBATIM
    )
endif()
