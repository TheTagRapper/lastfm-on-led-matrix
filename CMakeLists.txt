cmake_minimum_required(VERSION 3.31.5)
project(build_main LANGUAGES C)

find_package(CURL REQUIRED)

add_executable(build_main main.c)

target_link_libraries(build_main PRIVATE CURL::libcurl)


