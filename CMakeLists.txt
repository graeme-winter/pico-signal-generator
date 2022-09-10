cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH "/home/pi/C/pico/pico-sdk")

include(pico_sdk_import.cmake)

project(signal_generator C CXX ASM)

pico_sdk_init()

add_executable(signal_generator signal_generator.c)

pico_generate_pio_header(signal_generator ${CMAKE_CURRENT_LIST_DIR}/eightbit.pio)
pico_generate_pio_header(signal_generator ${CMAKE_CURRENT_LIST_DIR}/clock.pio)

pico_set_program_name(signal_generator "signal_generator")
pico_set_program_version(signal_generator "0.1")

pico_enable_stdio_uart(signal_generator 1)
pico_enable_stdio_usb(signal_generator 0)

target_link_libraries(signal_generator pico_stdlib hardware_pio hardware_dma)

pico_add_extra_outputs(signal_generator)

