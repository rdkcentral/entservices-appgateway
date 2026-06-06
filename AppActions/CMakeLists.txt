set(PLUGIN_NAME AppActions)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

message("Setup ${MODULE_NAME} v${PROJECT_VERSION}")

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

add_compile_definitions(APPACTIONS_MAJOR_VERSION=${VERSION_MAJOR})
add_compile_definitions(APPACTIONS_MINOR_VERSION=${VERSION_MINOR})
add_compile_definitions(APPACTIONS_PATCH_VERSION=${VERSION_PATCH})

set(MODULE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(PLUGIN_APPACTIONS_STARTUPORDER "" CACHE STRING "To configure startup order of AppActions plugin")
set(PLUGIN_APPACTIONS_AUTOSTART "false" CACHE STRING "Automatically start AppActions")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
        AppActions.cpp
        AppActions.h
        Module.cpp)

target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,defs)
target_include_directories(${MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../helpers)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_link_libraries(${MODULE_NAME}
        PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        uuid)

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
        AppActionsImplementation.cpp
        AppActionsImplementation.h
        Module.cpp)

target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)
# To error any missing links
target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,-z,defs)
# To error any warnings
add_compile_options(-Wall -Werror)

target_compile_definitions(${PLUGIN_IMPLEMENTATION} PRIVATE PLUGIN_NAME=Plugin_${PLUGIN_NAME})

target_link_libraries(${PLUGIN_IMPLEMENTATION}
        PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        uuid)

install(TARGETS ${PLUGIN_IMPLEMENTATION}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})