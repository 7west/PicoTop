add_library(sd_block STATIC
    sd_block.c
)

target_include_directories(sd_block PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(sd_block PUBLIC 
    pico_stdlib 
    os_debug 
    hardware_spi
)