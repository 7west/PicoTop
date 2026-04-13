add_library(calc STATIC)

target_sources(calc PRIVATE
    calc.c
    math/eval.c
    math/vars.c
    math/token.c
    bg/calc_ui.c
    bg/calc_edit.c
    bg/calc_file.c
)

target_include_directories(calc PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(calc PUBLIC
    pico_stdlib
    prog
    os_debug
    terminal
    vfs
    usb_kbd
)