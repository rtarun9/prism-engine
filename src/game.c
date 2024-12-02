#include "game.h"

#include "common.h"

typedef struct
{
    f32 x;
    f32 y;

    i32 tile_map_index_x;
    i32 tile_map_index_y;
} game_raw_position_t;

// NOTE: For now raw and canonical are pretty similar.
// But, once total FP positions are used, this will no longer be relevant.
typedef struct
{
    u32 tile_index_x;
    u32 tile_index_y;

    u32 tile_map_index_x;
    u32 tile_map_index_y;
} game_canonical_position_t;

internal void game_render_rectangle(game_offscreen_buffer_t *const buffer,
                                    f32 top_left_x, f32 top_left_y, u32 width,
                                    u32 height, f32 normalized_red,
                                    f32 normalized_green, f32 normalized_blue,
                                    f32 normalized_alpha)
{
    ASSERT(buffer);

    if (top_left_x < 0.0)
    {
        top_left_x = 0.0f;
    }

    if (top_left_y < 0.0)
    {
        top_left_y = 0.0f;
    }

    // Convert coords to framebuffer coords.
    u32 fb_top_left_x = round_f32_to_u32(top_left_x);
    u32 fb_top_left_y = round_f32_to_u32(top_left_y);

    if (fb_top_left_x + width >= buffer->width)
    {
        fb_top_left_x = buffer->width - width;
    }

    if (fb_top_left_y + height >= buffer->height)
    {
        fb_top_left_y = buffer->height - height;
    }

    u32 *row = (buffer->framebuffer_memory + fb_top_left_y * buffer->width +
                fb_top_left_x);
    u32 pitch = buffer->width;

    const u8 red = round_f32_to_u8(normalized_red * 255.0f);
    const u8 green = round_f32_to_u8(normalized_green * 255.0f);
    const u8 blue = round_f32_to_u8(normalized_blue * 255.0f);
    const u8 alpha = round_f32_to_u8(normalized_alpha * 255.0f);

    // Layout in memory is : XX RR GG BB.
    const u32 color = blue | (green << 8) | (red << 16) | (alpha << 24);

    for (u32 y = 0; y < height; y++)
    {
        u32 *pixel = row;
        for (u32 x = 0; x < width; x++)
        {
            ASSERT(pixel);
            *pixel++ = color;
        }
        row += pitch;
    }
}

