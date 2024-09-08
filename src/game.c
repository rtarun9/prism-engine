#include "game.h"

global_variable game_counter_t *global_game_counters = NULL;

#include "arena_allocator.c"
#include "arena_allocator.h"

#include "custom_intrinsics.h"
#include "custom_math.h"

#include "renderer.h"

typedef struct
{
    i64 chunk_x;
    i64 chunk_y;
} game_chunk_index_pair_t;

game_chunk_index_pair_t get_chunk_index_from_position(game_position_t position)
{
    game_chunk_index_pair_t result = {0};

    result.chunk_x = floor_f64_to_i64((position.position.x) /
                                      ((f64)CHUNK_DIMENSION_IN_METERS_X));

    result.chunk_y = floor_f64_to_i64((position.position.y) /
                                      ((f64)CHUNK_DIMENSION_IN_METERS_Y));

    return result;
}
game_chunk_t *get_game_chunk(game_world_t *game_world, i64 chunk_index_x,
                             i64 chunk_index_y)
{
    // TODO: When collisions become a problem, the hash function should probably
    // be updated.
    // For simplicity, number of entries in the chunk hash map array is assumed
    // to be powers of 2. This will make getting a valid index from hash value
    // easy, as power of 2 - 1 gives a mask.
    u32 hash_value = (chunk_index_x * 3 + chunk_index_y * 66) &
                     (ARRAY_COUNT(game_world->chunk_hash_map) - 1);

    game_chunk_t *chunk = &game_world->chunk_hash_map[hash_value];
    while (chunk && (chunk->chunk_index_x != chunk_index_x &&
                     chunk->chunk_index_y != chunk_index_y))
    {
        chunk = chunk->next_chunk;
    }

    // If chunk is null, then it has NOT been set.
    ASSERT(chunk);
    ASSERT(chunk->chunk_index_x == chunk_index_x &&
           chunk->chunk_index_y == chunk_index_y);

    return chunk;
}

// Call this function to create a new game chunk and add to the hash map.
game_chunk_t *set_game_chunk(game_world_t *game_world, i64 chunk_index_x,
                             i64 chunk_index_y,
                             arena_allocator_t *arena_allocator)
{
    ASSERT(arena_allocator);

    // TODO: When collisions become a problem, the hash function should probably
    // be updated.
    // For simplicity, number of entries in the chunk hash map array is assumed
    // to be powers of 2. This will make getting a valid index from hash value
    // easy, as power of 2 - 1 gives a mask.
    u32 hash_value = (chunk_index_x * 3 + chunk_index_y * 66) &
                     (ARRAY_COUNT(game_world->chunk_hash_map) - 1);

    game_chunk_t *chunk = &game_world->chunk_hash_map[hash_value];

    // Check to see if the head is empty (i.e the sentinel value in
    // chunk_hash_map).
    if (!chunk->chunk_entity_block)
    {
        chunk->chunk_entity_block =
            (game_chunk_entity_block_t *)arena_alloc_default_aligned(
                arena_allocator, sizeof(game_chunk_entity_block_t));

        chunk->chunk_index_x = chunk_index_x;
        chunk->chunk_index_y = chunk_index_y;

        return chunk;
    }
    else
    {
        // Find the tail element and set its next chunk pointer to a new chunk.
        while (chunk->next_chunk)
        {
            chunk = chunk->next_chunk;
        }

        ASSERT(chunk && !chunk->next_chunk);

        game_chunk_t *new_chunk = (game_chunk_t *)arena_alloc_default_aligned(
            arena_allocator, sizeof(game_chunk_t));

        new_chunk->chunk_entity_block =
            (game_chunk_entity_block_t *)arena_alloc_default_aligned(
                arena_allocator, sizeof(game_chunk_entity_block_t));

        new_chunk->chunk_index_x = chunk_index_x;
        new_chunk->chunk_index_y = chunk_index_y;

        chunk->next_chunk = new_chunk;

        return new_chunk;
    }

    ASSERT("Code should never reach here!!!");
    ASSERT(0);

    return NULL;
}

