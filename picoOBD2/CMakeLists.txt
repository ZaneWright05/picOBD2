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
set(PICO_BOARD pico2_w CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(picOBD2)
pico_sdk_init()

# add compiled files subdirectory
add_subdirectory(UI/Config)
add_subdirectory(UI/OLED)
add_subdirectory(UI/Fonts)
add_subdirectory(UI/GUI)
# add_subdirectory(examples)



# add header file directory
include_directories(./UI/Config)
include_directories(./UI/GUI)
include_directories(./UI/OLED)


# generate executable file
add_executable(picOBD2 
main.c
CAN/MCP2515.c
CAN/CAN_DEV_Config.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(picOBD2 1)
pico_enable_stdio_uart(picOBD2 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picOBD2)

target_link_libraries(picOBD2
 OLED GUI Fonts Config pico_stdlib hardware_spi)