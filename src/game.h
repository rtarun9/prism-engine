#ifndef __GAME_H__
#define __GAME_H__

// All platform - independent game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "common.h"

#include "arena_allocator.h"
#include "custom_math.h"
#include "renderer.h"
#include "tile_map.h"

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
} game_memory_t;

// NOTE: There are 2 'types' of entites in the game, based on how frequently
// they update. high_freq_entities are those that are update as frequently as
// possible. The player and entites around the player fall into this category.
// low_freq_entities are those that are not updated as frequently as player.
// high freq entities are in a different coordinate system (float coord system),
// where the camera center is origin.
// low freq entites are in tile map coord system.
// Note that even if a entity is high freq, it can still get some of its data
// (like width and height that is common for all entity types) from the low freq
// entity struct.

#define MAX_NUMBER_OF_ENTITIES 512

typedef enum
{
    game_entity_type_wall,
    game_entity_type_player,
} game_entity_type_t;

typedef enum
{
    game_entity_does_not_exist,
    game_entity_low_freq,
    game_entity_high_freq,
} game_entity_states_t;

typedef struct
{
    vector2_t position;
} game_high_freq_entity_t;

typedef struct
{
    game_tile_map_position_t tile_map_position;
    vector2_t dimensions;
} game_low_freq_entity_t;

typedef struct
{
    game_low_freq_entity_t *low_freq_entity;
    game_high_freq_entity_t *high_freq_entity;
} game_entity_t;

typedef struct
{
    f32 tile_dimension;
    game_tile_chunk_t *tile_chunks;

    game_entity_states_t entity_states[MAX_NUMBER_OF_ENTITIES];
    game_high_freq_entity_t high_freq_entities[MAX_NUMBER_OF_ENTITIES];
    game_low_freq_entity_t low_freq_entities[MAX_NUMBER_OF_ENTITIES];
    game_entity_type_t entity_type[MAX_NUMBER_OF_ENTITIES];

    u32 index_of_last_created_entity;

    u32 player_entity_index;
    game_tile_map_position_t camera_world_position;
} game_world_t;

typedef struct
{
    i64 cycles;
    u64 hit_count;
} game_counter_t;

typedef enum
{
    game_update_and_render_counter,
    game_get_world_position,
    game_total_counters
} game_counter_types_t;

// #def's for perf counters.
#ifdef PRISM_INTERNAL

#ifdef COMPILER_MSVC
#define BEGIN_GAME_COUNTER(id) i64 counter_##id = __rdtsc();

#define END_GAME_COUNTER(id)                                                   \
    global_game_counters[id].cycles = __rdtsc() - counter_##id;                \
    global_game_counters[id].hit_count++;

#define CLEAR_GAME_COUNTERS()                                                  \
    for (i32 i = 0; i < game_total_counters; i++)                              \
    {                                                                          \
        global_game_counters[i].hit_count = 0;                                 \
        global_game_counters[i].cycles = 0;                                    \
    }

#else
#error "Compilers other than MSVC have not been setup and configured!!!"
#endif

#else
#define BEGIN_GAME_COUNTER(id)
#define END_GAME_COUNTER(id)
#define CLEAR_GAME_COUNTERS()                                                  \
    {                                                                          \
    }
#endif

typedef struct
{

    f32 pixels_to_meters;
    vector2_t player_velocity;

    game_world_t game_world;
    arena_allocator_t memory_arena;

    game_texture_t player_texture;

    game_counter_t game_counters[game_total_counters];

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

#define FUNC_GAME_UPDATE_AND_RENDER(name)                                      \
    void name(game_memory_t *restrict game_memory_allocator,                   \
              game_framebuffer_t *restrict game_framebuffer,                   \
              game_input_t *restrict game_input,                               \
              platform_services_t *restrict platform_services)

// Core game functions that will be called by the platform layer.
__declspec(dllexport) FUNC_GAME_UPDATE_AND_RENDER(game_update_and_render);

#endif
