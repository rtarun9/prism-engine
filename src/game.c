#include "game.h"

#include "common.h"

internal void game_render_rectangle(
    game_offscreen_buffer_t *restrict const buffer, f32 top_left_x,
    f32 top_left_y, u32 width, u32 height, f32 normalized_red,
    f32 normalized_green, f32 normalized_blue, f32 normalized_alpha)
{
    i32 min_x = round_f32_to_i32(top_left_x);
    i32 min_y = round_f32_to_i32(top_left_y);

    i32 max_x = round_f32_to_i32(top_left_x + width);
    i32 max_y = round_f32_to_i32(top_left_y + height);

    if (min_x < 0)
    {
        min_x = 0;
    }

    if (min_y < 0)
    {
        min_y = 0;
    }

    if (max_x > (i32)buffer->width)
    {
        max_x = buffer->width;
    }

    if (max_y > (i32)buffer->height)
    {
        max_y = buffer->height;
    }

    ASSERT(buffer);

    u32 *row = (buffer->framebuffer_memory + min_y * buffer->width + min_x);
    u32 pitch = buffer->width;

    const u8 red = round_f32_to_u8(normalized_red * 255.0f);
    const u8 green = round_f32_to_u8(normalized_green * 255.0f);
    const u8 blue = round_f32_to_u8(normalized_blue * 255.0f);
    const u8 alpha = round_f32_to_u8(normalized_alpha * 255.0f);

    // Layout in memory is : XX RR GG BB.
    const u32 color = blue | (green << 8) | (red << 16) | (alpha << 24);

    for (i32 y = min_y; y < max_y; y++)
    {
        u32 *pixel = row;
        for (i32 x = min_x; x < max_x; x++)
        {
            ASSERT(pixel);
            *pixel++ = color;
        }
        row += pitch;
    }
}

internal game_canonical_position_t get_canonical_position_from_raw(
    game_world_t *const restrict world, game_raw_position_t raw_position)
{
    ASSERT(world);

    game_canonical_position_t result = {0};

    i32 tile_index_x = truncate_f32_to_i32(
        (raw_position.x - world->top_left_x) / world->tile_width);

    i32 tile_index_y = truncate_f32_to_i32(
        (raw_position.y - world->top_left_y) / world->tile_height);

    f32 tile_rel_x = raw_position.x - (i32)tile_index_x * world->tile_width;
    f32 tile_rel_y = raw_position.y - (i32)tile_index_y * world->tile_height;

    ASSERT(tile_rel_x < world->tile_width);
    ASSERT(tile_rel_y < world->tile_height);

    ASSERT(tile_rel_x >= 0.0f);
    ASSERT(tile_rel_y >= 0.0f);

    i32 tile_map_index_x = raw_position.tile_map_index_x;
    i32 tile_map_index_y = raw_position.tile_map_index_y;

    if (tile_index_x < 0)
    {
        tile_index_x += TILE_MAP_DIM_X;
        tile_map_index_x--;
    }

    if (tile_index_x >= TILE_MAP_DIM_X)
    {
        tile_index_x -= TILE_MAP_DIM_X;
        tile_map_index_x++;
    }

    if (tile_index_y < 0)
    {
        tile_index_y += TILE_MAP_DIM_Y;
        tile_map_index_y--;
    }

    if (tile_index_y >= TILE_MAP_DIM_Y)
    {
        tile_index_y -= TILE_MAP_DIM_Y;
        tile_map_index_y++;
    }

    if (tile_map_index_x < 0)
    {
        tile_map_index_x = world->tile_map_count_x - 1;
    }

    if (tile_map_index_x >= (i32)world->tile_map_count_x)
    {
        tile_map_index_x = 0;
    }

    if (tile_map_index_y < 0)
    {
        tile_map_index_y = world->tile_map_count_y - 1;
    }

    if (tile_map_index_y >= (i32)world->tile_map_count_y)
    {
        tile_map_index_y = 0;
    }

    result.tile_map_index_x = tile_map_index_x;
    result.tile_map_index_y = tile_map_index_y;

    result.tile_index_x = (u32)tile_index_x;
    result.tile_index_y = (u32)tile_index_y;

    result.tile_rel_x = tile_rel_x;
    result.tile_rel_y = tile_rel_y;

    return result;
}

