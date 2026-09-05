cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)


project(last_fm_matrix C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(build_main
	main.c
	cJSON.c
	json-parser.h
	lwip-callbacks.h
)

pico_enable_stdio_usb(build_main 1)
pico_enable_stdio_uart(build_main 0)

pico_add_extra_outputs(build_main)

target_include_directories(build_main PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries (build_main pico_cyw43_arch_lwip_threadsafe_background pico_lwip_mbedtls pico_mbedtls pico_stdlib)
