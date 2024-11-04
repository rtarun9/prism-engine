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
    game_key_state_t key_a;
    game_key_state_t key_s;
    game_key_state_t key_d;
} game_keyboard_state_t;

typedef struct
{
    game_keyboard_state_t keyboard_state;
    f32 delta_time;
} game_input_t;

typedef struct
{
    i32 x_shift;
    i32 y_shift;

    f32 t_sine;
    u32 frequency;

    b32 is_initialized;
} game_state_t;

typedef struct
{
    u8 *permanent_memory_block;
    u64 permanent_memory_block_size;
} game_memory_t;

// Interfaces provided by the platform to the game.

internal u8 *platform_read_file(const char *file_name);
internal void platform_close_file(u8 *file_buffer);
internal b32 platform_write_to_file(const char *string, const char *file_name);

#endif
