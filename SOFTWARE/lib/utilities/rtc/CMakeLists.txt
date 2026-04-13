add_library(rtc STATIC
    rtc.c
    rv3028.c
)

target_include_directories(rtc PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(rtc PUBLIC 
    pico_stdlib 
    pico_aon_timer 
    os_debug
    hardware_i2c
)