add_library(buzz STATIC
    buzz.c
)

target_include_directories(buzz PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(buzz PUBLIC
    os_debug
    pico_stdlib
    hardware_pwm
    hardware_sync
)