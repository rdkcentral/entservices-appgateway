cmake_minimum_required(VERSION 3.16)

project(appgateway_l0test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(APPGW_TEST_ENABLE_WARNINGS "Enable additional compiler warnings for appgateway_l0test" ON)
option(APPGW_ENABLE_COVERAGE "Enable coverage flags for appgateway_l0test" ON)

if(NOT DEFINED PREFIX OR "${PREFIX}" STREQUAL "")
    if(DEFINED ENV{PREFIX} AND NOT "$ENV{PREFIX}" STREQUAL "")
        set(PREFIX "$ENV{PREFIX}")
    else()
        set(PREFIX "${CMAKE_SOURCE_DIR}/../../install/usr")
    endif()
endif()
set(PREFIX "${PREFIX}" CACHE STRING "Install prefix to locate Thunder/WPEFramework artifacts (include/lib/plugins)" FORCE)

message(STATUS "Using PREFIX='${PREFIX}'")

set(APPGW_L0_SOURCES
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

add_executable(appgateway_l0test ${APPGW_L0_SOURCES})

target_compile_definitions(appgateway_l0test PRIVATE
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
    target_compile_options(appgateway_l0test PRIVATE -O0 -g --coverage)
    target_link_options(appgateway_l0test PRIVATE --coverage)
endif()

if(APPGW_TEST_ENABLE_WARNINGS)
    target_compile_options(appgateway_l0test PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

target_include_directories(appgateway_l0test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AppGateway
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/../../AppGateway
    ${CMAKE_SOURCE_DIR}/../../helpers
    ${PREFIX}/include
    ${PREFIX}/include/WPEFramework
)

if(CMAKE_PREFIX_PATH)
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_prefix}/include")
            target_include_directories(appgateway_l0test PRIVATE "${_prefix}/include")
        endif()
        if(EXISTS "${_prefix}/include/WPEFramework")
            target_include_directories(appgateway_l0test PRIVATE "${_prefix}/include/WPEFramework")
        endif()
    endforeach()
endif()

if(EXISTS "${PREFIX}/lib")
    target_link_directories(appgateway_l0test PRIVATE "${PREFIX}/lib")
endif()
if(EXISTS "${PREFIX}/lib/wpeframework/plugins")
    target_link_directories(appgateway_l0test PRIVATE "${PREFIX}/lib/wpeframework/plugins")
endif()
if(EXISTS "${PREFIX}/lib/plugins")
    target_link_directories(appgateway_l0test PRIVATE "${PREFIX}/lib/plugins")
endif()

find_library(WPEFRAMEWORK_CORE_LIB NAMES WPEFrameworkCore ThunderCore HINTS "${PREFIX}/lib")
find_library(WPEFRAMEWORK_MESSAGING_LIB NAMES WPEFrameworkMessaging ThunderMessaging HINTS "${PREFIX}/lib")
find_library(WPEFRAMEWORK_PLUGINS_LIB NAMES WPEFrameworkPlugins ThunderPlugins HINTS "${PREFIX}/lib")
find_library(WPEFRAMEWORK_WEBSOCKET_LIB NAMES WPEFrameworkWebSocket ThunderWebSocket HINTS "${PREFIX}/lib")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(WPEFRAMEWORK_CORE_LIB)
    target_link_libraries(appgateway_l0test PRIVATE ${WPEFRAMEWORK_CORE_LIB})
endif()
if(WPEFRAMEWORK_MESSAGING_LIB)
    target_link_libraries(appgateway_l0test PRIVATE ${WPEFRAMEWORK_MESSAGING_LIB})
endif()
if(WPEFRAMEWORK_PLUGINS_LIB)
    target_link_libraries(appgateway_l0test PRIVATE ${WPEFRAMEWORK_PLUGINS_LIB})
endif()
if(WPEFRAMEWORK_WEBSOCKET_LIB)
    target_link_libraries(appgateway_l0test PRIVATE ${WPEFRAMEWORK_WEBSOCKET_LIB})
endif()

find_library(APPGATEWAY_PLUGIN_LIB
    NAMES AppGateway WPEFrameworkAppGateway
    HINTS
        "${CMAKE_BINARY_DIR}/AppGateway"
        "${CMAKE_BINARY_DIR}/../AppGateway"
        "${PREFIX}/lib/wpeframework/plugins"
        "${PREFIX}/lib/plugins"
        "${PREFIX}/lib"
)

if(NOT APPGATEWAY_PLUGIN_LIB)
    message(FATAL_ERROR "AppGateway plugin library not found. Ensure libAppGateway.so is built and staged under ${PREFIX}/lib/wpeframework/plugins or ${PREFIX}/lib/plugins")
endif()

target_link_libraries(appgateway_l0test PRIVATE
    ${APPGATEWAY_PLUGIN_LIB}
    Threads::Threads
    m
    dl
)

set_target_properties(appgateway_l0test PROPERTIES
    BUILD_RPATH "$ORIGIN;${PREFIX}/lib;${PREFIX}/lib/wpeframework;${PREFIX}/lib/wpeframework/plugins;${PREFIX}/lib/plugins"
    INSTALL_RPATH "${PREFIX}/lib;${PREFIX}/lib/wpeframework;${PREFIX}/lib/wpeframework/plugins;${PREFIX}/lib/plugins"
)

add_custom_target(run
    DEPENDS appgateway_l0test
    COMMAND $<TARGET_FILE:appgateway_l0test>
    COMMENT "Running appgateway_l0test"
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
