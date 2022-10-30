cmake_minimum_required(VERSION 3.12)

idf_component_register(
    SRCS src/link.cpp src/encoding.cpp src/cobs.c
    INCLUDE_DIRS include
    REQUIRES jacUtility)
