cmake_minimum_required(VERSION 3.12)

# This command has to come first to properly initialize all variables introduced
# the IDF build system
idf_component_register(
    SRCS
    INCLUDE_DIRS include
    REQUIRES jacFilesystem
    EMBED_FILES assets/regeneratorRuntime.js assets/rtosTimerWrappers.js)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${COMPONENT_DIR}/releng)

include(FetchContent)
include(Duktape)

target_compile_options(${COMPONENT_LIB} INTERFACE
    -Wno-maybe-uninitialized
    -Wno-unused-value)


duktape_library(
    TARGET duktape
    VERSION v2.6.0
    CONFIGURATION ${COMPONENT_DIR}/duktape.yml)

target_link_libraries(${COMPONENT_LIB} INTERFACE duktape duktape_console duktape_module_node)
