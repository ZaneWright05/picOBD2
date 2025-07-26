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
project(UICode)
pico_sdk_init()

# add compiled files subdirectory
add_subdirectory(lib/Config)
add_subdirectory(lib/OLED)
add_subdirectory(lib/Fonts)
add_subdirectory(lib/GUI)
add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src)
# add_subdirectory(examples)



# add header file directory
include_directories(./lib/Config)
include_directories(./lib/GUI)
include_directories(./lib/OLED)
# include_directories(./lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src)


# generate executable file
add_executable(UICode 
main.c
hw_config.c
config_writer.c
ui.c
pid.c
)

# enable usb output, disable uart output
pico_enable_stdio_usb(UICode 1)
pico_enable_stdio_uart(UICode 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(UICode)

target_link_libraries(UICode
            OLED
            GUI
            Fonts
            Config
            pico_stdlib
            hardware_spi
            no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)