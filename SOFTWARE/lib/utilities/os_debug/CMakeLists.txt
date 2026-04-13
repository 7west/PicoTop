add_library(os_debug STATIC
    os_debug.c
)

target_include_directories(os_debug PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(os_debug PUBLIC 
    pico_stdlib 
    buzz
    vfs
    rtc
)