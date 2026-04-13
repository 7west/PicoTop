add_library(batt STATIC
    batt.c
)

target_include_directories(batt PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(batt PUBLIC
    os_debug
    pico_stdlib
    hardware_adc
    buzz
)