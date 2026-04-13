add_library(prog STATIC
    prog.c
)

target_include_directories(prog PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(prog PUBLIC 
    pico_stdlib 
    os_debug
    usb_kbd
    terminal
    rtc
)