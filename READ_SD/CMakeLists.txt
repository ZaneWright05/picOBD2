# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(picOBD2)
pico_sdk_init()


# generate executable file
add_executable(sdReader 
main.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(sdReader 1)
pico_enable_stdio_uart(sdReader 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(sdReader)

target_link_libraries(sdReader pico_stdlib hardware_spi)