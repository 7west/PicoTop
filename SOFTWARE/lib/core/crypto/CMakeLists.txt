add_library(crypto STATIC
    crypto.c
)

target_include_directories(crypto PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(crypto PUBLIC
    pico_stdlib
    os_debug
    pico_rand
    pico_mbedtls_crypto
    pico_mbedtls_headers
    pico_sha256
    vault
)