internal game_tile_map_t *get_tile_map_from_world(
    game_world_t *const restrict world, u32 tile_map_x, u32 tile_map_y)
{
    ASSERT(world);

    ASSERT(tile_map_x >= 0 && tile_map_x < world->tile_map_count_x);
    ASSERT(tile_map_y >= 0 && tile_map_y < world->tile_map_count_y);

    game_tile_map_t *tile_map =
        &world->tile_maps[tile_map_x + tile_map_y * world->tile_map_count_x];

    ASSERT(tile_map);

    return tile_map;
}

internal u32 get_tile_map_value(game_tile_map_t *const restrict tile_map,
                                const u32 tile_index_x, u32 tile_index_y)
{
    ASSERT(tile_map);

    if (tile_index_x >= 0 && tile_index_x < (i32)TILE_MAP_DIM_X &&
        tile_index_y >= 0 && tile_index_y < (i32)TILE_MAP_DIM_Y)
    {
        return tile_map->tile_map[tile_index_y][tile_index_x];
    }

    return INVALID_TILE_MAP_VALUE;
}

internal u32
get_tile_map_value_in_world(game_world_t *const restrict world,
                            const game_canonical_position_t position)

{
    game_tile_map_t *tile_map = get_tile_map_from_world(
        world, position.tile_map_index_x, position.tile_map_index_y);

    ASSERT(tile_map);

    return get_tile_map_value(tile_map, position.tile_index_x,
                              position.tile_index_y);
}

internal b32 is_tile_map_point_empty(game_tile_map_t *const restrict tile_map,
                                     const u32 tile_index_x,
                                     const u32 tile_index_y)
{
    ASSERT(tile_map);

    return get_tile_map_value(tile_map, tile_index_x, tile_index_y) == 0;
}

internal b32
is_tile_map_point_empty_in_world(game_world_t *const restrict world,
                                 const game_canonical_position_t position)

{
    // Convert x and y first into tile map coords.
    game_tile_map_t *tile_map = get_tile_map_from_world(
        world, position.tile_map_index_x, position.tile_map_index_y);

    return is_tile_map_point_empty(tile_map, position.tile_index_x,
                                   position.tile_index_y);
}

