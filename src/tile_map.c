#include "tile_map.h"

// The 'corrected' tile and tile chunk indices.
// In cases where the tile_x is more than the possible number, the chunk_x must
// be modified. Such cases are handled by the 'get_corrected_tiel_indices'
// function.
typedef struct
{
    u32 tile_x;
    u32 tile_y;

    u32 chunk_x;
    u32 chunk_y;
} corrected_tile_indices_t;

internal corrected_tile_indices_t get_corrected_tile_indices(i32 tile_x,
                                                             i32 tile_y,
                                                             i32 chunk_x,
                                                             i32 chunk_y)
{
    corrected_tile_indices_t result = {0};

    if (tile_x < 0)
    {
        // How far should chunk_x go down by? Not 1, in case tile_x is negative
        // and abs(tile_x) > number of tiles per chunk. Similar logic applies
        // for y.
        i32 chunks_to_shift =
            -floor_f32_to_i32(tile_x / (f32)NUMBER_OF_TILES_PER_CHUNK_X);
        chunk_x -= chunks_to_shift;
        tile_x += chunks_to_shift * NUMBER_OF_TILES_PER_CHUNK_X;
    }
    else if (tile_x >= NUMBER_OF_TILES_PER_CHUNK_X)
    {
        i32 chunks_to_shift =
            floor_f32_to_i32(tile_x / (f32)NUMBER_OF_TILES_PER_CHUNK_X);
        chunk_x += chunks_to_shift;
        tile_x -= chunks_to_shift * NUMBER_OF_TILES_PER_CHUNK_X;
    }

    if (tile_y < 0)
    {
        i32 chunks_to_shift =
            -floor_f32_to_i32(tile_y / (f32)NUMBER_OF_TILES_PER_CHUNK_Y);
        chunk_y -= chunks_to_shift;
        tile_y += chunks_to_shift * NUMBER_OF_TILES_PER_CHUNK_Y;
    }
    else if (tile_y >= NUMBER_OF_TILES_PER_CHUNK_Y)
    {
        i32 chunks_to_shift =
            floor_f32_to_i32(tile_y / (f32)NUMBER_OF_TILES_PER_CHUNK_Y);
        chunk_y += chunks_to_shift;
        tile_y -= chunks_to_shift * NUMBER_OF_TILES_PER_CHUNK_Y;
    }

    if (chunk_x < 0)
    {
        chunk_x = 0;
        tile_x = 0;
    }
    else if (chunk_x >= NUMBER_OF_CHUNKS_IN_WORLD_X)
    {
        chunk_x = NUMBER_OF_CHUNKS_IN_WORLD_X - 1;
    }

    if (chunk_y < 0)
    {
        chunk_y = 0;
        tile_y = 0;
    }
    else if (chunk_y >= NUMBER_OF_CHUNKS_IN_WORLD_Y)
    {
        chunk_y = NUMBER_OF_CHUNKS_IN_WORLD_Y - 1;
    }

    result.tile_x = tile_x;
    result.tile_y = tile_y;

    result.chunk_x = chunk_x;
    result.chunk_y = chunk_y;

    return result;
}

// Assumes that the tile x / y and chunk x / y values are corrected.
// NOTE: In the world, postive y goes up, but in memory y goes in the opposite
// direction. The function handles this case by modifying y to
// NUMBER_OF_TILES_PER_CHUNK_Y - y.
internal u32 get_value_in_tile_chunk(game_tile_chunk_t *restrict tile_chunk,
                                     u32 x, u32 y)
{
    ASSERT(tile_chunk);

    ASSERT(x < NUMBER_OF_TILES_PER_CHUNK_X);
    ASSERT(y < NUMBER_OF_TILES_PER_CHUNK_Y);

    if (tile_chunk->tile_chunk == NULL)
    {
        return CHUNK_NOT_LOADED;
    }

    return tile_chunk->tile_chunk[x + (NUMBER_OF_TILES_PER_CHUNK_Y - 1 - y) *
                                          NUMBER_OF_TILES_PER_CHUNK_X];
}

internal void set_value_in_tile_chunk(game_tile_chunk_t *restrict tile_chunk,
                                      u32 x, u32 y, u32 value)
{
    ASSERT(tile_chunk);

    ASSERT(x < NUMBER_OF_TILES_PER_CHUNK_X);
    ASSERT(y < NUMBER_OF_TILES_PER_CHUNK_Y);

    if (tile_chunk->tile_chunk == NULL)
    {
        return;
    }

    tile_chunk->tile_chunk[x + (NUMBER_OF_TILES_PER_CHUNK_Y - 1 - y) *
                                   NUMBER_OF_TILES_PER_CHUNK_X] = value;
}

