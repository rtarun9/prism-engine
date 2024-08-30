#include "game.h"

global_variable game_counter_t *global_game_counters = NULL;

#include "arena_allocator.c"
#include "arena_allocator.h"

#include "custom_intrinsics.h"
#include "custom_math.h"

#include "renderer.h"

#include "tile_map.c"
#include "tile_map.h"

internal game_entity_t get_entity(game_state_t *game_state, u32 index)
{
    game_entity_t entity = {0};

    entity.low_freq_entity = &game_state->game_world.low_freq_entities[index];
    entity.high_freq_entity = &game_state->game_world.high_freq_entities[index];

    return entity;
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

game_texture_t load_texture(platform_services_t *restrict platform_services,
                            const char *file_path,
                            game_state_t *restrict game_state)
{
    platform_file_read_result_t bmp_read_result =
        platform_services->platform_read_entire_file(file_path);

    bmp_header_t *bmp_header =
        (bmp_header_t *)(bmp_read_result.file_content_buffer);

    ASSERT(bmp_header->bits_per_pixel == 32);

    u32 *texture_pointer = (u32 *)arena_alloc_array(
        &game_state->memory_arena, bmp_header->width * bmp_header->height,
        sizeof(u32), sizeof(u32));

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

        // For now, it is assumed that a single meter equals 1/xth of the
        // framebuffer width.
        game_state->pixels_to_meters = game_framebuffer->width / 20.0f;
        game_state->game_world.tile_dimension =
            1.0f * game_state->pixels_to_meters;

        // The tile chunks for the world is pre-allocated. HOWEVER, the tile
        // array inside of the tile chunks are sparsely loaded.
        game_state->game_world.tile_chunks =
            (game_tile_chunk_t *)arena_alloc_array(
                &game_state->memory_arena,
                NUMBER_OF_CHUNKS_IN_WORLD_X * NUMBER_OF_CHUNKS_IN_WORLD_Y,
                sizeof(game_tile_chunk_t), DEFAULT_ALIGNMENT_VALUE);

        // Setup a basic tile chunk.
        for (i32 chunk_y = 0; chunk_y < NUMBER_OF_CHUNKS_IN_WORLD_Y; chunk_y++)
        {
            for (i32 chunk_x = 0; chunk_x < NUMBER_OF_CHUNKS_IN_WORLD_X;
                 chunk_x++)
            {
                if (chunk_x == 0 || (chunk_x == 1 && chunk_y == 1))
                {
                    game_tile_chunk_t *basic_tile_chunk = get_tile_chunk(
                        game_state->game_world.tile_chunks, chunk_x, chunk_y);

                    basic_tile_chunk->tile_chunk = (u32 *)arena_alloc_array(
                        &game_state->memory_arena,
                        NUMBER_OF_TILES_PER_CHUNK_X *
                            NUMBER_OF_TILES_PER_CHUNK_Y,
                        sizeof(u32), sizeof(u32));

                    // Set the left and right border as obstacles.
                    for (i32 tile_y = 0; tile_y < NUMBER_OF_TILES_PER_CHUNK_Y;
                         tile_y++)
                    {

                        set_value_in_tile_chunk(basic_tile_chunk, 0, tile_y,
                                                TILE_WALL);
                        set_value_in_tile_chunk(basic_tile_chunk,
                                                NUMBER_OF_TILES_PER_CHUNK_X - 1,
                                                tile_y, TILE_WALL);
                    }

                    set_value_in_tile_chunk(
                        basic_tile_chunk, NUMBER_OF_TILES_PER_CHUNK_X - 1,
                        NUMBER_OF_TILES_PER_CHUNK_Y / 2, TILE_EMPTY);

                    set_value_in_tile_chunk(basic_tile_chunk, 0,
                                            NUMBER_OF_TILES_PER_CHUNK_Y / 2,
                                            TILE_EMPTY);

                    // Set the top and bottom border as obstacles.
                    for (i32 tile_x = 0; tile_x < NUMBER_OF_TILES_PER_CHUNK_X;
                         tile_x++)
                    {
                        set_value_in_tile_chunk(basic_tile_chunk, tile_x, 0,
                                                TILE_WALL);

                        set_value_in_tile_chunk(basic_tile_chunk, tile_x,
                                                NUMBER_OF_TILES_PER_CHUNK_Y - 1,
                                                TILE_WALL);
                    }

                    set_value_in_tile_chunk(basic_tile_chunk,
                                            NUMBER_OF_TILES_PER_CHUNK_X / 2, 0,
                                            TILE_EMPTY);

                    set_value_in_tile_chunk(
                        basic_tile_chunk, NUMBER_OF_TILES_PER_CHUNK_X / 2,
                        NUMBER_OF_TILES_PER_CHUNK_Y - 1, TILE_EMPTY);

                    set_value_in_tile_chunk(
                        basic_tile_chunk, NUMBER_OF_TILES_PER_CHUNK_X / 2,
                        NUMBER_OF_TILES_PER_CHUNK_Y / 2, TILE_WALL);
                }
            }
        }

        game_state->player_texture = load_texture(
            platform_services, "../assets/player_sprite.bmp", game_state);

        game_entity_t player_entity =
            get_entity(game_state, game_state->player_entity_index);

        player_entity.low_freq_entity->tile_map_position.tile_index_x =
            SET_TILE_INDEX(0, 5);

        player_entity.low_freq_entity->tile_map_position.tile_index_y =
            SET_TILE_INDEX(0, 5);

        player_entity.low_freq_entity->tile_map_position.tile_relative_offset
            .x = 0.5f;
        player_entity.low_freq_entity->tile_map_position.tile_relative_offset
            .y = 0.5f;

        // The camera should be in the same tile chunk as player, but the tile
        // index should be in the middle of the chunk. Tile relative x and y
        // should be zero.
        game_state->camera_world_position.tile_index_x =
            SET_TILE_INDEX(0, NUMBER_OF_TILES_PER_CHUNK_X / 2);
        game_state->camera_world_position.tile_index_y =
            SET_TILE_INDEX(0, NUMBER_OF_TILES_PER_CHUNK_Y / 2);

        game_state->is_initialized = 1;
    }

    game_world_t game_world = game_state->game_world;

    // Update player position based on input.
    // Explanation of the movement logic.
    // Acceleration is instantaneous.
    // Integrating over the delta time, velocity becomes (previous velocity) + a
    // * dt. Then, position = vt + 1/2 at^2.
    f32 player_movement_speed = game_input->dt_for_frame * 16000.0f;

    vector2_t player_acceleration = {0};

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
            vector2_scalar_multiply(player_acceleration, 0.70710f);
    }

    player_acceleration =
        vector2_scalar_multiply(player_acceleration, player_movement_speed);

    // Faking friction :)
    player_acceleration = vector2_subtract(
        player_acceleration,
        vector2_scalar_multiply(game_state->player_velocity, 2.5f));

    vector2_t player_velocity = vector2_add(
        game_state->player_velocity,
        vector2_scalar_multiply(player_acceleration, game_input->dt_for_frame));

    vector2_t player_new_tile_relative_position = vector2_add(
        vector2_scalar_multiply(player_velocity, game_input->dt_for_frame),
        vector2_scalar_multiply(player_acceleration,
                                0.5f * game_input->dt_for_frame *
                                    game_input->dt_for_frame));

    game_state->player_velocity = player_velocity;

    game_entity_t player_entity =
        get_entity(game_state, game_state->player_entity_index);
    game_tile_map_position_t player_world_position =
        player_entity.low_freq_entity->tile_map_position;

    player_new_tile_relative_position =
        vector2_add(player_new_tile_relative_position,
                    player_world_position.tile_relative_offset);

    // The player position will be at the middle bottom of the player
    // sprite (players feet in technical terms?) The player center is
    // not used since the player can go near a wall, but will not stop
    // immediately as soon as the player sprite collides with the wall.
    // const f32 player_sprite_width = (f32)game_state->player_texture.width;
    const f32 player_sprite_width = (f32)game_state->player_texture.width;
    const f32 player_sprite_height = (f32)game_state->player_texture.height;

    game_tile_map_position_t current_player_position = player_world_position;

    game_tile_map_position_t new_player_position = get_game_tile_map_position(
        game_world.tile_dimension, player_new_tile_relative_position,
        player_world_position.tile_index_x, player_world_position.tile_index_y);

    game_tile_map_position_t player_right = get_game_tile_map_position(
        game_world.tile_dimension,
        vector2_add(player_new_tile_relative_position,
                    (vector2_t){player_sprite_width * 0.5f, 0.0f}),
        current_player_position.tile_index_x,
        current_player_position.tile_index_y);

    game_tile_map_position_t player_left = get_game_tile_map_position(
        game_world.tile_dimension,
        vector2_add(player_new_tile_relative_position,
                    (vector2_t){-player_sprite_width * 0.5f, 0.0f}),
        current_player_position.tile_index_x,
        current_player_position.tile_index_y);

    game_tile_map_position_t player_position_that_collided_with_tilemap = {};
    b32 player_collided = 0;

    if (check_point_and_tile_chunk_collision(&game_world, new_player_position))
    {
        player_position_that_collided_with_tilemap = new_player_position;
        player_collided = 1;
    }

    if (check_point_and_tile_chunk_collision(&game_world, player_right))
    {
        player_position_that_collided_with_tilemap = player_right;
        player_collided = 1;
    }

    if (check_point_and_tile_chunk_collision(&game_world, player_left))
    {
        player_position_that_collided_with_tilemap = player_left;
        player_collided = 1;
    }

    {
        // NOTE: Assumes that if a chunk is not loaded in a certain place, it is
        // out of bounds.
        if (!player_collided)
        {
            // Player can move to new position!!! Check for change in
            // current tilemap. Note that this is ONLY done by checking
            // the players position (i.e
            // player_position_collision_check). This should make the
            // code simpler as well.
            player_entity.low_freq_entity->tile_map_position =
                new_player_position;
        }
        else
        {
            // If player has collided with tilemap, the player velocity gets
            // reflected around the wall normal.

            vector2_t wall_normal = {};
            if (GET_TILE_INDEX(
                    player_position_that_collided_with_tilemap.tile_index_y) <
                GET_TILE_INDEX(player_world_position.tile_index_y))
            {
                wall_normal.y = -1.0f;
            }
            if (GET_TILE_INDEX(
                    player_position_that_collided_with_tilemap.tile_index_y) >
                GET_TILE_INDEX(player_world_position.tile_index_y))
            {
                wall_normal.y = 1.0f;
            }
            if (GET_TILE_INDEX(
                    player_position_that_collided_with_tilemap.tile_index_x) >
                GET_TILE_INDEX(player_world_position.tile_index_x))
            {
                wall_normal.x = 1.0f;
            }
            if (GET_TILE_INDEX(
                    player_position_that_collided_with_tilemap.tile_index_x) <
                GET_TILE_INDEX(player_world_position.tile_index_x))
            {
                wall_normal.x = -1.0f;
            }

            vector2_t input_vector_to_wall_projection = vector2_scalar_multiply(
                wall_normal,
                vector2_dot(wall_normal, game_state->player_velocity));

            vector2_t wall_projection_into_2 =
                vector2_scalar_multiply(input_vector_to_wall_projection, 2.0f);

            game_state->player_velocity = vector2_scalar_multiply(
                vector2_subtract(game_state->player_velocity,
                                 wall_projection_into_2),
                0.9f);
        }
    }

    game_tile_map_position_t player_position = player_world_position;

    // Clear screen.
    draw_rectangle(game_framebuffer, (vector2_t){0.0f, 0.0f},
                   (vector2_t){(f32)game_framebuffer->width,
                               (f32)game_framebuffer->height},
                   1.0f, 1.0f, 1.0f);

    // At any moment of time, the number of tiles that can be viewed on
    // the screen is FIXED.
    const i32 tiles_viewed_x = truncate_f32_to_i32(
        game_state->pixels_to_meters * game_framebuffer->width /
        game_world.tile_dimension);

    const i32 tiles_viewed_y = truncate_f32_to_i32(
        game_state->pixels_to_meters * game_framebuffer->height /
        game_world.tile_dimension);

    // Rendering offsets. The value are computed such that the current chunk is
    // at the center of the screen.
    vector2_t tile_map_rendering_bottom_left_offset = {0};
    tile_map_rendering_bottom_left_offset.x =
        game_framebuffer->width / 2.0f -
        GET_TILE_INDEX(game_state->camera_world_position.tile_index_x) *
            game_world.tile_dimension -
        game_world.tile_dimension / 2.0f;

    tile_map_rendering_bottom_left_offset.y =
        game_framebuffer->height / 2.0f -
        GET_TILE_INDEX(game_state->camera_world_position.tile_index_y) *
            game_world.tile_dimension -
        game_world.tile_dimension / 2.0f;

    game_tile_map_position_difference_t player_and_camera_difference =
        tile_map_position_difference(&player_position,
                                     &game_state->camera_world_position);

    if (player_and_camera_difference.chunk_x_diff != 0 ||
        player_and_camera_difference.chunk_y_diff != 0)
    {
        // Now, set the camera position to the same as player. Continue to move
        // the camera's position until the tile is the center of the chunk.
        game_state->camera_world_position = player_position;
    }

    // check if camera tile is the center chunk.
    u32 camera_tile_x =
        GET_TILE_INDEX(game_state->camera_world_position.tile_index_x);
    u32 camera_tile_y =
        GET_TILE_INDEX(game_state->camera_world_position.tile_index_y);

    if (camera_tile_x != NUMBER_OF_TILES_PER_CHUNK_X / 2 ||
        camera_tile_y != NUMBER_OF_TILES_PER_CHUNK_Y / 2)
    {
        u32 current_camera_chunk_center_tile_index_x =
            NUMBER_OF_TILES_PER_CHUNK_X / 2;

        u32 current_camera_chunk_center_tile_index_y =
            NUMBER_OF_TILES_PER_CHUNK_Y / 2;

        i32 direction_to_move_camera_to_reach_chunk_center_x =
            (current_camera_chunk_center_tile_index_x - camera_tile_x);

        if (direction_to_move_camera_to_reach_chunk_center_x > 1)
        {
            direction_to_move_camera_to_reach_chunk_center_x = 1;
        }
        else if (direction_to_move_camera_to_reach_chunk_center_x < 0)
        {
            direction_to_move_camera_to_reach_chunk_center_x = -1;
        }

        i32 direction_to_move_camera_to_reach_chunk_center_y =
            (current_camera_chunk_center_tile_index_y - camera_tile_y);

        if (direction_to_move_camera_to_reach_chunk_center_y > 1)
        {
            direction_to_move_camera_to_reach_chunk_center_y = 1;
        }
        else if (direction_to_move_camera_to_reach_chunk_center_y < 0)
        {
            direction_to_move_camera_to_reach_chunk_center_y = -1;
        }

        // Advance the camera tile by 1.
        game_state->camera_world_position.tile_index_x = SET_TILE_INDEX(
            game_state->camera_world_position.tile_index_x,
            (GET_TILE_INDEX(game_state->camera_world_position.tile_index_x)) +
                direction_to_move_camera_to_reach_chunk_center_x);

        game_state->camera_world_position.tile_index_y = SET_TILE_INDEX(
            game_state->camera_world_position.tile_index_y,
            (GET_TILE_INDEX(game_state->camera_world_position.tile_index_y)) +
                direction_to_move_camera_to_reach_chunk_center_y);
    }

    for (i32 _y = -tiles_viewed_y / 2; _y <= tiles_viewed_y / 2; ++_y)
    {
        for (i32 _x = -tiles_viewed_x / 2; _x <= tiles_viewed_x / 2; ++_x)
        {

            i32 x = _x + GET_TILE_INDEX(
                             game_state->camera_world_position.tile_index_x);
            i32 y = _y + GET_TILE_INDEX(
                             game_state->camera_world_position.tile_index_y);

            vector2_t bottom_left =
                vector2_add(tile_map_rendering_bottom_left_offset,
                            (vector2_t){game_world.tile_dimension * (x),
                                        (y)*game_world.tile_dimension});

            corrected_tile_indices_t tile_indices = get_corrected_tile_indices(
                x, y,
                GET_TILE_CHUNK_INDEX(
                    game_state->camera_world_position.tile_index_x),
                GET_TILE_CHUNK_INDEX(
                    game_state->camera_world_position.tile_index_y));

            u32 tile_value = get_value_of_tile_in_chunks(game_world.tile_chunks,
                                                         tile_indices);

            const vector2_t tile_dimension_vector = (vector2_t){
                game_world.tile_dimension, game_world.tile_dimension};

            if (tile_value == TILE_WALL)
            {
                draw_rectangle(game_framebuffer, bottom_left,
                               tile_dimension_vector, 0.4f, 0.1f, 0.4f);
            }
            else if (tile_value == TILE_EMPTY)

            {
                draw_rectangle(game_framebuffer, bottom_left,
                               tile_dimension_vector, 0.0f, 0.0f, 1.0f);
            }
            else if (tile_value != CHUNK_NOT_LOADED)
            {
                draw_rectangle(game_framebuffer, bottom_left,
                               tile_dimension_vector, 0.0f, 0.0f, 0.0f);
            }
        }
    }

    vector2_t player_bottom_left = {0};
    player_bottom_left.x = player_position.tile_relative_offset.x +
                           GET_TILE_INDEX(player_position.tile_index_x) *
                               game_world.tile_dimension -
                           0.5f * player_sprite_width;

    player_bottom_left.y = player_position.tile_relative_offset.y +
                           GET_TILE_INDEX(player_position.tile_index_y) *
                               game_world.tile_dimension;

    draw_texture(
        &game_state->player_texture, game_framebuffer,
        vector2_add(tile_map_rendering_bottom_left_offset, player_bottom_left));

    END_GAME_COUNTER(game_update_and_render_counter);
}
