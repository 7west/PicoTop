#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "hangman.h"
#include "terminal.h"
#include "shell.h"
#include "word_list.h"


#define STAND_TOP_LEFT_X 40
#define STAND_TOP_LEFT_Y 3
#define WRONG_LETTER_AREA_X 4
#define WRONG_LETTER_AREA_Y 10
#define GUESS_WORD_X 4
#define GUESS_WORD_Y (STAND_TOP_LEFT_Y + 5)
#define GAME_OVER_X GUESS_WORD_X
#define GAME_OVER_Y STAND_TOP_LEFT_Y

#define HANGMAN_CHANCES 7
#define MAX_WORD_LEN 13 //if you change word_list.h, you have to update this


typedef struct {
    char the_word[MAX_WORD_LEN + 1];
    char wrong_letters[26+1];
    uint8_t word_len;
    uint8_t mistakes;
    uint8_t num_correct_letters;
    bool game_over;
} Hangman_Memory;

static const struct { uint8_t x, y, sym; } parts[] = {
    {STAND_TOP_LEFT_X,      STAND_TOP_LEFT_Y + 1,   1},
    {STAND_TOP_LEFT_X,      STAND_TOP_LEFT_Y + 2,   179},
    {STAND_TOP_LEFT_X + 1,  STAND_TOP_LEFT_Y + 2,   '\\'},
    {STAND_TOP_LEFT_X - 1,  STAND_TOP_LEFT_Y + 2,   '/'},
    {STAND_TOP_LEFT_X,      STAND_TOP_LEFT_Y + 3,   179},
    {STAND_TOP_LEFT_X + 1,  STAND_TOP_LEFT_Y + 4,   '\\'},
    {STAND_TOP_LEFT_X - 1,  STAND_TOP_LEFT_Y + 4,   '/'},

};

static_assert(sizeof(Hangman_Memory) <= PROG_MEM_SIZE, "Hangman_Memory exceeds the 100kB shared memory bank!");

static Hangman_Memory *mem;

const prog_vtable_t hangman_prog = {
    .name = "Hangman",
    .enter = hangman_enter,
    .exit = hangman_exit,
    .on_key = hangman_on_key,
    .tick = NULL,
    .periodic = NULL,
};

static void hangman_draw_init_ui(void);
static void draw_hangman_stand(void);
static void draw_hangman_person(uint8_t mistakes);
static void hangman_pick_word(void);
static void hangman_take_key(char sym);
static void hangman_add_wrong_letter(char sym);
static void hangman_draw_success(void);
static void hangman_draw_failure(void);
static void hangman_start_over(void);

void hangman_enter(void) {
    term_clear_prog_screen();

    memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);

    mem = (Hangman_Memory *)prog_get_mem();

    hangman_draw_init_ui();

    hangman_pick_word();
}

void hangman_exit(void) {
    memset(prog_get_mem(), 0x00, PROG_MEM_SIZE);
    term_clear_prog_screen();
}

void hangman_on_key(key_event_t key) {
    if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'x' || key.keycode == 'X')) {
        prog_switch(&shell_prog);
        return;
    }

    if ((key.modifiers & KBD_CTRL_BIT) && (key.keycode == 'r' || key.keycode == 'R')) {
        hangman_start_over();
        return;
    }

    if (!mem->game_over && key.keycode > ' ') {
        hangman_take_key(key.keycode);
    }
    
}



static void hangman_start_over(void) {
    term_clear_prog_screen();
    memset(mem, 0x00, sizeof(Hangman_Memory));

    hangman_draw_init_ui();

    hangman_pick_word();
}

static void hangman_take_key(char sym) {

    // capitalize any lowercase letters
    if (sym >= 'a' && sym <= 'z') sym -= 32;

    bool letter_found = false;
    for (uint8_t i = 0; i < mem->word_len; i++) {

        printf("%c\n", mem->the_word[i]);

        if (sym == mem->the_word[i]) {
            term_draw_char(GUESS_WORD_X + i, GUESS_WORD_Y, sym);
            letter_found = true;
            mem->num_correct_letters += 1;
        }
    }

    if (!letter_found && (!strchr(mem->wrong_letters, sym))) {
        hangman_add_wrong_letter(sym);
    }

    if (mem->num_correct_letters == mem->word_len) {
        mem->game_over = true;
        hangman_draw_success();
    }
}

static void hangman_add_wrong_letter(char sym) {
   
    mem->wrong_letters[mem->mistakes] = sym;
    term_draw_char(WRONG_LETTER_AREA_X + mem->mistakes, WRONG_LETTER_AREA_Y + 1, sym);

    mem->mistakes += 1;

    draw_hangman_person(mem->mistakes);
}

static void hangman_draw_success(void) {
    term_draw_string(GAME_OVER_X, GAME_OVER_Y, "SUCCESS");
}

static void hangman_draw_failure(void) {
    term_draw_string(GAME_OVER_X, GAME_OVER_Y, "FAILURE");

    term_draw_string(GUESS_WORD_X, GUESS_WORD_Y, mem->the_word);
}


static void hangman_pick_word(void) {
    uint32_t word_index = get_rand_32();
    word_index &= WORD_LIST_COUNT - 1;

    strcpy(mem->the_word, word_list[word_index]);
    mem->word_len = strlen(mem->the_word);

    for (uint8_t i = 0; i < mem->word_len; i++) {
        term_draw_char(GUESS_WORD_X + i, GUESS_WORD_Y, '-');
    }
}

static void hangman_draw_init_ui(void) {
    draw_hangman_stand();
    draw_hangman_person(0);

    term_draw_string(WRONG_LETTER_AREA_X, WRONG_LETTER_AREA_Y, "Wrong Letters:");

    term_draw_string(1, TERM_NUM_ROWS-1, "^X Exit | ^R Restart");
    term_invert_line(TERM_NUM_ROWS-1);
}

static void draw_hangman_stand(void) {
    term_draw_char(STAND_TOP_LEFT_X, STAND_TOP_LEFT_Y, 218);
    term_draw_char(STAND_TOP_LEFT_X + 1, STAND_TOP_LEFT_Y, 196);
    term_draw_char(STAND_TOP_LEFT_X + 2, STAND_TOP_LEFT_Y, 191);
    term_draw_char(STAND_TOP_LEFT_X + 2, STAND_TOP_LEFT_Y + 1, 179);
    term_draw_char(STAND_TOP_LEFT_X + 2, STAND_TOP_LEFT_Y + 2, 179);
    term_draw_char(STAND_TOP_LEFT_X + 2, STAND_TOP_LEFT_Y + 3, 179);
    term_draw_char(STAND_TOP_LEFT_X + 2, STAND_TOP_LEFT_Y + 4, 179);
    term_draw_char(STAND_TOP_LEFT_X + 2, STAND_TOP_LEFT_Y + 5, 193);
}

static void draw_hangman_person(uint8_t mistakes) {

    for (uint8_t i = 0; i < HANGMAN_CHANCES; i++) {
        if (i < mistakes) {
            term_draw_char(parts[i].x, parts[i].y, parts[i].sym);
        } else {
            term_draw_char(parts[i].x, parts[i].y, ' ');
        }
    }

    if (mistakes == HANGMAN_CHANCES) {
        mem->game_over = true;
        hangman_draw_failure();
    }

}