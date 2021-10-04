cmake_minimum_required(VERSION 3.12)

idf_component_register(
    SRCS src/link.cpp src/muxCodec.cpp src/cobs.c
    INCLUDE_DIRS include)