internal void game_render_gradient_to_framebuffer(
    game_offscreen_buffer_t *const buffer, const i32 x_shift, const i32 y_shift)
{
    ASSERT(buffer);

    u32 *row = buffer->framebuffer_memory;
    u32 pitch = buffer->width;

    for (u32 y = 0; y < buffer->height; y++)
    {
        u32 *pixel = row;
        for (u32 x = 0; x < buffer->width; x++)
        {
            u8 red = ((y + x_shift) & 0xff);
            u8 blue = 0x00;
            u8 green = ((x + y_shift) & 0xff);
            u8 alpha = 0xff;

            // Layout in memory is : XX RR GG BB.
            *pixel++ = blue | (green << 8) | (red << 16) | (alpha << 24);
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

    if (tile_index_x < 0)
    {
        tile_index_x += TILE_MAP_DIM_X;
        raw_position.tile_map_index_x--;
    }

    if (tile_index_x >= TILE_MAP_DIM_X)
    {
        tile_index_x -= TILE_MAP_DIM_X;
        raw_position.tile_map_index_x++;
    }

    if (tile_index_y < 0)
    {
        tile_index_y += TILE_MAP_DIM_Y;
        raw_position.tile_map_index_y--;
    }

    if (tile_index_y >= TILE_MAP_DIM_Y)
    {
        tile_index_y -= TILE_MAP_DIM_Y;
        raw_position.tile_map_index_y++;
    }

    if (raw_position.tile_map_index_x < 0)
    {
        raw_position.tile_map_index_x = world->tile_map_count_x - 1;
    }

    if (raw_position.tile_map_index_x >= (i32)world->tile_map_count_x)
    {
        raw_position.tile_map_index_x = 0;
    }

    if (raw_position.tile_map_index_y < 0)
    {
        raw_position.tile_map_index_y = world->tile_map_count_y - 1;
    }

    if (raw_position.tile_map_index_y >= (i32)world->tile_map_count_y)
    {
        raw_position.tile_map_index_y = 0;
    }

    result.tile_map_index_x = (u32)raw_position.tile_map_index_x;
    result.tile_map_index_y = (u32)raw_position.tile_map_index_y;

    result.tile_index_x = (u32)tile_index_x;
    result.tile_index_y = (u32)tile_index_y;

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
        game_state->frequency = 256;

        u8 *file_buffer = platform_services->read_file("../src/game.c");
        ASSERT(file_buffer);

        platform_services->close_file(file_buffer);

        const char *string_to_write_to_file =
            "Hi@!!!!, this is some text that is written to a file!";

        ASSERT(platform_services->write_to_file(string_to_write_to_file,
                                                "output.txt") == true);

        game_state->player_x = 150.0f;
        game_state->player_y = 150.0f;

        game_state->player_width = 50u;
        game_state->player_height = 50u;

        game_state->player_tile_map_index_x = 0;
        game_state->player_tile_map_index_y = 0;

        game_state->game_world.tile_map_count_x = 2;
        game_state->game_world.tile_map_count_y = 2;

        game_state->game_world.top_left_x = 0.0f;
        game_state->game_world.top_left_y = 0.0f;

        game_state->game_world.tile_map_count_x = 2;
        game_state->game_world.tile_map_count_y = 1;

        game_state->game_world.tile_width = 50u;
        game_state->game_world.tile_height = 50u;

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

    // clang-format on

    game_tile_map_t tile_maps[2] = {tile_map00, tile_map01};
    game_state->game_world.tile_maps = (game_tile_map_t *)(tile_maps);

    const f32 player_movement_speed = game_input->delta_time * 0.2f;
    b32 can_player_move = true;

    f32 new_player_x = game_state->player_x;
    f32 new_player_y = game_state->player_y;

    if (game_input->keyboard_state.key_w.is_key_down)
    {
        game_state->x_shift += 4;
        game_state->frequency++;

        new_player_y -= player_movement_speed;
    }

    if (game_input->keyboard_state.key_s.is_key_down)
    {
        game_state->x_shift--;
        game_state->frequency--;

        new_player_y += player_movement_speed;
    }

    if (game_input->keyboard_state.key_a.is_key_down)
    {
        game_state->y_shift++;
        game_state->frequency--;

        new_player_x -= player_movement_speed;
    }

    if (game_input->keyboard_state.key_d.is_key_down)
    {
        game_state->y_shift--;
        game_state->frequency++;

        new_player_x += player_movement_speed;
    }

    game_state->y_shift++;
    game_state->x_shift++;

    // Use the player sprite's bottom left / center / right point for collision
    // detection.
    f32 player_collision_check_point_x_center =
        new_player_x + game_state->player_width / 2.0f;

    f32 player_collision_check_point_x_left = new_player_x;

    f32 player_collision_check_point_x_right =
        new_player_x + game_state->player_width;

    f32 player_collision_check_point_y =
        new_player_y + game_state->player_height;

    game_raw_position_t player_center_rp = {0};
    player_center_rp.tile_map_index_x = game_state->player_tile_map_index_x;
    player_center_rp.tile_map_index_y = game_state->player_tile_map_index_y;
    player_center_rp.x = player_collision_check_point_x_center;
    player_center_rp.y = player_collision_check_point_y;

    game_raw_position_t player_left_rp = {0};
    player_left_rp.tile_map_index_x = game_state->player_tile_map_index_x;
    player_left_rp.tile_map_index_y = game_state->player_tile_map_index_y;
    player_left_rp.x = player_collision_check_point_x_left;
    player_left_rp.y = player_collision_check_point_y;

    game_raw_position_t player_right_rp = {0};
    player_right_rp.tile_map_index_x = game_state->player_tile_map_index_x;
    player_right_rp.tile_map_index_y = game_state->player_tile_map_index_y;
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

    u32 a = is_tile_map_point_empty_in_world(&game_state->game_world,
                                             player_canonical_position_center);

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
        if (game_state->player_tile_map_index_x !=
            player_canonical_position_center.tile_map_index_x)
        {
            game_state->player_x =
                (f32)(player_canonical_position_center.tile_index_x *
                      game_state->game_world.tile_width);
            game_state->player_y =
                (f32)(player_canonical_position_center.tile_index_y *
                      game_state->game_world.tile_height);
        }
        else
        {
            game_state->player_x = new_player_x;
            game_state->player_y = new_player_y;
        }

        game_state->player_tile_map_index_x =
            player_canonical_position_center.tile_map_index_x;
        game_state->player_tile_map_index_y =
            player_canonical_position_center.tile_map_index_y;
    }

    game_render_gradient_to_framebuffer(
        game_offscreen_buffer, game_state->x_shift, game_state->y_shift);

    // Basic tile map rendering.
    game_tile_map_t *current_tile_map =
        &(game_state->game_world
              .tile_maps[game_state->player_tile_map_index_y *
                             game_state->game_world.tile_map_count_x +
                         game_state->player_tile_map_index_x]);

    for (u32 y = 0; y < TILE_MAP_DIM_Y; y++)
    {
        for (u32 x = 0; x < TILE_MAP_DIM_X; x++)
        {
            f32 tile_top_left_x =
                (f32)(0 + (x * game_state->game_world.tile_width));
            f32 tile_top_left_y =
                (f32)(0 + (y * game_state->game_world.tile_height));

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
    game_render_rectangle(game_offscreen_buffer, game_state->player_x,
                          game_state->player_y, game_state->player_width,
                          game_state->player_height, 0.1f, 0.2f, 1.0f, 1.0f);
}
