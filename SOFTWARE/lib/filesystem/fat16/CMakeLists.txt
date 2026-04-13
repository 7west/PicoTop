add_library(fat16 STATIC
    fat16.c
)

target_include_directories(fat16 PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(fat16 PUBLIC 
    pico_stdlib 
    os_debug 
    sd_block
    rtc
)