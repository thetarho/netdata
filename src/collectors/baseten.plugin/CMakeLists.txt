# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.12)
project(baseten_plugin C)

# Find required libraries
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED json-c)

# Plugin sources
set(BASETEN_PLUGIN_FILES
    baseten-main.c
    baseten-api.c
    baseten-function.c
    baseten-utils.c
)

# Netdata source root (3 levels up from this CMakeLists.txt)
get_filename_component(NETDATA_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# Find libnetdata - it should be in the installed location after netdata-installer runs
set(LIBNETDATA_INCLUDE_DIR "${NETDATA_SRC_ROOT}/src")
set(LIBNETDATA_LIB "/usr/lib/netdata/libnetdata.a")

# Create the plugin executable
add_executable(baseten.plugin ${BASETEN_PLUGIN_FILES})

# Include directories
target_include_directories(baseten.plugin PRIVATE
    ${LIBNETDATA_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${JSON_C_INCLUDE_DIRS}
)

# Link libraries - order matters!
target_link_libraries(baseten.plugin PRIVATE
    ${LIBNETDATA_LIB}
    ${CURL_LIBRARIES}
    ${JSON_C_LIBRARIES}
    pthread
    m
)

# Set compile options
target_compile_options(baseten.plugin PRIVATE
    -Wall
    -Wextra
)
