idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ""
    REQUIRES jacMachine jacStorage jacLink
             nvs_flash log)

target_compile_options(${COMPONENT_LIB} PUBLIC
    -Wno-maybe-uninitialized
    -Wno-unused-value)