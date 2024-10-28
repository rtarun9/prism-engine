#ifndef __GAME_H__
#define __GAME_H__

#include "common.h"

typedef struct
{
    u32 *framebuffer_memory;

    u32 width;
    u32 height;
} game_offscreen_buffer_t;

typedef struct
{
    // buffer is an in-out parameter.
    i16 *buffer;
    u32 samples_per_second;
    u32 samples_to_output;
} game_sound_buffer_t;

typedef struct
{
    b32 is_key_down;
    u32 state_transition_count;
} game_key_state_t;

typedef struct
{
    game_key_state_t key_w;
    game_key_state_t key_s;
} game_keyboard_state_t;

typedef struct
{
    game_keyboard_state_t keyboard_state;
} game_input_t;

#endif
