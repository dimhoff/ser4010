cmake_minimum_required(VERSION 2.8)
project(ser4010 C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Build Options
set(DEFAULT_SERIAL_DEV "/dev/ttyUSB0" CACHE STRING "Serial device to use by default by the tools if non is specified")

configure_file(config.h.in "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

add_subdirectory(libser4010)
add_subdirectory(tools)
