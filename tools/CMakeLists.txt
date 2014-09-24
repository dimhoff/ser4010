include_directories(${PROJECT_SOURCE_DIR}/libser4010)
link_directories(${PROJECT_BUILD_DIR}/libser4010)

add_executable(ser4010_kaku ser4010_kaku.c)
target_link_libraries(ser4010_kaku ser4010)

add_executable(blindctl_somfy blindctl_somfy.c ser4010_rts.c)
target_link_libraries(blindctl_somfy ser4010)

add_executable(test_comm test_comm.c)
target_link_libraries(test_comm ser4010)

add_executable(ser4010_dump ser4010_dump.c)
target_link_libraries(ser4010_dump ser4010)
