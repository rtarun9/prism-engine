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

// NOTE: State changed is used to determine if the state of the key
// press has changed since last frame (for example : the value will be true if
// key was down last frame and no is released).
typedef struct
{
    b8 state_changed;
    b8 is_key_down;
} game_key_state_t;

// FIX: Create an array for this in the future?
typedef struct
{
    game_key_state_t key_w;
    game_key_state_t key_a;
    game_key_state_t key_s;
    game_key_state_t key_d;
    game_key_state_t key_space;
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

// NOTE: Game state will be stored *in* the permanent section of game
// memory.
typedef struct
{
    f32 blue_offset;
    f32 green_offset;

    i16 player_x;
    i16 player_y;

    b8 is_initialized;
} game_state_t;

// Services / interfaces provided by the platform layer to the game.
typedef struct
{
    u8 *file_content_buffer;
    u64 file_content_size;
} platform_file_read_result_t;

// NOTE: Why are these functions.. typedefs?
// For a few reasons. First, the function's will be created in the platform
// files, note in the game. A compilation error will arise if the function
// signations are alone defined. Second, the func pointers to these platform
// services will be provided along with the 'platform services' struct.
// The #defines are used simply because it will make defining the stub easier in
// the platform layer code.

// TODO: Check if the memory allocator should just be renamed to memory and
// should the func pointers to platform services be included in that struct?

#define FUNC_PLATFORM_READ_ENTIRE_FILE(name)                                   \
    platform_file_read_result_t name(const char *file_path)
typedef FUNC_PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file_t);

// FIX: Return a file handle instead? Does it really make sense to
// have the function name as 'close file' but take as input the file content
// buffer?

#define FUNC_PLATFORM_CLOSE_FILE(name) void name(u8 *file_content_buffer)
typedef FUNC_PLATFORM_CLOSE_FILE(platform_close_file_t);

#define FUNC_PLATFORM_WRITE_TO_FILE(name)                                      \
    void name(const char *file_path, u8 *file_content_buffer,                  \
              u64 file_content_size)
typedef FUNC_PLATFORM_WRITE_TO_FILE(platform_write_to_file_t);

typedef struct
{
    platform_read_entire_file_t *platform_read_entire_file;
    platform_close_file_t *platform_close_file;
    platform_write_to_file_t *platform_write_to_file;
} platform_services_t;

#define FUNC_GAME_RENDER(name)                                                 \
    void name(game_memory_allocator_t *game_memory_allocator,                  \
              game_framebuffer_t *game_framebuffer, game_input_t *game_input,  \
              platform_services_t *platform_services)

// Core game functions that will be called by the platform layer.
// HACK: Not entire sure how I like this syntax... Better to keep the
// FUNC_GAME_RENDER macro in expanded form and have the #define just for the
// platform layer?
__declspec(dllexport) FUNC_GAME_RENDER(game_render);

#endif
