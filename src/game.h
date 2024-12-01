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

#define TILE_MAP_DIM_X 17u
#define TILE_MAP_DIM_Y 9u

#define INVALID_TILE_MAP_VALUE 0xffffffff

typedef struct
{
    u32 tile_map[TILE_MAP_DIM_Y][TILE_MAP_DIM_X];

    f32 top_left_x;
    f32 top_left_y;

    u32 tile_width;
    u32 tile_height;
} game_tile_map_t;

typedef struct
{
    game_tile_map_t *tile_maps;

    u32 tile_map_count_x;
    u32 tile_map_count_y;
} game_world_t;

typedef struct
{
    i32 x_shift;
    i32 y_shift;

    f32 t_sine;
    u32 frequency;

    b32 is_initialized;

    f32 player_x;
    f32 player_y;

    u32 player_width;
    u32 player_height;

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
