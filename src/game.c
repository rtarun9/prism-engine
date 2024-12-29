#include "game.h"

#include "common.h"

// NOTE: The top left x and y are relative to 'framebuffer' coordinates, where
// the top left corner is origin.
internal void game_render_rectangle(
    game_offscreen_buffer_t *restrict const buffer, f32 top_left_x,
    f32 top_left_y, f32 bottom_right_x, f32 bottom_right_y, f32 normalized_red,
    f32 normalized_green, f32 normalized_blue, f32 normalized_alpha)
{
    i32 min_x = round_f32_to_i32(top_left_x);
    i32 min_y = round_f32_to_i32(top_left_y);

    i32 max_x = round_f32_to_i32(bottom_right_x);
    i32 max_y = round_f32_to_i32(bottom_right_y);

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

// Handy helper function to get tile chunk index and tile index inside the tile
// chunk from the absolute tile coord.
typedef struct
{
    u32 tile_chunk_index_x;
    u32 tile_chunk_index_y;

    u32 tile_index_x;
    u32 tile_index_y;
} game_tile_chunk_position_t;

game_tile_chunk_position_t get_tile_chunk_position_from_abs_coords(
    u32 abs_tile_x, u32 abs_tile_y, u32 chunk_index_mask)
{
    const u32 tile_index_mask = ~chunk_index_mask;

    game_tile_chunk_position_t result = {};
    result.tile_chunk_index_x = abs_tile_x & chunk_index_mask;
    result.tile_chunk_index_y = abs_tile_y & chunk_index_mask;

    result.tile_index_x = abs_tile_x & tile_index_mask;
    result.tile_index_y = abs_tile_y & tile_index_mask;

    return result;
}

// TODO: Are both versions required?
game_tile_chunk_position_t get_tile_chunk_position_from_game_coord(
    const game_world_position_t position, const u32 chunk_index_mask)
{
    const u32 tile_index_mask = ~chunk_index_mask;

    game_tile_chunk_position_t result = {};
    result.tile_chunk_index_x = position.abs_tile_index_x & chunk_index_mask;
    result.tile_chunk_index_y = position.abs_tile_index_y & chunk_index_mask;

    result.tile_index_x = position.abs_tile_index_x & tile_index_mask;
    result.tile_index_y = position.abs_tile_index_y & tile_index_mask;

    return result;
}
// Adjust value of tile index if tile rel goes
// over the tile dimension.
// Adjust values of tile index & tile rel inplace.
internal void readjust_coordinate(u32 *tile_index, f32 *tile_rel, u32 tile_dim)
{
    ASSERT(tile_index);
    ASSERT(tile_rel);

    i32 delta = floor_f32_to_i32(*tile_rel / tile_dim);

    *tile_index += delta;

    *tile_rel = *tile_rel - delta * (i32)tile_dim;

    ASSERT(*tile_rel < tile_dim);
    ASSERT(*tile_rel >= 0.0f);
}

internal game_world_position_t readjust_position(
    game_world_t *const restrict world, game_world_position_t position)
{
    ASSERT(world);

    game_world_position_t result = position;

    readjust_coordinate(&result.abs_tile_index_x, &result.tile_rel_x,
                        world->tile_width);
    readjust_coordinate(&result.abs_tile_index_y, &result.tile_rel_y,
                        world->tile_height);
    return result;
}

// NOTE: Assertions disabled. If there is no tile chunk, don't hit a assertion,
// just return null.
// TODO: Check if assertion is still required here.
internal game_tile_chunk_t *get_tile_chunk_from_world(
    game_world_t *const restrict world, u32 tile_chunk_x, u32 tile_chunk_y)
{
    ASSERT(world);

    // ASSERT(tile_chunk_x >= 0 && tile_chunk_x < world->tile_chunk_count_x);
    // ASSERT(tile_chunk_y >= 0 && tile_chunk_y < world->tile_chunk_count_y);

    if (tile_chunk_x < world->tile_chunk_count_x &&
        tile_chunk_y < world->tile_chunk_count_y)
    {
        game_tile_chunk_t *tile_chunk =
            &world->tile_chunks[tile_chunk_x +
                                tile_chunk_y * world->tile_chunk_count_x];
        return tile_chunk;
    }

    return NULL;
}

internal u32 get_tile_chunk_value(game_tile_chunk_t *const restrict tile_chunk,
                                  const u32 tile_index_x, u32 tile_index_y)
{
    if (tile_chunk)
    {
        if (tile_index_x < (i32)TILE_CHUNK_DIM_X &&
            tile_index_y < (i32)TILE_CHUNK_DIM_Y)
        {
            return tile_chunk->tiles[tile_index_y][tile_index_x];
        }
    }

    return INVALID_TILE_VALUE;
}

internal u32
get_tile_chunk_value_in_world(game_world_t *const restrict world,
                              const game_tile_chunk_position_t position)

{
    game_tile_chunk_t *tile_chunk = get_tile_chunk_from_world(
        world, position.tile_chunk_index_x, position.tile_chunk_index_y);

    ASSERT(tile_chunk);

    return get_tile_chunk_value(tile_chunk, position.tile_index_x,
                                position.tile_index_y);
}

internal b32
is_tile_chunk_point_empty(game_tile_chunk_t *const restrict tile_chunk,
                          const u32 tile_index_x, const u32 tile_index_y)
{
    if (tile_chunk)
    {
        return get_tile_chunk_value(tile_chunk, tile_index_x, tile_index_y) ==
               0;
    }

    return false;
}

internal u32 get_tile_value_in_world(game_world_t *const restrict world,
                                     const game_tile_chunk_position_t position)

{
    // Convert x and y first into tile map coords.
    game_tile_chunk_t *tile_chunk = get_tile_chunk_from_world(
        world, position.tile_chunk_index_x, position.tile_chunk_index_y);

    return get_tile_chunk_value(tile_chunk, position.tile_index_x,
                                position.tile_index_y);
}

internal b32
is_tile_chunk_point_empty_in_world(game_world_t *const restrict world,
                                   const game_tile_chunk_position_t position)

{
    // Convert x and y first into tile map coords.
    game_tile_chunk_t *tile_chunk = get_tile_chunk_from_world(
        world, position.tile_chunk_index_x, position.tile_chunk_index_y);

    return is_tile_chunk_point_empty(tile_chunk, position.tile_index_x,
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
        game_state->pixels_per_meter = 100;

        game_state->chunk_index_mask = 0xffffff00;

        // The position of player bottom center.
        game_world_position_t player_pos = {};
        player_pos.abs_tile_index_x = 2;
        player_pos.abs_tile_index_y = 2;

        game_state->player_position = player_pos;

        game_state->player_width = 0.65f;
        game_state->player_height = 1.0f;

        game_state->game_world.tile_width = 1u;
        game_state->game_world.tile_height = 1u;

        game_state->game_world.bottom_left_x = 50.0f;
        game_state->game_world.bottom_left_y =
            (f32)game_offscreen_buffer->height -
            game_state->game_world.tile_height * game_state->pixels_per_meter;

        game_state->game_world.tile_chunk_count_x = 1;
        game_state->game_world.tile_chunk_count_y = 1;

        game_state->is_initialized = true;
    }

    // clang-format off
    // NOTE: The bottom left tile is considered as 0, 0.
    // This means that the tiles in the tile chunk prefab are flipped.
    game_tile_chunk_t tile_chunk_00 = {
    .tiles = {
            {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,    1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},

            {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,    1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
            {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1}
    },};
    // clang-format on

    // Clear screen.
    game_render_rectangle(
        game_offscreen_buffer, 0.0f, 0.0f, (f32)game_offscreen_buffer->width,
        (f32)game_offscreen_buffer->height, 0.0f, 0.0f, 0.0f, 1.0f);

    game_state->game_world.tile_chunks = &tile_chunk_00;

    // delta time in ms per frame.
    // Player movement speed is in meters per second.
    const f32 player_movement_speed = game_input->delta_time * 12.0f / 1000.0f;

    // NOTE: Player & world coords are such that y increases up, x increases
    // right (normal math coord system).
    f32 new_player_x = game_state->player_position.tile_rel_x;
    f32 new_player_y = game_state->player_position.tile_rel_y;

    if (game_input->keyboard_state.key_w.is_key_down)
    {
        new_player_y += player_movement_speed;
    }

    if (game_input->keyboard_state.key_s.is_key_down)
    {
        new_player_y -= player_movement_speed;
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

    f32 player_collision_check_point_y = new_player_y;

    game_world_position_t player_center_rp = game_state->player_position;

    player_center_rp.tile_rel_x = player_collision_check_point_x_center;
    player_center_rp.tile_rel_y = player_collision_check_point_y;

    game_world_position_t player_left_rp = game_state->player_position;
    player_left_rp.tile_rel_x = player_collision_check_point_x_left;
    player_left_rp.tile_rel_y = player_collision_check_point_y;

    game_world_position_t player_right_rp = game_state->player_position;
    player_right_rp.tile_rel_x = player_collision_check_point_x_right;
    player_right_rp.tile_rel_y = player_collision_check_point_y;

    game_world_position_t player_world_position_center =
        readjust_position(&game_state->game_world, player_center_rp);

    game_world_position_t player_world_position_left =
        readjust_position(&game_state->game_world, player_left_rp);

    game_world_position_t player_world_position_right =
        readjust_position(&game_state->game_world, player_right_rp);

    b32 can_player_move = true;
    if (!(is_tile_chunk_point_empty_in_world(
              &game_state->game_world, get_tile_chunk_position_from_game_coord(
                                           player_world_position_center,
                                           game_state->chunk_index_mask)) &&
          is_tile_chunk_point_empty_in_world(
              &game_state->game_world,
              get_tile_chunk_position_from_game_coord(
                  player_world_position_left, game_state->chunk_index_mask)) &&
          is_tile_chunk_point_empty_in_world(
              &game_state->game_world,
              get_tile_chunk_position_from_game_coord(
                  player_world_position_right, game_state->chunk_index_mask))))
    {
        can_player_move = false;
    }

    // Convert player coords to tile map coords.
    if (can_player_move)
    {
        game_state->player_position = player_world_position_center;
    }

    // NOTE: Player is always rendered right at the center of screen.
    const f32 center_x = game_offscreen_buffer->width / 2.0f;
    const f32 center_y = game_offscreen_buffer->height / 2.0f;

    for (i32 y = -7; y < 7; y++)
    {
        for (i32 x = -19; x < 19; x++)
        {
            i32 tile_x = x + (i32)game_state->player_position.abs_tile_index_x;
            i32 tile_y = y + (i32)game_state->player_position.abs_tile_index_y;

            f32 color = 0.0f;

            game_tile_chunk_position_t render_tile_chunk_position =
                get_tile_chunk_position_from_abs_coords(
                    tile_x, tile_y, game_state->chunk_index_mask);

            u32 current_tile_value = get_tile_value_in_world(
                &game_state->game_world, render_tile_chunk_position);

            if (current_tile_value == INVALID_TILE_VALUE)
            {
                continue;
            }

            if (current_tile_value == 1)
            {
                color = 1.0f;
            }

            // Higher player current tile position.
            if (x == 0 && y == 0)
            {
                color = 0.5f;
            }

            // NOTE: These are in framebuffer coords (top left corner is
            // origin).
            f32 fb_tile_left_x =
                (f32)(center_x + (x * (i32)game_state->game_world.tile_width *
                                  game_state->pixels_per_meter));

            f32 fb_tile_right_x =
                fb_tile_left_x + (game_state->pixels_per_meter *
                                  (i32)game_state->game_world.tile_width);

            f32 fb_tile_bottom_y =
                (f32)(center_y -
                      ((y) * (i32)game_state->game_world.tile_height *
                       game_state->pixels_per_meter));

            f32 fb_tile_top_y =
                fb_tile_bottom_y - (game_state->pixels_per_meter *
                                    game_state->game_world.tile_height);

            game_render_rectangle(game_offscreen_buffer, fb_tile_left_x,
                                  fb_tile_top_y, fb_tile_right_x,
                                  fb_tile_bottom_y, color, color, color, 1.0f);
        }
    }

    f32 fb_player_left_x = center_x;

    f32 fb_player_right_x = fb_player_left_x + (game_state->pixels_per_meter *
                                                game_state->player_width);

    f32 fb_player_bottom_y = center_y;
    f32 fb_player_top_y = fb_player_bottom_y - game_state->pixels_per_meter *
                                                   game_state->player_height;

    // Render the player.
    game_render_rectangle(game_offscreen_buffer, fb_player_left_x,
                          fb_player_top_y, fb_player_right_x,
                          fb_player_bottom_y, 0.1f, 0.2f, 1.0f, 1.0f);
}
