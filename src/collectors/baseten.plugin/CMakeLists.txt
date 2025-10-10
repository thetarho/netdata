# SPDX-License-Identifier: GPL-3.0-or-later

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

# Create the plugin executable
add_executable(baseten.plugin ${BASETEN_PLUGIN_FILES})

# Include directories
target_include_directories(baseten.plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${JSON_C_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(baseten.plugin PRIVATE
    libnetdata
    ${CURL_LIBRARIES}
    ${JSON_C_LIBRARIES}
)

# Set compile options
target_compile_options(baseten.plugin PRIVATE
    -Wall
    -Wextra
    -Werror
)

# Install the plugin
install(TARGETS baseten.plugin
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/netdata/plugins.d
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
