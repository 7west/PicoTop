add_library(vfs STATIC
    vfs.c
)

target_include_directories(vfs PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(vfs PUBLIC
    pico_stdlib
    os_debug
    fat16
    vault
)