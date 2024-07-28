#ifndef __GAME_H__
#define __GAME_H__

// All platform - independent game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "common.h"

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
    b8 state_changed;
    b8 is_key_down;
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

typedef struct
{
    u64 permanent_memory_size;
    u8 *permanent_memory;
} game_memory_allocator_t;

// note(rtarun9) : Game state will be stored *in* the permanent section of game
// memory.
typedef struct
{
    f32 blue_offset;
    f32 green_offset;
    b8 is_initialized;
} game_state_t;

internal void game_render(game_memory_allocator_t *game_memory_allocator,
                          game_framebuffer_t *game_framebuffer,
                          game_input_t *game_input);

// Services / interfaces provided by the platform layer to the game.
typedef struct
{
    u8 *file_content_buffer;
    u64 file_content_size;
} platform_file_read_result_t;

internal platform_file_read_result_t
platform_read_entire_file(const char *file_path);

// note(rtarun9) : Return a file handle instead? Does it really make sense to
// have the function name as 'close file' but take as input the file content
// buffer?
internal void platform_close_file(u8 *file_content_buffer);

internal void platform_write_to_file(const char *file_path,
                                     u8 *file_content_buffer,
                                     u64 file_content_size);

#endif