// Place entity in the correct chunk's low freq entity block.
void place_low_freq_entity_in_chunk(
    game_world_t *restrict game_world, u32 entity_low_freq_index,
    game_chunk_index_pair_t chunk_to_place_entity_in,
    arena_allocator_t *restrict arena_allocator)
{
    i64 chunk_index_x = chunk_to_place_entity_in.chunk_x;
    i64 chunk_index_y = chunk_to_place_entity_in.chunk_y;

    game_chunk_t *chunk =
        get_game_chunk(game_world, chunk_index_x, chunk_index_y);
    ASSERT(chunk);

    // Now, go over the entity blocks to see if entities low freq index is
    // already in it.
    game_chunk_entity_block_t *chunk_entity_block = chunk->chunk_entity_block;

    while (chunk_entity_block)
    {
        // TODO: Try moving this computation to SIMD.
        for (u32 entity_iter = 0;
             entity_iter < chunk_entity_block->low_freq_entity_count;
             entity_iter++)
        {
            if (chunk_entity_block->low_freq_entity_indices[entity_iter] ==
                entity_low_freq_index)
            {
                return;
            }
        }

        if (chunk_entity_block->next_chunk_entity_block)
        {
            chunk_entity_block = chunk_entity_block->next_chunk_entity_block;
        }
        else
        {
            break;
        }
    }

    // If entity is not in chunk, first remove entity from the previous chunk
    // (if it was in one), and add to new chunk.
    game_chunk_index_pair_t entity_current_chunk_index_pair =
        get_chunk_index_from_position(
            game_world->low_freq_entities[entity_low_freq_index].position);

    game_chunk_entity_block_t *entity_current_chunk_entity_block =
        get_game_chunk(game_world, entity_current_chunk_index_pair.chunk_x,
                       entity_current_chunk_index_pair.chunk_y)
            ->chunk_entity_block;

    while (entity_current_chunk_entity_block)
    {
        // TODO: Try moving this computation to SIMD.
        for (u32 entity_iter = 0;
             entity_iter <
             entity_current_chunk_entity_block->low_freq_entity_count;
             entity_iter++)
        {
            if (entity_current_chunk_entity_block
                    ->low_freq_entity_indices[entity_iter] ==
                entity_low_freq_index)
            {
                // Take the last element from this chunk block, and put it
                // in place of current elements low freq index entry.
                // If this is the only low freq entity in chunk, simply
                // decrement the chunks low freq entity count.
                if (entity_current_chunk_entity_block->low_freq_entity_count ==
                    1)
                {
                    entity_current_chunk_entity_block->low_freq_entity_count--;
                }
                else
                {
                    entity_current_chunk_entity_block
                        ->low_freq_entity_indices[entity_iter] =
                        entity_current_chunk_entity_block
                            ->low_freq_entity_indices
                                [entity_current_chunk_entity_block
                                     ->low_freq_entity_count-- -
                                 1];
                }

                break;
            }
        }

        entity_current_chunk_entity_block =
            entity_current_chunk_entity_block->next_chunk_entity_block;
    }

    // Add entity to new chunk.
    // Create entity block if it does not exist.
    // If the entity block is full, allocate a new one.
    if (!(chunk_entity_block->low_freq_entity_count <
          ARRAY_COUNT(chunk_entity_block->low_freq_entity_indices)))
    {
        // TODO: Maintain a linked list of free entity blocks and re-use. Alloc
        // from memory arena only if that linked list is totally empty.
        game_chunk_entity_block_t *new_chunk_entity_block =
            (game_chunk_entity_block_t *)arena_alloc_default_aligned(
                arena_allocator, sizeof(game_chunk_entity_block_t));

        new_chunk_entity_block->low_freq_entity_count = 0;
        new_chunk_entity_block->next_chunk_entity_block = NULL;

        new_chunk_entity_block->low_freq_entity_indices
            [new_chunk_entity_block->low_freq_entity_count++] =
            entity_low_freq_index;

        chunk_entity_block->next_chunk_entity_block = new_chunk_entity_block;
    }
    else
    {
        chunk_entity_block->low_freq_entity_indices
            [chunk_entity_block->low_freq_entity_count++] =
            entity_low_freq_index;
    }

    return;
}

void remove_low_freq_entity_from_chunk(
    game_world_t *restrict game_world, u32 entity_low_freq_index,
    game_chunk_index_pair_t chunk_to_place_entity_in,
    arena_allocator_t *restrict arena_allocator)
{
    i64 chunk_index_x = chunk_to_place_entity_in.chunk_x;
    i64 chunk_index_y = chunk_to_place_entity_in.chunk_y;

    game_chunk_t *chunk =
        get_game_chunk(game_world, chunk_index_x, chunk_index_y);
    ASSERT(chunk);

    // Now, go over the entity blocks to see if entities low freq index is
    // already in it.
    game_chunk_entity_block_t *chunk_entity_block = chunk->chunk_entity_block;

    while (chunk_entity_block)
    {
        // TODO: Try moving this computation to SIMD.
        for (u32 entity_iter = 0;
             entity_iter < chunk_entity_block->low_freq_entity_count;
             entity_iter++)
        {
            if (chunk_entity_block->low_freq_entity_indices[entity_iter] ==
                entity_low_freq_index)
            {
                // Take the last entity from this chunk and place it in current
                // entities position.
                chunk_entity_block->low_freq_entity_indices[entity_iter] =
                    chunk_entity_block->low_freq_entity_indices
                        [chunk_entity_block->low_freq_entity_count-- - 1];

                return;
            }
        }

        if (chunk_entity_block->next_chunk_entity_block)
        {
            chunk_entity_block = chunk_entity_block->next_chunk_entity_block;
        }
        else
        {
            break;
        }
    }

    ASSERT(
        "If the code reaches here, entity was not in this chunks entity block");
    ASSERT(0);
}
// when entites are created, they are all in low freq state.
// Adds the entity to the particular chunks low freq indices array internally.
internal u32 create_entity(game_world_t *restrict game_world,
                           game_entity_type_t entity_type,
                           game_position_t position, v2f32_t dimension,
                           b32 collides,
                           arena_allocator_t *restrict arena_allocator,
                           b32 place_entity_in_chunk)
{
    ASSERT(arena_allocator);
    ASSERT(game_world);

    ASSERT(game_world->low_freq_entity_count <
           ARRAY_COUNT(game_world->low_freq_entities));

    u32 low_freq_entity_index = game_world->low_freq_entity_count++;

    game_world->low_freq_entities[low_freq_entity_index].position = position;
    game_world->low_freq_entities[low_freq_entity_index].dimension = dimension;
    game_world->low_freq_entities[low_freq_entity_index].collides = collides;
    game_world->low_freq_entities[low_freq_entity_index].entity_type =
        entity_type;

    if (place_entity_in_chunk)
    {
        place_low_freq_entity_in_chunk(game_world, low_freq_entity_index,
                                       get_chunk_index_from_position(position),
                                       arena_allocator);
    }

    return low_freq_entity_index;
}

