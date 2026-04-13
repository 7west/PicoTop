#pragma once

#include <stdio.h>
#include <string.h>
#include "shell.h"

typedef struct {
    uint8_t optind;
    const char *optarg;
    const char *scanpos;
} getopt_state_t;

// cmd_common.c
void getopt_init(getopt_state_t *state);

int32_t getopt(uint8_t argc, const char ** argv, const char *optstr, getopt_state_t *state);

// shell native commands
bool shell_cmd_clear(uint8_t argc, const char ** argv, const out_t *out);

bool shell_cmd_history(uint8_t argc, const char ** argv, const out_t *out);

// SD commands
bool cmd_sd_format(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_sd_mount(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_sd_unmount(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_sd_usage(uint8_t argc, const char ** argv, const out_t *out);

// Vault commands
bool cmd_vault_setup(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_vault_encrypt(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_vault_decrypt(uint8_t argc, const char ** argv, const out_t *out);

// attrib.c
bool cmd_attr(uint8_t argc, const char ** argv, const out_t *out);

// test.c
bool cmd_test(uint8_t argc, const char ** argv, const out_t *out);

// gpio.c
bool cmd_gpio(uint8_t argc, const char ** argv, const out_t *out);

// car-rem.c
bool cmd_car_kill(uint8_t argc, const char ** argv, const out_t *out);

// pinout.c
bool cmd_pinout(uint8_t argc, const char ** argv, const out_t *out);

// timeset.c:
bool cmd_timeset(uint8_t argc, const char ** argv, const out_t *out);

// battv.c
bool cmd_battv(uint8_t argc, const char ** argv, const out_t *out);

// hexdump.c
bool cmd_hexdump(uint8_t argc, const char ** argv, const out_t *out);

// debug-log.c
bool cmd_debug_log(uint8_t argc, const char ** argv, const out_t *out);

// uedit_cmd.c
bool cmd_uedit(uint8_t argc, const char ** argv, const out_t *out);

// calc_cmd.c
bool cmd_calc(uint8_t argc, const char ** argv, const out_t *out);

// hangman_cmd.c
bool cmd_hangman(uint8_t argc, const char ** argv, const out_t *out);

// beep.c
bool cmd_beep(uint8_t argc, const char ** argv, const out_t *out);

// dir_cmds.c: pwd, chdir, ls, mkdir, rmdir
bool cmd_pwd(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_chdir(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_ls(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_mkdir(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_rmdir(uint8_t argc, const char ** argv, const out_t *out);

// file_cmds.c: rm, cp, mv
bool cmd_rm(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_move(uint8_t argc, const char ** argv, const out_t *out);

bool cmd_copy(uint8_t argc, const char ** argv, const out_t *out);