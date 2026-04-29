add_library(ptable STATIC)

target_sources(ptable PRIVATE
    ptable.c
)

target_include_directories(ptable PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(ptable PUBLIC
    terminal
    prog
    pico_stdlib
    shell
)