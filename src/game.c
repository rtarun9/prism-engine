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

        // For now, it is assumed that a single meter equals 1/xth of the
        // framebuffer width.
        game_state->pixels_to_meters = game_framebuffer->width / 20.0f;

        game_state->game_world.camera_world_position.position =
            (v2f64_t){0.0f, 0.0f};

        // Setup a basic tile chunk.
        for (i32 tile_y = -CHUNK_DIMENSION_IN_METERS_Y;
             tile_y <= CHUNK_DIMENSION_IN_METERS_Y; tile_y++)
        {
            if (tile_y == CHUNK_DIMENSION_IN_METERS_Y / 2)
            {
                continue;
            }

            game_position_t position = {0};

            position.position =
                (v2f64_t){-CHUNK_DIMENSION_IN_METERS_X / 2.0f, (f32)tile_y};

            create_entity(&game_state->game_world, game_entity_state_high_freq,
                          game_entity_type_wall, position,
                          (v2f32_t){1.0f, 1.0f});

            position.position = (v2f64_t){
                (f32)(CHUNK_DIMENSION_IN_METERS_X / 2.0f), (f32)tile_y};

            create_entity(&game_state->game_world, game_entity_state_high_freq,
                          game_entity_type_wall, position,
                          (v2f32_t){1.0f, 1.0f});
        }

        game_state->player_texture =
            load_texture(platform_services, "../assets/player_sprite.bmp",
                         &game_state->memory_arena);

        game_position_t player_position = {0};
        player_position.position =
            (v2f64_t){-CHUNK_DIMENSION_IN_METERS_X / 2.0f, 0};

        create_entity(&game_state->game_world, game_entity_state_high_freq,
                      game_entity_type_player, player_position,
                      (v2f32_t){(f32)game_state->player_texture.width,
                                (f32)game_state->player_texture.height});

        game_state->game_world.player_entity_index =
            game_state->game_world.index_of_last_created_entity;

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

    game_state->player_velocity 
    = v2f32_add(
        game_state->player_velocity,
        v2f32_scalar_multiply(player_acceleration, game_input->dt_for_frame));


    v2f64_t player_new_position = convert_to_v2f64(v2f32_add(
        v2f32_scalar_multiply(game_state->player_velocity, game_input->dt_for_frame),
        v2f32_scalar_multiply(player_acceleration,
                              0.5f * game_input->dt_for_frame *
                                  game_input->dt_for_frame)));


    game_entity_t *player_entity = get_entity(
        &game_state->game_world, game_state->game_world.player_entity_index);

    player_new_position =
        v2f64_add(player_new_position, player_entity->position.position);

    b32 player_collided = 0;

    for (u32 entity_index = 0; entity_index < MAX_NUMBER_OF_ENTITIES;
         entity_index++)
    {
        if (entity_index == game_world->player_entity_index)
        {
            continue;
        }

        // Perform simple AABB collision.
        typedef struct
        {
            f32 bottom_left;
            f32 bottom_right;
            f32 top_left;
            f32 top_right;
        } rectangle_t;

        rectangle_t player_rect = {0};
    }

    player_entity->position.position = player_new_position;

    // Clear screen.
    draw_rectangle(game_framebuffer, (v2f32_t){0.0f, 0.0f},
                   (v2f32_t){(f32)game_framebuffer->width,
                               (f32)game_framebuffer->height},
                   1.0f, 1.0f, 1.0f);

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

#if 0
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
#endif
#endif

    // Render entites
    for (u32 entity_index = 0; entity_index < MAX_NUMBER_OF_ENTITIES;
         entity_index++)
    {
        game_entity_t *entity =
            get_entity(&game_state->game_world, entity_index);

        v2f32_t entity_position_v2f32  = convert_to_v2f32(entity->position.position);

        if (game_state->game_world.entity_type[entity_index] ==
            game_entity_type_player)
        {
            v2f32_t player_bottom_left = {0};

            entity_position_v2f32 =
                v2f32_scalar_multiply(
                    entity_position_v2f32,
                    game_state->pixels_to_meters);

            player_bottom_left.x =entity_position_v2f32.x + 
                0.5f * entity->dimension.width +
                (f32)game_world->camera_world_position.position.x +
                (game_framebuffer->width / 2.0f);

            player_bottom_left.y =
                entity_position_v2f32.y +
                (f32)game_world->camera_world_position.position.y +
                (game_framebuffer->height / 2.0f);

            draw_texture(&game_state->player_texture, game_framebuffer,
                         player_bottom_left);
        }
        else if (game_state->game_world.entity_type[entity_index] ==
                 game_entity_type_wall)
        {
            const v2f32_t wall_dimension_vector =
                (v2f32_t){1.0f * game_state->pixels_to_meters,
                            1.0f * game_state->pixels_to_meters};

            entity_position_v2f32 =
                v2f32_scalar_multiply(entity_position_v2f32,
                                        game_state->pixels_to_meters);

            v2f32_t wall_bottom_left = entity_position_v2f32;
            wall_bottom_left.x +=
                (f32)game_world->camera_world_position.position.x -
                wall_dimension_vector.x / 2.0f +
                (game_framebuffer->width / 2.0f);

            wall_bottom_left.y +=
                (f32)game_world->camera_world_position.position.y -
                wall_dimension_vector.y / 2.0f +
                (game_framebuffer->height / 2.0f);

            draw_rectangle(game_framebuffer, wall_bottom_left,
                           wall_dimension_vector, 0.4f, 0.1f, 0.4f);
        }
    }

    END_GAME_COUNTER(game_update_and_render_counter);
}
