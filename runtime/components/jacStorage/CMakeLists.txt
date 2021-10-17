cmake_minimum_required(VERSION 3.12)

idf_component_register(
    SRCS src/storage.cpp src/uploader.cpp
    INCLUDE_DIRS include
    REQUIRES jacUtility jacFilesystem jacLink fatfs mbedtls)
