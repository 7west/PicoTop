add_library(terminal STATIC
    terminal.c
)

target_include_directories(terminal PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(terminal PUBLIC 
    pico_stdlib
    text_out 
    video_out 
    os_debug
)