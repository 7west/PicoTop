add_library(shell STATIC)

target_sources(shell PRIVATE
    shell.c
    cmd_common.c
    commands/timeset.c
    commands/debug-log.c
    commands/dir_cmds.c
    commands/sd_cmds.c
    commands/file_cmds.c
    commands/battv.c
    commands/beep.c
    commands/uedit_cmd.c
    commands/hexdump.c
    commands/gpio.c
    commands/pinout.c
    commands/car-kill.c
    commands/test.c
    commands/attrib.c
    commands/vault_cmds.c
    commands/calc_cmd.c
    commands/hangman_cmd.c
)

target_include_directories(shell PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(shell PUBLIC 
    terminal 
    pico_stdlib 
    os_debug
    prog
    rtc
    usb_kbd
    terminal
    vfs
    fat16
    batt
    buzz
    uedit
    vault
    calc
    hangman
)