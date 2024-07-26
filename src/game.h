#ifndef __GAME_H__
#define __GAME_H__

// All platform - independant game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "types.h"

typedef struct
{
    u8 *backbuffer_memory;
    i32 width;
    i32 height;
} game_framebuffer_t;

// note(rtarun9) : State changed is used to determine if the state of the key
// press has changed since last frame (for example : the value will be true if
// key was down last frame and no is released).
typedef struct
{
    b32 state_changed;
    b32 is_key_down;
} game_key_state_t;

// note(rtarun9) : Create an array for this in the future?
typedef struct
{
    game_key_state_t key_w;
    game_key_state_t key_a;
    game_key_state_t key_s;
    game_key_state_t key_d;
} game_keyboard_state_t;

typedef struct
{
    game_keyboard_state_t keyboard_state;
} game_input_t;

internal void game_render(game_framebuffer_t *game_framebuffer,
                          game_input_t *game_input);

#endif
