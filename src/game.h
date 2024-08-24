#ifndef __GAME_H__
#define __GAME_H__

// All platform - independent game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "common.h"

#include "arena_allocator.h"
#include "custom_math.h"

typedef struct
{
    u8 *backbuffer_memory;
    u32 width;
    u32 height;
} game_framebuffer_t;

// NOTE: State changed is used to determine if the state of the key
// press has changed since last frame.
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

#define GET_TILE_INDEX(x) (((x) >> 24))
#define GET_TILE_CHUNK_INDEX(x) (((x) & 0x00ffffff))

// X is the tile_index_A, and y is the value.
#define SET_TILE_INDEX(x, y) (((x) & 0x00ffffff | (y) << 24))
#define SET_TILE_CHUNK_INDEX(x, y) (((x) & 0xff000000 | (y) & 0x00ffffff))

#define NUMBER_OF_TILES_PER_CHUNK_X 25
#define NUMBER_OF_TILES_PER_CHUNK_Y 19

#define NUMBER_OF_CHUNKS_IN_WORLD_X 64
#define NUMBER_OF_CHUNKS_IN_WORLD_Y 64

#define CHUNK_NOT_LOADED (u32)(-1)

typedef struct
{
    // The fp offset within a tile.
    vector2_t tile_relative_offset;

    // The 8 MSB bits are the index of tile within tile chunk.
    // The other 24 bits are the index of the tile chunk within the world.
    u32 tile_index_x;
    u32 tile_index_y;
} game_world_position_t;

typedef struct
{
    u32 *tile_chunk;
} game_tile_chunk_t;

// NOTE: Game state will be stored *in* the permanent section of game memory.
typedef struct
{
    // Dimensions of each tile in the tile chunk.
    vector2_t tile_dimensions;

    game_tile_chunk_t *tile_chunks;
} game_world_t;

typedef struct
{
    // Given a pixel color, use the following shift values using (pixel >>
    // shift) & 0xff to extract individual channel values.
    u32 red_shift;
    u32 green_shift;
    u32 blue_shift;
    u32 alpha_shift;

    u32 height;
    u32 width;

    u32 *pointer;
} game_texture_t;

typedef struct
{
    game_world_position_t player_world_position;
    game_world_position_t camera_world_position;

    f32 pixels_to_meters;
    vector2_t player_velocity;

    game_world_t *game_world;
    arena_allocator_t memory_arena;

    game_texture_t test_texture;
    game_texture_t player_texture;

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