// Move a entity from the low freq array to the high freq array.
// Returns high freq entity index and result status (success or failiure).
typedef struct
{
    u32 high_freq_index;
    b32 success;
} make_entity_high_freq_result_t;

internal make_entity_high_freq_result_t
make_entity_high_freq(game_world_t *game_world, u32 low_freq_index)
{
    // What should this array do :
    // (i) Create a high freq entity and add to the high freq entity array.
    // (ii) Move the low freq entity out of the array, and take the last element
    // in that array and place it in the previous low freq entities slot.

    ASSERT(low_freq_index < ARRAY_COUNT(game_world->low_freq_entities));

    make_entity_high_freq_result_t result = {0};

    game_entity_t *low_freq_entity =
        &game_world->low_freq_entities[low_freq_index];

    if (game_world->high_freq_entity_count <
        ARRAY_COUNT(game_world->high_freq_entities))
    {

        u32 high_freq_entity_index = game_world->high_freq_entity_count++;

        game_world->high_freq_entities[high_freq_entity_index].position =
            low_freq_entity->position;

        game_world->high_freq_entities[high_freq_entity_index].dimension =
            low_freq_entity->dimension;

        game_world->high_freq_entities[high_freq_entity_index].entity_type =
            low_freq_entity->entity_type;

        game_world->high_freq_entities[high_freq_entity_index].collides =
            low_freq_entity->collides;

        // If the low freq index is the last element of the array, simply
        // decrement low freq entity count.
        if (low_freq_index == game_world->low_freq_entity_count - 1)
        {
            game_world->low_freq_entity_count--;

            result.high_freq_index = high_freq_entity_index;
            result.success = 1;

            return result;
        }
        else
        {
            game_entity_t last_low_freq_entity =
                game_world
                    ->low_freq_entities[game_world->low_freq_entity_count - 1];

            game_world->low_freq_entities[low_freq_index] =
                last_low_freq_entity;
            --game_world->low_freq_entity_count;

            result.high_freq_index = high_freq_entity_index;
            result.success = 1;

            return result;
        }
    }

    else
    {
        // If the code reaches here, a high freq entity can NOT be created as
        // the high freq array list is already full.
    }

    result.high_freq_index = 0;
    result.success = 0;

    return result;
}

// Move a entity from the high freq array to the low freq array.
// Returns low freq entity index and a b32 to indicate if operation was
// possible.
typedef struct
{
    u32 low_freq_index;
    b32 success;
} make_entity_low_freq_result_t;

internal make_entity_low_freq_result_t
make_entity_low_freq(game_world_t *game_world, u32 high_freq_index)
{
    // What should this array do :
    // (i) Create a low freq entity and add to the low freq entity array.
    // (ii) Move the high freq entity out of the array, and take the last
    // element in that array and place it in the previous high freq entities
    // slot.

    ASSERT(high_freq_index < ARRAY_COUNT(game_world->high_freq_entities));
    make_entity_low_freq_result_t result = {0};

    game_entity_t *high_freq_entity =
        &game_world->high_freq_entities[high_freq_index];

    if (game_world->low_freq_entity_count <
        ARRAY_COUNT(game_world->low_freq_entities))
    {

        u32 low_freq_entity_index = game_world->low_freq_entity_count++;

        game_world->low_freq_entities[low_freq_entity_index].position =
            high_freq_entity->position;

        game_world->low_freq_entities[low_freq_entity_index].dimension =
            high_freq_entity->dimension;

        game_world->low_freq_entities[low_freq_entity_index].entity_type =
            high_freq_entity->entity_type;

        game_world->low_freq_entities[low_freq_entity_index].collides =
            high_freq_entity->collides;

        // If the high freq index is the last element of the array, simply
        // decrement high freq entity count.
        if (high_freq_index == game_world->high_freq_entity_count - 1)
        {
            game_world->high_freq_entity_count--;

            result.low_freq_index = low_freq_entity_index;
            result.success = 1;

            return result;
        }
        else
        {
            if (game_world->high_freq_entity_count == 0)
            {
                ASSERT(0);
            }

            game_entity_t last_high_freq_entity =
                game_world
                    ->high_freq_entities[game_world->high_freq_entity_count -
                                         1];

            game_world->high_freq_entities[high_freq_index] =
                last_high_freq_entity;

            --game_world->high_freq_entity_count;

            result.low_freq_index = low_freq_entity_index;
            result.success = 1;

            return result;
        }
    }
    else
    {
        // If the code reaches here, a low freq entity can NOT be created as
        // the low freq array list is already full.
    }

    result.low_freq_index = 0;
    result.success = 0;

    return result;
}

