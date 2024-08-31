#include "game.h"

global_variable game_counter_t *global_game_counters = NULL;

#include "arena_allocator.c"
#include "arena_allocator.h"

#include "custom_intrinsics.h"
#include "custom_math.h"

#include "renderer.h"

internal game_entity_t *create_entity(game_world_t *game_world,
                                      game_entity_state_t entity_state,
                                      game_entity_type_t entity_type,
                                      game_position_t position,
                                      v2f32_t dimension)
{
    ASSERT(game_world->index_of_last_created_entity < MAX_NUMBER_OF_ENTITIES);

    u32 entity_index = ++game_world->index_of_last_created_entity;

    game_world->entity_states[entity_index] = entity_state;
    game_world->entity_type[entity_index] = entity_type;
    game_world->entities[entity_index].position = position;
    game_world->entities[entity_index].dimension = dimension;

    game_entity_t *entity = &game_world->entities[entity_index];

    return entity;
}

internal game_entity_t *get_entity(game_world_t *game_world, u32 index)
{
    ASSERT(index < MAX_NUMBER_OF_ENTITIES);

    game_entity_t *entity = &game_world->entities[index];

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
                            arena_allocator_t *restrict arena_allocator)
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
            (v2f64_t){0.0f, 0.0f};

        game_state->player_texture =
            load_texture(platform_services, "../assets/player_sprite.bmp",
                         &game_state->memory_arena);

        game_position_t player_position = {0};
        player_position.position =
            (v2f64_t){-CHUNK_DIMENSION_IN_METERS_X / 4.0f, 0.0};

        create_entity(&game_state->game_world, game_entity_state_high_freq,
                      game_entity_type_player, player_position,
                      (v2f32_t){(f32)0.5f, (f32)0.5f});

        game_state->game_world.player_entity_index =
            game_state->game_world.index_of_last_created_entity;

        // Setup a basic chunk.
        for (i32 tile_y = -CHUNK_DIMENSION_IN_METERS_Y / 2;
             tile_y <= CHUNK_DIMENSION_IN_METERS_Y / 2; tile_y++)
        {
            if (tile_y == 0)
            {
                continue;
            }

            game_position_t position = {0};

            position.position =
                (v2f64_t){-CHUNK_DIMENSION_IN_METERS_X / 2.0f, (f64)tile_y};

            create_entity(&game_state->game_world, game_entity_state_high_freq,
                          game_entity_type_wall, position,
                          (v2f32_t){1.0f, 1.0f});

            position.position = (v2f64_t){
                (f64)(CHUNK_DIMENSION_IN_METERS_X / 2.0f), (f64)tile_y};

            create_entity(&game_state->game_world, game_entity_state_high_freq,
                          game_entity_type_wall, position,
                          (v2f32_t){1.0f, 1.0f});
        }

        // Set the top and bottom border as obstacles.
        for (i32 tile_x = -CHUNK_DIMENSION_IN_METERS_X / 2;
             tile_x <= CHUNK_DIMENSION_IN_METERS_X / 2; tile_x++)
        {
            if (tile_x == 0)
            {
                continue;
            }

            game_position_t position = {0};
            position.position =
                (v2f64_t){(f64)tile_x, -CHUNK_DIMENSION_IN_METERS_Y / 2.0f};

            create_entity(&game_state->game_world, game_entity_state_high_freq,
                          game_entity_type_wall, position,
                          (v2f32_t){1.0f, 1.0f});

            position.position =
                (v2f64_t){(f64)tile_x, (f64)CHUNK_DIMENSION_IN_METERS_Y / 2.0f};

            create_entity(&game_state->game_world, game_entity_state_high_freq,
                          game_entity_type_wall, position,
                          (v2f32_t){1.0f, 1.0f});
        }

        game_state->is_initialized = 1;
    }

    game_world_t *game_world = &game_state->game_world;

    // Update player position based on input.
    // Explanation of the movement logic.
    // Acceleration is instantaneous.
    // Integrating over the delta time, velocity becomes (previous velocity) + a
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

    game_entity_t *player_entity = get_entity(
        &game_state->game_world, game_state->game_world.player_entity_index);

    player_new_position =
        v2f64_add(player_new_position, player_entity->position.position);

    // TODO: Perform AABB in camera relative OR even player relative coords.
    // Perform simple AABB collision.
    rectangle_t player_rect = rectangle_from_center_and_origin(
        convert_to_v2f32(player_new_position), player_entity->dimension);

    b32 player_collided = 0;
    v2f32_t wall_normal = (v2f32_t){0.0f, 0.0f};

    for (u32 entity_index = 0; entity_index < MAX_NUMBER_OF_ENTITIES;
         entity_index++)
    {
        if (entity_index == game_world->player_entity_index)
        {
            continue;
        }

        if (game_state->game_world.entity_states[entity_index] ==
            game_entity_state_does_not_exist)
        {
            continue;
        }

        game_entity_t *entity =
            get_entity(&game_state->game_world, entity_index);

        rectangle_t entity_rect = rectangle_from_center_and_origin(
            convert_to_v2f32(entity->position.position), entity->dimension);

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
            1.9f);

        game_state->player_velocity = reflection_vector;
    }

    // Clear screen.
    draw_rectangle(
        game_framebuffer, (v2f32_t){0.0f, 0.0f},
        (v2f32_t){(f32)game_framebuffer->width, (f32)game_framebuffer->height},
        0.0f, 0.0f, 0.0f);

