# AS7341 + Matriz WS2812 - BitDogLab v7

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_PLATFORM rp2350 CACHE STRING "Platform type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(a734x_test C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable
add_executable(a734x_test
        a734x_test.c
        as7341.c
)

# Gera o header ws2812.pio.h a partir do arquivo PIO
pico_generate_pio_header(a734x_test ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_set_program_name(a734x_test "a734x_test")
pico_set_program_version(a734x_test "0.1")

# Habilita saída via USB (Serial Monitor)
pico_enable_stdio_uart(a734x_test 0)
pico_enable_stdio_usb(a734x_test 1)

# Bibliotecas necessárias
target_link_libraries(a734x_test
        pico_stdlib
        hardware_i2c
        hardware_pio)

# Includes
target_include_directories(a734x_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(a734x_test)
