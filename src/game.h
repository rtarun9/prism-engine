#ifndef __GAME_H__
#define __GAME_H__

// All platform - independent game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "common.h"

#include "arena_allocator.h"

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
    b32 state_changed;
    b32 is_key_down;
} game_key_state_t;

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
    f32 dt_for_frame;
} game_input_t;

typedef struct
{
    u64 permanent_memory_size;
    u8 *permanent_memory;
} game_memory_allocator_t;

#define GET_TILE_INDEX(x) (x >> 24)
#define GET_TILE_CHUNK_INDEX(x) (x & 0x00ffffff)

// X is the tile_index_A, and y is the value.
#define SET_TILE_INDEX(x, y) (x & 0x00ffffff | y << 24)
#define SET_TILE_CHUNK_INDEX(x, y) (x & 0xff000000 | y & 0x00ffffff)

typedef struct
{
    // The fp offset within a tile.
    f32 tile_relative_x;
    f32 tile_relative_y;

    // The 8 MSB bits are the index of tile within tile chunk.
    // The other 24 bits are the index of the tile chunk within the world.
    u32 tile_index_x;
    u32 tile_index_y;
} world_position_t;

typedef struct
{
    u32 *tile_chunk;
} game_tile_chunk_t;

#define NUMBER_OF_TILES_PER_CHUNK_X 25
#define NUMBER_OF_TILES_PER_CHUNK_Y 19

#define NUMBER_OF_CHUNKS_IN_WORLD_X 64
#define NUMBER_OF_CHUNKS_IN_WORLD_Y 64

// NOTE: Game state will be stored *in* the permanent section of game memory.
typedef struct
{
    // Dimensions of each tile in the tile chunk.
    f32 tile_width;
    f32 tile_height;

    game_tile_chunk_t *tile_chunks;
} game_world_t;

typedef struct
{
    world_position_t player_world_position;

    f32 pixels_to_meters;

    game_world_t *game_world;
    arena_allocator_t memory_arena;

    // NOTE: For bitmap loading and testing purposes only.
    u32 bitmap_mask_r;
    u32 bitmap_mask_g;
    u32 bitmap_mask_b;
    u32 bitmap_mask_a;
    u32 bitmap_height;
    u32 bitmap_width;
    u32 *bitmap_pointer;

    u32 is_initialized;
} game_state_t;

// Services / interfaces provided by the platform layer to the game.
typedef struct
{
    u8 *file_content_buffer;
    u64 file_content_size;
} platform_file_read_result_t;

// NOTE: Why are these functions.. typedefs?
// For a few reasons. First, the functions will be created in the platform
// files, not in the game. A compilation error will arise if the function
// signatures are alone defined. Second, the func pointers to these platform
// services will be provided along with the 'platform services' struct.
// The #defines are used simply because it will make defining the stub easier in
// the platform layer code.

#define FUNC_PLATFORM_READ_ENTIRE_FILE(name)                                   \
    platform_file_read_result_t name(const char *file_path)
typedef FUNC_PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file_t);

#define FUNC_PLATFORM_CLOSE_FILE(name) void name(u8 *file_content_buffer)
typedef FUNC_PLATFORM_CLOSE_FILE(platform_close_file_t);

#define FUNC_PLATFORM_WRITE_TO_FILE(name)                                      \
    void name(const char *restrict file_path,                                  \
              u8 *restrict file_content_buffer, u64 file_content_size)
typedef FUNC_PLATFORM_WRITE_TO_FILE(platform_write_to_file_t);

typedef struct
{
    platform_read_entire_file_t *platform_read_entire_file;
    platform_close_file_t *platform_close_file;
    platform_write_to_file_t *platform_write_to_file;
} platform_services_t;

#define FUNC_GAME_RENDER(name)                                                 \
    void name(game_memory_allocator_t *restrict game_memory_allocator,         \
              game_framebuffer_t *restrict game_framebuffer,                   \
              game_input_t *restrict game_input,                               \
              platform_services_t *restrict platform_services)

// Core game functions that will be called by the platform layer.
__declspec(dllexport) FUNC_GAME_RENDER(game_render);

#endif