#if 0
    // Rendering offsets. The value are computed such that the current chunk is
    // at the center of the screen.
    vector2_t tile_map_rendering_bottom_left_offset = {0};
    tile_map_rendering_bottom_left_offset.x =
        game_framebuffer->width / 2.0f -
        GET_TILE_INDEX(
            game_state->game_world.camera_world_position.tile_index_x) *
            game_world.tile_dimension -
        game_world.tile_dimension / 2.0f;

    tile_map_rendering_bottom_left_offset.y =
        game_framebuffer->height / 2.0f -
        GET_TILE_INDEX(
            game_state->game_world.camera_world_position.tile_index_y) *
            game_world.tile_dimension -
        game_world.tile_dimension / 2.0f;

    game_tile_map_position_difference_t player_and_camera_difference =
        tile_map_position_difference(
            &player_position, &game_state->game_world.camera_world_position);

    if (player_and_camera_difference.chunk_x_diff != 0 ||
        player_and_camera_difference.chunk_y_diff != 0)
    {
        // Now, set the camera position to the same as player. Continue to move
        // the camera's position until the tile is the center of the chunk.
        game_state->game_world.camera_world_position = player_position;
    }

    // check if camera tile is the center chunk.
    u32 camera_tile_x = GET_TILE_INDEX(
        game_state->game_world.camera_world_position.tile_index_x);
    u32 camera_tile_y = GET_TILE_INDEX(
        game_state->game_world.camera_world_position.tile_index_y);

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
        game_state->game_world.camera_world_position.tile_index_x =
            SET_TILE_INDEX(
                game_state->game_world.camera_world_position.tile_index_x,
                (GET_TILE_INDEX(game_state->game_world.camera_world_position
                                    .tile_index_x)) +
                    direction_to_move_camera_to_reach_chunk_center_x);

        game_state->game_world.camera_world_position.tile_index_y =
            SET_TILE_INDEX(
                game_state->game_world.camera_world_position.tile_index_y,
                (GET_TILE_INDEX(game_state->game_world.camera_world_position
                                    .tile_index_y)) +
                    direction_to_move_camera_to_reach_chunk_center_y);
    }

#endif

    // Render entites
    for (u32 entity_index = 0; entity_index < MAX_NUMBER_OF_ENTITIES;
         entity_index++)
    {
        game_entity_t *entity =
            get_entity(&game_state->game_world, entity_index);

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

        if (game_state->game_world.entity_type[entity_index] ==
            game_entity_type_player)
        {
            draw_rectangle(game_framebuffer, rendering_offset,
                           v2f32_scalar_multiply(entity->dimension,
                                                 game_state->pixels_to_meters),
                           1.0f, 1.0f, 1.0f);

            // Player sprite position has to be such that the bottom middle of
            // the sprite coincides with the botom middle of player position.

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
        else if (game_state->game_world.entity_type[entity_index] ==
                 game_entity_type_wall)
        {
            draw_rectangle(game_framebuffer, rendering_offset,
                           v2f32_scalar_multiply(entity->dimension,
                                                 game_state->pixels_to_meters),
                           0.4f, 0.1f, 0.4f);
        }
    }

    END_GAME_COUNTER(game_update_and_render_counter);
}