// Ref for understanding the BMP file format :
// https://gibberlings3.github.io/iesdp/file_formats/ie_formats/bmp_v5.htm
// Use pragma pack so that the padding of struct and all elements is 1 byte.
// NOTE: Assume that bmp v5 is being read.
#pragma pack(push, 1)
typedef struct
{
    u16 file_type;
    u32 file_size;
    u32 reserved;
    u32 bitmap_offset;
    u32 header_size;
    u32 width;
    u32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 image_size;
    u32 x_pixels_per_m;
    u32 y_pixels_per_m;
    u32 colors_used;
    u32 colors_important;
    u32 color_mask_r;
    u32 color_mask_g;
    u32 color_mask_b;
    u32 color_mask_a;
} bmp_header_t;
#pragma pack(pop)

internal game_texture_t
load_texture(platform_services_t *restrict platform_services,
             const char *file_path, arena_allocator_t *restrict arena_allocator)
{
    platform_file_read_result_t bmp_read_result =
        platform_services->platform_read_entire_file(file_path);

    bmp_header_t *bmp_header =
        (bmp_header_t *)(bmp_read_result.file_content_buffer);

    ASSERT(bmp_header->bits_per_pixel == 32);

    u32 *texture_pointer = (u32 *)arena_alloc_array(
        arena_allocator, bmp_header->width * bmp_header->height, sizeof(u32),
        sizeof(u32));

    // Now that the bmp file has been read succesfully, copy it u32 by u32 into
    // game_state.
    {
        u32 *source = (u32 *)((u8 *)bmp_read_result.file_content_buffer +
                              bmp_header->bitmap_offset);

        u32 *source_row = source;

        u32 *destination_pixel = texture_pointer;

        for (u32 y = 0; y < bmp_header->height; y++)
        {
            u32 *source_pixel = source_row;
            for (u32 x = 0; x < bmp_header->width; x++)
            {
                *destination_pixel++ = *source_pixel++;
            }
            source_row += bmp_header->width;
        }
    }

    game_texture_t texture = {0};
    texture.red_shift = get_index_of_lsb_set(bmp_header->color_mask_r);
    texture.blue_shift = get_index_of_lsb_set(bmp_header->color_mask_b);
    texture.green_shift = get_index_of_lsb_set(bmp_header->color_mask_g);
    texture.alpha_shift = get_index_of_lsb_set(bmp_header->color_mask_a);

    texture.height = bmp_header->height;
    texture.width = bmp_header->width;

    texture.pointer = texture_pointer;

    platform_services->platform_close_file(bmp_read_result.file_content_buffer);

    return texture;
}