// Returns CHUNK_NOT_LOADED if the chunk isn't loaded :)
// NOTE: In the world, postive y goes up, but in memory y goes in the opposite
// direction. The function handles this case by modifying y to
// NUMBER_OF_TILES_PER_CHUNK_Y - y.
internal u32
get_value_of_tile_in_chunks(game_tile_chunk_t *restrict tile_chunks,
                            corrected_tile_indices_t tile_indices)
{
    ASSERT(tile_chunks);

    ASSERT(tile_indices.chunk_x < NUMBER_OF_CHUNKS_IN_WORLD_X);
    ASSERT(tile_indices.chunk_y < NUMBER_OF_CHUNKS_IN_WORLD_Y);

    game_tile_chunk_t *tile_chunk =
        &tile_chunks[tile_indices.chunk_x +
                    (NUMBER_OF_CHUNKS_IN_WORLD_Y - 1 - tile_indices.chunk_y) *
                        NUMBER_OF_CHUNKS_IN_WORLD_X];

    ASSERT(tile_chunk);

    return get_value_in_tile_chunk(tile_chunk, tile_indices.tile_x,
                                   tile_indices.tile_y);
}

internal game_tile_chunk_t *get_tile_chunk(
    game_tile_chunk_t *restrict tile_chunks, u32 x, u32 y)
{
    ASSERT(tile_chunks);

    ASSERT(x < NUMBER_OF_CHUNKS_IN_WORLD_X);
    ASSERT(y < NUMBER_OF_CHUNKS_IN_WORLD_Y);

    game_tile_chunk_t *tile_chunk =
        &tile_chunks[x + (NUMBER_OF_CHUNKS_IN_WORLD_Y - 1 - y) *
                            NUMBER_OF_CHUNKS_IN_WORLD_X];

    return tile_chunk;
}

internal game_tile_map_position_t get_game_tile_map_position(
    f32 tile_dimension, vector2_t tile_relative_offset,

    i32 tile_indices_x, i32 tile_indices_y)
{
    BEGIN_GAME_COUNTER(game_get_world_position);

    game_tile_map_position_t result = {0};
    result.tile_index_x = 0;
    result.tile_index_y = 0;

    i32 x_offset = floor_f32_to_i32(tile_relative_offset.x / tile_dimension);
    i32 y_offset = floor_f32_to_i32(tile_relative_offset.y / tile_dimension);

    result.tile_relative_offset.x =
        tile_relative_offset.x - x_offset * tile_dimension;
    result.tile_relative_offset.y =
        tile_relative_offset.y - y_offset * tile_dimension;

    ASSERT(result.tile_relative_offset.x >= 0 &&
           result.tile_relative_offset.x <= tile_dimension);
    ASSERT(result.tile_relative_offset.y >= 0 &&
           result.tile_relative_offset.y <= tile_dimension);

    i32 tile_x = x_offset + GET_TILE_INDEX(tile_indices_x);
    i32 tile_y = y_offset + GET_TILE_INDEX(tile_indices_y);

    corrected_tile_indices_t corrected_tile_indices =
        get_corrected_tile_indices(tile_x, tile_y,
                                   GET_TILE_CHUNK_INDEX(tile_indices_x),
                                   GET_TILE_CHUNK_INDEX(tile_indices_y));

    // Check if there is a tilemap where the player wants to head to.
    result.tile_index_x =
        SET_TILE_INDEX(result.tile_index_x, corrected_tile_indices.tile_x);

    result.tile_index_y =
        SET_TILE_INDEX(result.tile_index_y, corrected_tile_indices.tile_y);

    result.tile_index_x = SET_TILE_CHUNK_INDEX(result.tile_index_x,
                                               corrected_tile_indices.chunk_x);

    result.tile_index_y = SET_TILE_CHUNK_INDEX(result.tile_index_y,
                                               corrected_tile_indices.chunk_y);

    END_GAME_COUNTER(game_get_world_position);

    return result;
}

// Computes a - b.
game_tile_map_position_difference_t tile_map_position_difference(
    game_tile_map_position_t *restrict a, game_tile_map_position_t *restrict b)
{
    game_tile_map_position_difference_t result = {0};

    result.tile_x_diff =
        GET_TILE_INDEX(a->tile_index_x) - GET_TILE_INDEX(b->tile_index_x);
    result.tile_y_diff =
        GET_TILE_INDEX(a->tile_index_y) - GET_TILE_INDEX(b->tile_index_y);

    result.chunk_x_diff = GET_TILE_CHUNK_INDEX(a->tile_index_x) -
                          GET_TILE_CHUNK_INDEX(b->tile_index_x);

    result.chunk_y_diff = GET_TILE_CHUNK_INDEX(a->tile_index_y) -
                          GET_TILE_CHUNK_INDEX(b->tile_index_y);

    return result;
}

// Returns 1 if collision has occured, and 0 if it did not.
internal b32 check_point_and_tile_chunk_collision(
    game_world_t *const world, game_tile_map_position_t position)
{
    ASSERT(world);

    i32 chunk_x = GET_TILE_CHUNK_INDEX(position.tile_index_x);
    i32 chunk_y = GET_TILE_CHUNK_INDEX(position.tile_index_y);

    i32 tile_x = GET_TILE_INDEX(position.tile_index_x);
    i32 tile_y = GET_TILE_INDEX(position.tile_index_y);

    corrected_tile_indices_t tile_indices =
        get_corrected_tile_indices(tile_x, tile_y, chunk_x, chunk_y);

    return get_value_of_tile_in_chunks(world->tile_chunks, tile_indices);
}
