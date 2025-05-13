# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Projeto_webserver C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Projeto_webserver Projeto_webserver.c inc/ssd1306.c)

pico_set_program_name(Projeto_webserver "Projeto_webserver")
pico_set_program_version(Projeto_webserver "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Projeto_webserver 0)
pico_enable_stdio_usb(Projeto_webserver 1)

pico_generate_pio_header(Projeto_webserver ${CMAKE_CURRENT_LIST_DIR}/extra/animacoes_led.pio)

# Add the standard library to the build
target_link_libraries(Projeto_webserver
        pico_stdlib
        hardware_gpio
        hardware_adc
        hardware_adc
        hardware_pio
        pico_cyw43_arch_lwip_threadsafe_background
)

# Add the standard include files to the build
target_include_directories(Projeto_webserver PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/lwip/src/include
    ${PICO_SDK_PATH}/lib/lwip/src/include/arch
    ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
    ${CMAKE_CURRENT_SOURCE_DIR}/extra  # Para lwipopts.h
)

target_sources(Projeto_webserver PRIVATE
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/httpd.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/http/fs.c
)

# Add any user requested libraries
target_link_libraries(Projeto_webserver 
        hardware_i2c)

# Add any user requested libraries

pico_add_extra_outputs(Projeto_webserver)