__declspec(dllexport) void game_update_and_render(
    game_memory_t *restrict game_memory_allocator,
    game_framebuffer_t *restrict game_framebuffer,
    game_input_t *restrict game_input,
    platform_services_t *restrict platform_services)
{
    ASSERT(game_input != NULL);
    ASSERT(game_framebuffer->backbuffer_memory != NULL);
    ASSERT(game_memory_allocator->permanent_memory != NULL);
    ASSERT(platform_services != NULL);

    ASSERT(sizeof(game_state_t) < game_memory_allocator->permanent_memory_size)

    game_state_t *game_state =
        (game_state_t *)(game_memory_allocator->permanent_memory);

#ifdef PRISM_INTERNAL
    global_game_counters = game_state->game_counters;
#endif

    CLEAR_GAME_COUNTERS();
    BEGIN_GAME_COUNTER(game_update_and_render_counter);

    if (!game_state->is_initialized)
    {
        // Initialize memory arena.
        arena_init(&game_state->memory_arena,
                   game_memory_allocator->permanent_memory +
                       sizeof(game_state_t),
                   game_memory_allocator->permanent_memory_size -
                       sizeof(game_state_t));

        game_state->pixels_to_meters = game_framebuffer->width / 22.0f;

        game_state->game_world.camera_world_position.position =
            (v2f64_t){CHUNK_DIMENSION_IN_METERS_X / 2.0f,
                      CHUNK_DIMENSION_IN_METERS_Y / 2.0f};

        game_state->player_texture =
            load_texture(platform_services, "../assets/player_sprite.bmp",
                         &game_state->memory_arena);

        game_position_t player_position = {0};
        player_position.position =
            (v2f64_t){(f64)CHUNK_DIMENSION_IN_METERS_X / 4.0f,
                      (f64)CHUNK_DIMENSION_IN_METERS_Y / 4.0f};

        u32 player_low_freq_entity_index = create_entity(
            &game_state->game_world, game_entity_type_player, player_position,
            (v2f32_t){(f32)0.5f, (f32)0.5f}, 1, &game_state->memory_arena, 0);

        u32 player_high_freq_entity_index =
            make_entity_high_freq(&game_state->game_world,
                                  player_low_freq_entity_index)
                .high_freq_index;

        game_state->game_world.player_high_freq_entity_index =
            player_high_freq_entity_index;

        // Create a few chunks for the player to roam in.
        for (i32 chunk_y = -1; chunk_y <= 1; chunk_y++)
        {
            for (i32 chunk_x = -1; chunk_x <= 1; chunk_x++)
            {
                game_chunk_t *chunk =
                    set_game_chunk(&game_state->game_world, chunk_x, chunk_y,
                                   &game_state->memory_arena);

                v2f64_t chunk_relative_tile_entity_offset =
                    (v2f64_t){(f64)chunk_x * CHUNK_DIMENSION_IN_METERS_X,
                              (f64)chunk_y * CHUNK_DIMENSION_IN_METERS_Y};

                for (i32 tile_y = 0; tile_y < CHUNK_DIMENSION_IN_METERS_Y;
                     tile_y++)
                {
                    if (tile_y == CHUNK_DIMENSION_IN_METERS_Y / 2)
                    {
                        continue;
                    }

                    game_position_t position = {0};

                    position.position = (v2f64_t){(f64)0, (f64)tile_y};
                    position.position = v2f64_add(
                        position.position, chunk_relative_tile_entity_offset);

                    create_entity(&game_state->game_world,
                                  game_entity_type_wall, position,
                                  (v2f32_t){1.0f, 1.0f}, 1,
                                  &game_state->memory_arena, 1);

                    position.position = (v2f64_t){
                        (f64)(CHUNK_DIMENSION_IN_METERS_X - 1), (f64)tile_y};
                    position.position = v2f64_add(
                        position.position, chunk_relative_tile_entity_offset);

                    create_entity(&game_state->game_world,
                                  game_entity_type_wall, position,
                                  (v2f32_t){1.0f, 1.0f}, 1,
                                  &game_state->memory_arena, 1);
                }

                // Set the top and bottom border as obstacles.
                for (i32 tile_x = 0; tile_x < CHUNK_DIMENSION_IN_METERS_X;
                     tile_x++)
                {
                    if (tile_x == CHUNK_DIMENSION_IN_METERS_X / 2)
                    {
                        continue;
                    }

                    game_position_t position = {0};
                    position.position = (v2f64_t){(f64)tile_x, 0};
                    position.position = v2f64_add(
                        position.position, chunk_relative_tile_entity_offset);

                    create_entity(&game_state->game_world,
                                  game_entity_type_wall, position,
                                  (v2f32_t){1.0f, 1.0f}, 1,
                                  &game_state->memory_arena, 1);

                    position.position = (v2f64_t){
                        (f64)tile_x, (f64)CHUNK_DIMENSION_IN_METERS_Y - 1};
                    position.position = v2f64_add(
                        position.position, chunk_relative_tile_entity_offset);

                    create_entity(&game_state->game_world,
                                  game_entity_type_wall, position,
                                  (v2f32_t){1.0f, 1.0f}, 1,
                                  &game_state->memory_arena, 1);
                }
            }
        }
        // Add a familiar entity.
        /*
        create_entity(&game_state->game_world, game_entity_type_familiar,
                      player_position, (v2f32_t){0.5f, 0.5f}, 0,
                      &game_state->memory_arena, 1);
        */
        game_state->is_initialized = 1;
    }

    game_world_t *game_world = &game_state->game_world;

    game_chunk_index_pair_t camera_chunk_index =
        get_chunk_index_from_position(game_world->camera_world_position);

    i64 camera_current_chunk_x = camera_chunk_index.chunk_x;
    i64 camera_current_chunk_y = camera_chunk_index.chunk_y;

    // Take the current camera position, and the high frequency entities that
    // are not in the same chunk as player, move it to low freq array.
    u32 high_freq_entity_count = game_world->high_freq_entity_count;

    for (u32 entity_index = 0; entity_index < high_freq_entity_count;)
    {
        // NOTE: ignore the player, as player will always be high frequency.
        if (entity_index == game_world->player_high_freq_entity_index)
        {
            ++entity_index;
            continue;
        }

        game_entity_t *high_freq_entity =
            &game_world->high_freq_entities[entity_index];

        game_chunk_index_pair_t entity_chunk_index =
            get_chunk_index_from_position(high_freq_entity->position);

        i64 entity_current_chunk_x = entity_chunk_index.chunk_x;
        i64 entity_current_chunk_y = entity_chunk_index.chunk_y;

        if (entity_current_chunk_x != camera_current_chunk_x ||
            entity_current_chunk_y != camera_current_chunk_y)
        {
            make_entity_low_freq_result_t low_freq_entity_result =
                make_entity_low_freq(game_world, entity_index);

            if (!low_freq_entity_result.success)
            {
                break;
            }

            place_low_freq_entity_in_chunk(
                game_world, low_freq_entity_result.low_freq_index,
                entity_chunk_index, &game_state->memory_arena);
        }

        ++entity_index;
    }
    // Take the current camera position, and whatever is in the SAME chunk
    // as camera, make it high frequency. This computation is ONLY done for
    // entities that are in the new chunk.

    {

        game_chunk_t *camera_current_chunk = get_game_chunk(
            game_world, camera_current_chunk_x, camera_current_chunk_y);

        game_chunk_entity_block_t *camera_current_chunk_entity_block =
            camera_current_chunk->chunk_entity_block;

        while (camera_current_chunk_entity_block)
        {
            // NOTE: This varaibles changes value during the course of loop
            // iteration.
            u32 low_freq_entity_count =
                camera_current_chunk_entity_block->low_freq_entity_count;

            for (u32 entity_iterator = 0;
                 entity_iterator < low_freq_entity_count;)
            {
                u32 entity_index =
                    camera_current_chunk_entity_block
                        ->low_freq_entity_indices[entity_iterator];

                game_entity_t *low_freq_entity =
                    &game_world->low_freq_entities[entity_index];

                // BUG: Somehow, this is different from the cameras current
                // chunk index? How?
                game_chunk_index_pair_t entity_chunk_index =
                    get_chunk_index_from_position(low_freq_entity->position);

                i64 entity_current_chunk_x = entity_chunk_index.chunk_x;
                i64 entity_current_chunk_y = entity_chunk_index.chunk_y;

                // TODO: Is there a better way to handle this?
                // If the high freq array is already full, just increase the
                // entity index so that application will not lag and freeze
                // in infinite loop.
                make_entity_high_freq_result_t entity_high_freq_result =
                    make_entity_high_freq(game_world, entity_index);

                if (!entity_high_freq_result.success)
                {
                    break;
                }
                else
                {
                    ++entity_iterator;
                }

                // BUG: This fails, as the last element goes to first index,
                // and the below function is never called for that entity
                // that replaced the current one. If entity is made high
                // freq, remove it from the low freq chunk array.
                remove_low_freq_entity_from_chunk(game_world, entity_index,
                                                  entity_chunk_index,
                                                  &game_state->memory_arena);
            }
            camera_current_chunk_entity_block =
                camera_current_chunk_entity_block->next_chunk_entity_block;
        }
    }

    // Update player position based on input.
    // Explanation of the movement logic.
    // Acceleration is instantaneous.
    // Integrating over the delta time, velocity becomes (previous velocity)
    // + a
    // * dt. Then, position = vt + 1/2 at^2.
    f32 player_movement_speed = game_input->dt_for_frame * 1600.0f;

    v2f32_t player_acceleration = {0};

    player_acceleration.x =
        (game_input->keyboard_state.key_a.is_key_down ? -1.0f : 0.0f);

    player_acceleration.x +=
        (game_input->keyboard_state.key_d.is_key_down ? 1.0f : 0.0f);

    player_acceleration.y =
        (game_input->keyboard_state.key_w.is_key_down ? 1.0f : 0.0f);
    player_acceleration.y +=
        (game_input->keyboard_state.key_s.is_key_down ? -1.0f : 0.0f);

    if (player_acceleration.x && player_acceleration.y)
    {
        // 1.0f / sqrt(2) = 0.70710.
        player_acceleration =
            v2f32_scalar_multiply(player_acceleration, 0.70710f);
    }

    player_acceleration =
        v2f32_scalar_multiply(player_acceleration, player_movement_speed);

    // Faking friction :)
    player_acceleration = v2f32_subtract(
        player_acceleration,
        v2f32_scalar_multiply(game_state->player_velocity, 2.5f));

    game_state->player_velocity = v2f32_add(
        game_state->player_velocity,
        v2f32_scalar_multiply(player_acceleration, game_input->dt_for_frame));

    v2f64_t player_new_position = convert_to_v2f64(
        v2f32_add(v2f32_scalar_multiply(game_state->player_velocity,
                                        game_input->dt_for_frame),
                  v2f32_scalar_multiply(player_acceleration,
                                        0.5f * game_input->dt_for_frame *
                                            game_input->dt_for_frame)));

    game_entity_t *player_entity =
        &game_world
             ->high_freq_entities[game_world->player_high_freq_entity_index];

    player_new_position =
        v2f64_add(player_new_position, player_entity->position.position);

    // Perform simple AABB collision.
    rectangle_t player_rect = rectangle_from_center_and_origin(
        convert_to_v2f32(v2f64_subtract(
            player_new_position, game_world->camera_world_position.position)),
        player_entity->dimension);

    b32 player_collided = 0;
    v2f32_t wall_normal = (v2f32_t){0.0f, 0.0f};

    for (u32 entity_index = 0;
         entity_index < game_world->high_freq_entity_count; entity_index++)
    {
        if (entity_index == game_world->player_high_freq_entity_index)
        {
            continue;
        }

        if (game_state->game_world.high_freq_entities[entity_index]
                .entity_type == game_entity_state_does_not_exist)
        {
            continue;
        }

        game_entity_t *entity = &game_world->high_freq_entities[entity_index];

        if (!entity->collides)
        {
            continue;
        }

        rectangle_t entity_rect = rectangle_from_center_and_origin(
            convert_to_v2f32(
                v2f64_subtract(entity->position.position,
                               game_world->camera_world_position.position)),
            entity->dimension);

        b32 left_edge_collision =
            (player_rect.bottom_left_x <
             entity_rect.bottom_left_x + entity_rect.width);

        b32 right_edge_collision =
            (player_rect.bottom_left_x + player_rect.width >
             entity_rect.bottom_left_x);

        b32 top_edge_collision =
            (player_rect.bottom_left_y + player_rect.height >
             entity_rect.bottom_left_y);

        b32 bottom_edge_collision =
            (player_rect.bottom_left_y <
             entity_rect.bottom_left_y + entity_rect.height);

        if (left_edge_collision && right_edge_collision &&
            bottom_edge_collision && top_edge_collision)
        {
            player_collided = 1;
        }

        if (player_collided)
        {
            f32 left_x_overlap = entity_rect.bottom_left_x + entity_rect.width -
                                 player_rect.bottom_left_x;

            f32 right_x_overlap = player_rect.bottom_left_x +
                                  player_rect.width - entity_rect.bottom_left_x;

            f32 top_y_overlap = player_rect.bottom_left_y + player_rect.height -
                                entity_rect.bottom_left_y;

            f32 bottom_y_overlap = entity_rect.bottom_left_y +
                                   entity_rect.height -
                                   player_rect.bottom_left_y;

            f32 min_x_overlap = MIN(left_x_overlap, right_x_overlap);
            f32 min_y_overlap = MIN(bottom_y_overlap, top_y_overlap);

            if (min_x_overlap < min_y_overlap)
            {
                if (left_x_overlap < right_x_overlap)
                {
                    wall_normal = (v2f32_t){1.0f, 0.0f}; // Left edge collision
                }
                else
                {
                    wall_normal =
                        (v2f32_t){-1.0f, 0.0f}; // Right edge collision
                }
            }
            else
            {
                if (bottom_y_overlap < top_y_overlap)
                {
                    wall_normal =
                        (v2f32_t){0.0f, 1.0f}; // Bottom edge collision
                }
                else
                {
                    wall_normal = (v2f32_t){0.0f, -1.0f}; // Top edge collision
                }
            }
            break;
        }
    }

    if (!player_collided)
    {
        player_entity->position.position = player_new_position;
    }
    else
    {
        // Modify player velocity taking into account reflection.
        v2f32_t velocity_vector_to_wall_projection = v2f32_scalar_multiply(
            wall_normal, v2f32_dot(wall_normal, game_state->player_velocity));

        v2f32_t reflection_vector = v2f32_scalar_multiply(
            v2f32_subtract(game_state->player_velocity,
                           v2f32_scalar_multiply(
                               velocity_vector_to_wall_projection, 2.0f)),
            0.9f);

        game_state->player_velocity = reflection_vector;
    }

    // Take the familiar entity, and make it follow the player.
    for (u32 entity_index = 0;
         entity_index < game_world->high_freq_entity_count; entity_index++)
    {
        game_entity_t *familiar = &game_world->high_freq_entities[entity_index];
        if (familiar->entity_type == game_entity_type_familiar)
        {
            // The movement "speed" for familiar is inversely proportional
            // to the distance to player.
            v2f64_t familiar_to_player_vector = v2f64_subtract(
                player_entity->position.position, familiar->position.position);

            f64 distance_to_player = v2f64_len_sq(familiar_to_player_vector);

            if (distance_to_player != 0.0f && distance_to_player > 2.0f)
            {
                familiar->position.position = v2f64_add(
                    familiar->position.position,
                    v2f64_scalar_multiply(familiar_to_player_vector,
                                          0.1f * (1.0f / distance_to_player)));
            }
        }
    }

    // Clear screen.
    draw_rectangle(
        game_framebuffer, (v2f32_t){0.0f, 0.0f},
        (v2f32_t){(f32)game_framebuffer->width, (f32)game_framebuffer->height},
        0.0f, 0.0f, 0.0f, 1.0f);

    // If the player moves out of the current chunk, perform a smooth scroll
    // on the camera until it reaches the center of the players new chunk.
    //
    game_chunk_index_pair_t player_chunk_index =
        get_chunk_index_from_position(player_entity->position);

    i64 player_current_chunk_x = player_chunk_index.chunk_x;
    i64 player_current_chunk_y = player_chunk_index.chunk_y;

    f64 player_current_chunk_center_x =
        (f64)(player_current_chunk_x * CHUNK_DIMENSION_IN_METERS_X +
              0.5f * CHUNK_DIMENSION_IN_METERS_X);

    f64 player_current_chunk_center_y =
        (f64)(player_current_chunk_y * CHUNK_DIMENSION_IN_METERS_Y +
              0.5f * CHUNK_DIMENSION_IN_METERS_Y);

    // Get the current chunk the player is in.
    game_chunk_t *current_player_chunk = get_game_chunk(
        game_world, player_current_chunk_x, player_current_chunk_y);

    if (player_current_chunk_center_x !=
            game_world->camera_world_position.position.x ||
        player_current_chunk_center_y !=
            game_world->camera_world_position.position.y)
    {
        // Find the direction camera has to move.
        i64 direction_to_move_x =
            truncate_f64_to_i64(player_current_chunk_center_x -
                                game_world->camera_world_position.position.x);

        i64 direction_to_move_y =
            truncate_f64_to_i64(player_current_chunk_center_y -
                                game_world->camera_world_position.position.y);

        if (direction_to_move_x > 1)
        {
            direction_to_move_x = 1;
        }
        else if (direction_to_move_x < 0)
        {
            direction_to_move_x = -1;
        }

        if (direction_to_move_y > 1)
        {
            direction_to_move_y = 1;
        }
        else if (direction_to_move_y < 0)
        {
            direction_to_move_y = -1;
        }

        game_world->camera_world_position.position.x += direction_to_move_x;
        game_world->camera_world_position.position.y += direction_to_move_y;
    }

    // Render entites
    for (u32 entity_index = 0;
         entity_index < game_world->high_freq_entity_count; entity_index++)
    {
        game_entity_t *entity = &game_world->high_freq_entities[entity_index];

        v2f32_t entity_camera_relative_position_v2f32 = convert_to_v2f32(
            v2f64_subtract(entity->position.position,
                           game_world->camera_world_position.position));

        v2f32_t entity_position_bottom_left =
            v2f32_subtract(entity_camera_relative_position_v2f32,
                           v2f32_scalar_multiply(entity->dimension, 0.5f));

        v2f32_t entity_bottom_left_position_in_pixels = v2f32_scalar_multiply(
            entity_position_bottom_left, game_state->pixels_to_meters);

        v2f32_t rendering_offset =
            v2f32_add((v2f32_t){game_framebuffer->width / 2.0f,
                                game_framebuffer->height / 2.0f},
                      entity_bottom_left_position_in_pixels);

        switch (entity->entity_type)
        {
        case game_entity_type_player: {
            draw_rectangle(game_framebuffer, rendering_offset,
                           v2f32_scalar_multiply(entity->dimension,
                                                 game_state->pixels_to_meters),
                           1.0f, 1.0f, 1.0f, 0.5f);

            // Player sprite position has to be such that the bottom middle
            // of the sprite coincides with the botom middle of player
            // position.

            v2f32_t player_sprite_bottom_left_offset =
                v2f32_add((v2f32_t){game_framebuffer->width / 2.0f,
                                    game_framebuffer->height / 2.0f},
                          entity_bottom_left_position_in_pixels);

            // TODO: This is a very hacky solution, and will likely not work
            // when the player sprite changes dimensions. Fix this!!
            player_sprite_bottom_left_offset.x -=
                game_state->player_texture.width / 8.0f;

            draw_texture(&game_state->player_texture, game_framebuffer,
                         player_sprite_bottom_left_offset);
        }
        break;

        case game_entity_type_wall: {
            draw_rectangle(game_framebuffer, rendering_offset,
                           v2f32_scalar_multiply(entity->dimension,
                                                 game_state->pixels_to_meters),
                           0.4f, 0.1f, 0.4f, 0.5f);
        }
        break;

        case game_entity_type_familiar: {
            draw_rectangle(game_framebuffer, rendering_offset,
                           v2f32_scalar_multiply(entity->dimension,
                                                 game_state->pixels_to_meters),
                           0.4f, 0.1f, 0.4f, 0.5f);
        }
        break;

        default: {
            ASSERT("Invalid code path");
            ASSERT(0);
        }
        break;
        }
    }

    END_GAME_COUNTER(game_update_and_render_counter);
}
