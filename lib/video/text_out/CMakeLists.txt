add_library(text_out STATIC
    text_out.c
)

target_include_directories(text_out PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(text_out PUBLIC video_out os_debug pico_stdlib)