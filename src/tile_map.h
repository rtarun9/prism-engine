#ifndef __TILE_MAP_H__
#define __TILE_MAP_H__

#include "custom_math.h"
#include "custom_intrinsics.h"

// The upper 8 bits of tile index are reserved for the tile index, the other 24
// bits are reserved for chunk index.
#define GET_TILE_INDEX(x) (((x) >> 24))
#define GET_TILE_CHUNK_INDEX(x) (((x) & 0x00ffffff))

// X is the tile_index_A, and y is the value.
#define SET_TILE_INDEX(x, y) (((x) & 0x00ffffff | (y) << 24))
#define SET_TILE_CHUNK_INDEX(x, y) (((x) & 0xff000000 | (y) & 0x00ffffff))

#define NUMBER_OF_TILES_PER_CHUNK_X 15
#define NUMBER_OF_TILES_PER_CHUNK_Y 11

#define NUMBER_OF_CHUNKS_IN_WORLD_X 4
#define NUMBER_OF_CHUNKS_IN_WORLD_Y 4

#define CHUNK_NOT_LOADED (u32)(-1)
#define TILE_WALL 1
#define TILE_EMPTY 0

typedef struct
{
    // The fp offset within a tile.
    vector2_t tile_relative_offset;

    // The 8 MSB bits are the index of tile within tile chunk.
    // The other 24 bits are the index of the tile chunk within the world.
    u32 tile_index_x;
    u32 tile_index_y;
} game_tile_map_position_t;

typedef struct
{
    u32 *tile_chunk;
} game_tile_chunk_t;
#endif