__declspec(dllexport) DEF_GAME_UPDATE_AND_RENDER_FUNC(game_update_and_render)
{
    ASSERT(game_offscreen_buffer);
    ASSERT(game_input);
    ASSERT(game_memory);
    ASSERT(platform_services);

    game_state_t *game_state =
        (game_state_t *)game_memory->permanent_memory_block;

    if (!game_state->is_initialized)
    {
        game_raw_position_t player_pos = {};
        player_pos.x = 100.0f;
        player_pos.y = 150.0f;
        player_pos.tile_map_index_x = 0;
        player_pos.tile_map_index_y = 0;

        game_state->player_position = player_pos;

        game_state->player_width = 30u;
        game_state->player_height = 50u;

        game_state->game_world.top_left_x = 0.0f;
        game_state->game_world.top_left_y = 0.0f;

        game_state->game_world.tile_width = 50u;
        game_state->game_world.tile_height = 50u;

        game_state->game_world.tile_map_count_x = 2;
        game_state->game_world.tile_map_count_y = 2;

        game_state->is_initialized = true;
    }

    // clang-format off
    game_tile_map_t tile_map00 = {
        .tile_map = {
            {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
            {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
            {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
            {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1}
        }
    };

    game_tile_map_t tile_map01 = {
        .tile_map = {
            {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
            {0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
            {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1}
        }
    };

    game_tile_map_t tile_map10 = {
        .tile_map = {
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
            {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  0, 0, 1, 0, 1},
            {1, 0, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  0, 1, 1, 0, 1},
            {1, 0, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  0, 1, 1, 0, 0},
            {1, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  0, 1, 1, 0, 1},
            {1, 0, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1, 1, 1, 0, 1},
            {1, 1, 1, 1,  1, 0, 0, 0,  0, 1, 0, 0,  0, 1, 1, 0, 1},
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1}
        }
    };

    game_tile_map_t tile_map11 = {
        .tile_map = {
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
            {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1},
            {0, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0},
            {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1},
            {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1},
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1}
        }
    };
    // clang-format on

    // Clear screen.
    game_render_rectangle(
        game_offscreen_buffer, 0.0f, 0.0f, game_offscreen_buffer->width,
        game_offscreen_buffer->height, 0.0f, 0.0f, 0.0f, 1.0f);

    game_tile_map_t tile_maps[4] = {tile_map00, tile_map01, tile_map10,
                                    tile_map11};
    game_state->game_world.tile_maps = (game_tile_map_t *)(tile_maps);

    const f32 player_movement_speed = game_input->delta_time * 0.5f;

    f32 new_player_x = game_state->player_position.x;
    f32 new_player_y = game_state->player_position.y;

    if (game_input->keyboard_state.key_w.is_key_down)
    {
        new_player_y -= player_movement_speed;
    }

    if (game_input->keyboard_state.key_s.is_key_down)
    {
        new_player_y += player_movement_speed;
    }

    if (game_input->keyboard_state.key_a.is_key_down)
    {
        new_player_x -= player_movement_speed;
    }

    if (game_input->keyboard_state.key_d.is_key_down)
    {
        new_player_x += player_movement_speed;
    }

    // Use the player sprite's bottom left / center / right point for collision
    // detection.
    f32 player_collision_check_point_x_right =
        new_player_x + game_state->player_width / 2.0f;

    f32 player_collision_check_point_x_center = new_player_x;

    f32 player_collision_check_point_x_left =
        new_player_x - game_state->player_width / 2.0f;

    f32 player_collision_check_point_y =
        new_player_y + game_state->player_height;

    game_raw_position_t player_center_rp = {0};
    player_center_rp.tile_map_index_x =
        game_state->player_position.tile_map_index_x;

    player_center_rp.tile_map_index_y =
        game_state->player_position.tile_map_index_y;

    player_center_rp.x = player_collision_check_point_x_center;
    player_center_rp.y = player_collision_check_point_y;

    game_raw_position_t player_left_rp = player_center_rp;
    player_left_rp.x = player_collision_check_point_x_left;
    player_left_rp.y = player_collision_check_point_y;

    game_raw_position_t player_right_rp = player_center_rp;
    player_right_rp.x = player_collision_check_point_x_right;
    player_right_rp.y = player_collision_check_point_y;

    game_canonical_position_t player_canonical_position_center =
        get_canonical_position_from_raw(&game_state->game_world,
                                        player_center_rp);

    game_canonical_position_t player_canonical_position_left =
        get_canonical_position_from_raw(&game_state->game_world,
                                        player_left_rp);

    game_canonical_position_t player_canonical_position_right =
        get_canonical_position_from_raw(&game_state->game_world,
                                        player_right_rp);

    b32 can_player_move = true;
    if (!(is_tile_map_point_empty_in_world(&game_state->game_world,
                                           player_canonical_position_center) &&
          is_tile_map_point_empty_in_world(&game_state->game_world,
                                           player_canonical_position_left) &&
          is_tile_map_point_empty_in_world(&game_state->game_world,
                                           player_canonical_position_right)))
    {
        can_player_move = false;
    }

    // Convert player coords to tile map coords.
    if (can_player_move)
    {
        game_state->player_position.tile_map_index_x =
            player_canonical_position_center.tile_map_index_x;

        game_state->player_position.tile_map_index_y =
            player_canonical_position_center.tile_map_index_y;

        game_state->player_position.x =
            player_canonical_position_center.tile_index_x *
                game_state->game_world.tile_width +
            player_canonical_position_center.tile_rel_x;

        game_state->player_position.y =
            player_canonical_position_center.tile_index_y *
                game_state->game_world.tile_height +
            player_canonical_position_center.tile_rel_y -
            game_state->player_height;
    }

    // Basic tile map rendering.
    game_tile_map_t *current_tile_map =
        &(game_state->game_world
              .tile_maps[game_state->player_position.tile_map_index_y *
                             game_state->game_world.tile_map_count_x +
                         game_state->player_position.tile_map_index_x]);

    for (u32 y = 0; y < TILE_MAP_DIM_Y; y++)
    {
        for (u32 x = 0; x < TILE_MAP_DIM_X; x++)
        {
            f32 tile_top_left_x =
                (f32)(game_state->game_world.top_left_x +
                      (x * game_state->game_world.tile_width));
            f32 tile_top_left_y =
                (f32)(game_state->game_world.top_left_y +
                      (y * game_state->game_world.tile_height));

            f32 color = 0.0f;
            if (current_tile_map->tile_map[y][x] == 1u)
            {
                color = 1.0f;
            }

            game_render_rectangle(
                game_offscreen_buffer, tile_top_left_x, tile_top_left_y,
                game_state->game_world.tile_width,
                game_state->game_world.tile_height, color, color, color, 1.0f);
        }
    }

    // Render the player.
    game_render_rectangle(
        game_offscreen_buffer,
        game_state->player_position.x - game_state->player_width / 2.0f,
        game_state->player_position.y, game_state->player_width,
        game_state->player_height, 0.1f, 0.2f, 1.0f, 1.0f);
}
