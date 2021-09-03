find_package(Python2 COMPONENTS Interpreter)
if (NOT ${Python2_FOUND})
    message(FATAL_ERROR "Python 2 not found. Cannot configure Duktape")
endif()

function(duktape_library)
    cmake_parse_arguments(A "" "TARGET;VERSION;CONFIGURATION" "" ${ARGN})

    FetchContent_Declare(
        duktape_${A_VERSION}
        GIT_REPOSITORY https://github.com/svaarala/duktape
        GIT_TAG        ${A_VERSION}
    )
    FetchContent_GetProperties(duktape_${A_VERSION})
    if(NOT ${duktape_${A_VERSION}_POPULATED})
        FetchContent_Populate(duktape_${A_VERSION})
    endif()

    set(DUKTAPE_SOURCE ${duktape_${A_VERSION}_SOURCE_DIR})

    file(GLOB_RECURSE duktape_input_sources ${duktape_${A_VERSION}_SOURCE_DIR})
    set(DUKTAPE_CONFIGURED_DIR ${CMAKE_CURRENT_BINARY_DIR}/duktape)

    add_custom_command(
        COMMENT "Configuring duktape_${A_VERSION}"
        OUTPUT ${DUKTAPE_CONFIGURED_DIR}/duktape.h
               ${DUKTAPE_CONFIGURED_DIR}/duktape.cpp
               ${DUKTAPE_CONFIGURED_DIR}/duk_config.h
        COMMAND ${CMAKE_COMMAND} -E remove -f ${DUKTAPE_CONFIGURED_DIR}/duktape.cpp
        COMMAND ${Python2_EXECUTABLE}
                    ${DUKTAPE_SOURCE}/tools/configure.py
                    --output-directory ${DUKTAPE_CONFIGURED_DIR}
                    --rom-support --option-file ${A_CONFIGURATION}
        COMMAND ${CMAKE_COMMAND} -E copy
                    ${DUKTAPE_CONFIGURED_DIR}/duktape.c
                    ${DUKTAPE_CONFIGURED_DIR}/duktape.cpp
        COMMAND ${CMAKE_COMMAND} -E remove ${DUKTAPE_CONFIGURED_DIR}/duktape.c
        WORKING_DIRECTORY ${DUKTAPE_SOURCE}
        DEPENDS ${duktape_input_sources} ${A_CONFIGURATION})

    add_library(duktape STATIC ${DUKTAPE_CONFIGURED_DIR}/duktape.cpp)
    target_include_directories(${A_TARGET} PUBLIC ${DUKTAPE_CONFIGURED_DIR})

    # Duktape triggers several warnings
    target_compile_options(${A_TARGET} PRIVATE
        -Wno-maybe-uninitialized
        -Wno-unused-value)
    # target_link_options(${A_TARGET} PUBLIC
    #     -Wl,-ffunction-sections -Wl,-fdata-sections -Wl,--gc-sections)

    # Add library for console implementation
    add_library(${A_TARGET}_console
        ${duktape_${A_VERSION}_SOURCE_DIR}/extras/console/duk_console.c)
    target_include_directories(${A_TARGET}_console PUBLIC
        ${duktape_${A_VERSION}_SOURCE_DIR}/extras/console)
    target_link_libraries(${A_TARGET}_console ${A_TARGET})

    # Add library for node module loader
    add_library(${A_TARGET}_module_node
        ${duktape_${A_VERSION}_SOURCE_DIR}/extras/module-node/duk_module_node.c)
    target_include_directories(${A_TARGET}_module_node PUBLIC
        ${duktape_${A_VERSION}_SOURCE_DIR}/extras/module-node)
    target_link_libraries(${A_TARGET}_module_node ${A_TARGET})
endfunction()