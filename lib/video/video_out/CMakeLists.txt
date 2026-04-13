add_library(video_out STATIC
    video_out.c
)

target_include_directories(video_out PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(video_out PUBLIC 
    pico_stdlib 
    hardware_dma 
    pico_sync
    os_debug
)
