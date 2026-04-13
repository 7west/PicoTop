add_library(hangman STATIC)

target_sources(hangman PRIVATE
    hangman.c



)

target_include_directories(hangman PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(hangman PUBLIC 
    terminal
    prog
    pico_stdlib
    shell
    

)