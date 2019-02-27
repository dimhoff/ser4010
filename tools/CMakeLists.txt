include_directories(${PROJECT_SOURCE_DIR}/libser4010)
link_directories(${PROJECT_BUILD_DIR}/libser4010)

add_executable(ser4010_kaku ser4010_kaku.c)
target_link_libraries(ser4010_kaku ser4010)

add_executable(ser4010_somfy ser4010_somfy.c ser4010_rts.c dehexify.c)
target_link_libraries(ser4010_somfy ser4010)

add_executable(ser4010_si443x ser4010_si443x.c pn9.c crc_16.c dehexify.c)
target_link_libraries(ser4010_si443x ser4010)

add_executable(ser4010_test_comm test_comm.c)
target_link_libraries(ser4010_test_comm ser4010)

add_executable(ser4010_dump ser4010_dump.c)
target_link_libraries(ser4010_dump ser4010)

add_executable(ser4010_console ser4010_console.c dehexify.c)
target_link_libraries(ser4010_console ser4010 readline)

add_executable(ser4010_pulse ser4010_pulse.c)
target_link_libraries(ser4010_pulse ser4010)

add_executable(ser4010_sweep ser4010_sweep.c)
target_link_libraries(ser4010_sweep ser4010)
