cmake_minimum_required (VERSION 2.6)
project (pjmcli)

add_executable(pjmcli main.c)

target_link_libraries(pjmcli curl expat)

