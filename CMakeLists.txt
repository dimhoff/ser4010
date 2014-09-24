cmake_minimum_required(VERSION 2.8)
project(ser4010 C)

set(CMAKE_C_FLAGS "-Wall")

add_subdirectory(libser4010)
add_subdirectory(tools)
