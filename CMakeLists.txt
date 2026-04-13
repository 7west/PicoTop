# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Board type")

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
set(PICO_BOARD picotop CACHE STRING "Board type")

set(PICO_PLATFORM rp2350 CACHE STRING "Platform")

# set(PICO_BOARD pico2 CACHE STRING "Board type")


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoTop C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

#====== TINY USB SOON

#====================

add_subdirectory(lib/video/terminal)
add_subdirectory(lib/video/video_out)
add_subdirectory(lib/video/text_out)
add_subdirectory(lib/utilities/os_debug)
add_subdirectory(lib/utilities/rtc)
add_subdirectory(lib/utilities/batt)
add_subdirectory(lib/utilities/buzz)
add_subdirectory(lib/filesystem/sd_block)
add_subdirectory(lib/filesystem/fat16)
add_subdirectory(lib/filesystem/vfs)
add_subdirectory(lib/filesystem/vault)
add_subdirectory(lib/utilities/keyboard)
add_subdirectory(lib/programs/shell)
add_subdirectory(lib/programs/uedit)
add_subdirectory(lib/programs/calc)
add_subdirectory(lib/programs/hangman)
add_subdirectory(lib/core/prog)
add_subdirectory(lib/core/crypto)




############################ 
# Playground - just setting up and light testing
add_executable(playground 
    src/playground/main.c )

target_sources(playground PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/playground/main.c
)

# Add the standard include files to the build
target_include_directories(playground PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/playground
)

pico_enable_stdio_uart(playground 1)
pico_enable_stdio_usb(playground 0)

# Add the standard library to the build
target_link_libraries(playground
    PRIVATE
    pico_stdlib
    pico_multicore
    terminal
    os_debug
    usb_kbd
    
    # tinyusb_host
    # tinyusb_board
)

pico_add_extra_outputs(playground)

#############################
# File System - sd_block > fat16 > vfs
add_executable(filesystem 
    src/filesystem/main.c )

# Add the standard include files to the build
target_include_directories(filesystem PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/filesystem
)

pico_enable_stdio_uart(filesystem 1)
pico_enable_stdio_usb(filesystem 0)

# Add the standard library to the build
target_link_libraries(filesystem
    PRIVATE
    pico_stdlib
    pico_multicore
    terminal
    os_debug
    sd_block
    fat16
    vfs
)

pico_add_extra_outputs(filesystem)

#############################
# Shell Work - getting shells major features working
#       will involve some basic utilities
add_executable(shellwork 
    src/shellwork/main.c 
    src/shellwork/main-fns.c
)

# Add the standard include files to the build
target_include_directories(shellwork PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/shellwork
)

pico_enable_stdio_uart(shellwork 1)
pico_enable_stdio_usb(shellwork 0)

# Add the standard library to the build
target_link_libraries(shellwork
    PRIVATE
    pico_stdlib
    pico_multicore
    terminal
    os_debug
    usb_kbd
    rtc
    prog
    shell
    vfs
    batt
    buzz
    vault
)

pico_add_extra_outputs(shellwork)

#############################
# Board Test - getting the custom PCB up and running
add_executable(boardtest 
    src/boardtest/main.c 
)

# Add the standard include files to the build
target_include_directories(boardtest PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/boardtest
)

pico_enable_stdio_uart(boardtest 1)
pico_enable_stdio_usb(boardtest 0)

# Add the standard library to the build
target_link_libraries(boardtest
    PRIVATE
    pico_stdlib
    # sd_block
    pico_multicore
    terminal
    os_debug
    usb_kbd
    # rtc
    # prog
    # shell
    fat16
    hardware_adc
    hardware_pwm
    video_out
)

pico_add_extra_outputs(boardtest)