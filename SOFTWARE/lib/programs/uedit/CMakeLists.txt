add_library(uedit STATIC)

target_sources(uedit PRIVATE
    uedit.c
    uedit_view.c
    uedit_doc.c
)

target_include_directories(uedit PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(uedit PUBLIC
    terminal
    pico_stdlib
    os_debug
    prog
    vfs
    shell
    buzz
)