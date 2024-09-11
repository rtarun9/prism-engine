#ifndef __GAME_H__
#define __GAME_H__

// All platform - independent game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "common.h"

#include "arena_allocator.h"
#include "custom_math.h"
#include "renderer.h"

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

    game_key_state_t key_up;
    game_key_state_t key_down;

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

// NOTE: The chunks have a range of -2^53 to 2^53. There is no explicit 'checks'
// done for chunk index. Instead, if the player approaches these chunk
// coordinates a wall will be place so user cannot go out of bounds. However, it
// is probably not possible that the user will ever reach this chunk from the
// origin. Or alternatively, no checks can be done and the player will just wrap
// around the world, but this has to be checked.
#define CHUNKS_EXTENT_IN_WORLD_X (1 << 53)
#define CHUNKS_EXTENT_IN_WORLD_Y (1 << 53)

// All chunks share the same dimension in terms of meters.
// The center of a chunk is CHUNK_DIMENSIONS_IN_METER_X / 2, Y/2,and extents
// within a chunk go from [0, CHUNK_DIMENSION_IN_METERS_X - 1], etc.
#define CHUNK_DIMENSION_IN_METERS_X (17)
#define CHUNK_DIMENSION_IN_METERS_Y (11)

// TODO: This is subject to change, which is why is why game_position_t is not
// just a typedef for v2f64_t.
typedef struct
{
    // Chunk index can be easily obtained from position and the chunk
    // dimensions.
    v2f64_t position;
} game_position_t;

// NOTE: There are 2 'types' of entites in the game, based on how frequently
// they update. high_freq_entities are those that are update as frequently as
// possible. The player and entites around the player fall into this category.
// low_freq_entities are those that are not updated as frequently as player.

// Familiar is a entity that follows the player around.
typedef enum
{
    game_entity_type_none,
    game_entity_type_wall,
    game_entity_type_player,
    game_entity_type_familiar,
} game_entity_type_t;

typedef enum
{
    game_entity_state_does_not_exist,
    game_entity_state_low_freq,
    game_entity_state_high_freq,
} game_entity_state_t;

typedef struct
{
    game_position_t position;
    v2f32_t dimension;
    b32 collides;
    game_entity_type_t entity_type;

    u32 total_hitpoints;
    u32 hitpoints;
} game_entity_t;

// To partition entities spatially, a hash map of chunks is created, where
// each chunk consist of its index, and a list of the low frequenty entity
// indexes that are present in that chunk. This will make querying for
// entities in certain chunks very efficient.
// Since the number of low frequency entities is not constant, each chunk has a
// chunk entity block linked list.
typedef struct game_chunk_entity_block_t
{
    u32 low_freq_entity_indices[32];
    u32 low_freq_entity_count;

    struct game_chunk_entity_block_t *next_chunk_entity_block;
} game_chunk_entity_block_t;

// NOTE: A null chunk entity block will be used to check if chunk is invalid or
// not.
typedef struct game_chunk_t
{
    i64 chunk_index_x;
    i64 chunk_index_y;

    game_chunk_entity_block_t *chunk_entity_block;

    // Since game chunk is a SLL (single linked list), a next pointer is
    // required.
    struct game_chunk_t *next_chunk;

} game_chunk_t;

typedef struct
{
    game_entity_t high_freq_entities[128];
    u32 high_freq_entity_count;

    game_entity_t low_freq_entities[8192];
    u32 low_freq_entity_count;

    u32 player_high_freq_entity_index;

    game_chunk_t chunk_hash_map[256];

    game_position_t camera_world_position;
} game_world_t;

typedef struct
{
    i64 cycles;
    u64 hit_count;
} game_counter_t;

typedef enum
{
    game_update_and_render_counter,
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
    v2f32_t player_velocity;

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
