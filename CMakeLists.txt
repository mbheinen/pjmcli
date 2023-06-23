cmake_minimum_required (VERSION 2.6)
project (pjm-mg-util)

add_executable(pjm-mg-util main.c)

target_link_libraries(pjm-mg-util curl expat)

