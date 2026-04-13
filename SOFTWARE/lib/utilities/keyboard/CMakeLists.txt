add_library(usb_kbd STATIC
    usb_kbd.c
)

target_include_directories(usb_kbd PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(usb_kbd PUBLIC
    pico_stdlib
    os_debug
    tinyusb_host
    tinyusb_board
)