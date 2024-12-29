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

// Tile chunk related information.
#define TILE_CHUNK_DIM 256u

// NOTE: The absolute tile index consist of 2 parts : The lower 8 bits
// constitute the index of tile within the chunk, while the higher 24 bits are
// the index of chunk in the world.
#define GET_CHUNK_INDEX_IN_WORLD(x) (((x) & 0xffffff00) > 0xff)
#define GET_TILE_INDEX_IN_CHUNK(x) ((x) & 0x000000ff)

#define SET_CHUNK_INDEX(tile_pos, chunk_index)                                 \
    ((tile_pos & 0x000000ff | (chunk_index << 8)))

#define SET_TILE_INDEX_IN_CHUNK(tile_pos, tile_index)                          \
    ((tile_pos & 0xffffff00 | (tile_index & 0x000000ff)))

#define INVALID_TILE_VALUE 0xffffffff

typedef struct
{
    u32 tiles[TILE_CHUNK_DIM][TILE_CHUNK_DIM];
} game_tile_chunk_t;

typedef struct
{
    game_tile_chunk_t *tile_chunks;

    u32 tile_chunk_count_x;
    u32 tile_chunk_count_y;

    // Tile width and height are in meters.
    u32 tile_width;
    u32 tile_height;
} game_world_t;

typedef struct
{
    // The absolute tile index into the (toroidal) world, which is unbounded
    u32 abs_tile_index_x;
    u32 abs_tile_index_y;

    // fp offset into a particular tile.
    f32 tile_rel_x;
    f32 tile_rel_y;
} game_world_position_t;

typedef struct
{
    b32 is_initialized;

    game_world_position_t player_position;

    // Player dimensions in meters.
    f32 player_width;
    f32 player_height;

    // The number of pixels that makes up a meter.
    u32 pixels_per_meter;

    game_world_t game_world;

} game_state_t;

typedef struct
{
    u8 *permanent_memory_block;
    u64 permanent_memory_block_size;
} game_memory_t;

// Interfaces provided by the platform to the game.
#define DEF_PLATFORM_READ_FILE_FUNC(name) u8 *name(const char *file_name)
typedef DEF_PLATFORM_READ_FILE_FUNC(platform_read_file_t);

#define DEF_PLATFORM_CLOSE_FILE_FUNC(name) void name(u8 *file_buffer)
typedef DEF_PLATFORM_CLOSE_FILE_FUNC(platform_close_file_t);

#define DEF_PLATFORM_WRITE_TO_FILE_FUNC(name)                                  \
    b32 name(const char *string, const char *file_name)
typedef DEF_PLATFORM_WRITE_TO_FILE_FUNC(platform_write_to_file_t);

typedef struct
{
    platform_read_file_t *read_file;
    platform_close_file_t *close_file;
    platform_write_to_file_t *write_to_file;
} game_platform_services_t;

#define DEF_GAME_UPDATE_AND_RENDER_FUNC(name)                                  \
    void name(game_offscreen_buffer_t *const restrict game_offscreen_buffer,   \
              game_input_t *const restrict game_input,                         \
              game_memory_t *const restrict game_memory,                       \
              game_platform_services_t *const restrict platform_services)

typedef DEF_GAME_UPDATE_AND_RENDER_FUNC(game_update_and_render_t);

#endif
