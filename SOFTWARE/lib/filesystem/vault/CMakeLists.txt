add_library(vault STATIC
    vault.c
)

target_include_directories(vault PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(vault PUBLIC
    pico_stdlib
    os_debug
    fat16
    vfs
    crypto
    pico_mbedtls_crypto
    prog
    pico_sha256
    shell
    usb_kbd
)