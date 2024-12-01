#include "game.h"

#include "common.h"

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

game_tile_map_t *get_tile_map_from_world(game_world_t *const restrict world,
                                         u32 tile_map_x, u32 tile_map_y)
{
    ASSERT(world);

    game_tile_map_t *tile_map =
        &world->tile_maps[tile_map_x + tile_map_y * world->tile_map_count_x];
    ASSERT(tile_map);

    return tile_map;
}

u32 get_tile_map_value(game_tile_map_t *const restrict tile_map, f32 x, f32 y)
{
    ASSERT(tile_map);

    i32 tile_map_x_coord =
        truncate_f32_to_i32((x - tile_map->top_left_x) / tile_map->tile_width);

    i32 tile_map_y_coord =
        truncate_f32_to_i32((y - tile_map->top_left_y) / tile_map->tile_height);

    if (tile_map_x_coord >= 0 && tile_map_x_coord < (i32)TILE_MAP_DIM_X &&
        tile_map_y_coord >= 0 && tile_map_y_coord < (i32)TILE_MAP_DIM_Y)
    {
        return tile_map->tile_map[tile_map_y_coord][tile_map_x_coord];
    }

    return INVALID_TILE_MAP_VALUE;
}

u32 get_tile_map_value_in_world(game_world_t *const restrict world,
                                u32 tile_map_x, u32 tile_map_y, f32 x, f32 y)

{
    game_tile_map_t *tile_map =
        get_tile_map_from_world(world, tile_map_x, tile_map_y);
    ASSERT(tile_map);

    return get_tile_map_value(tile_map, x, y);
}

b32 is_tile_map_point_empty(game_tile_map_t *const restrict tile_map, f32 x,
                            f32 y)
{
    ASSERT(tile_map);

    return get_tile_map_value(tile_map, x, y) == 0;
}

b32 is_tile_map_point_empty_in_world(game_world_t *const restrict world,
                                     u32 tile_map_x, u32 tile_map_y, f32 x,
                                     f32 y)

{
    game_tile_map_t *tile_map =
        get_tile_map_from_world(world, tile_map_x, tile_map_y);

    return is_tile_map_point_empty(tile_map, x, y);
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

        game_state->player_x = game_offscreen_buffer->width / 1.9f;
        game_state->player_y = game_offscreen_buffer->height / 2.0f;

        game_state->player_width = 50u;
        game_state->player_height = 50u;

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

    // clang-format on
    tile_map00.top_left_x = 0.0f;
    tile_map00.top_left_y = 0.0f;

    tile_map00.tile_width = 80u;
    tile_map00.tile_height = 80u;

    game_state->game_world.tile_maps = (game_tile_map_t *)(&tile_map00);

    game_tile_map_t *current_tile_map = &game_state->game_world.tile_maps[0];

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

    if (!(is_tile_map_point_empty(current_tile_map,
                                  player_collision_check_point_x_center,
                                  player_collision_check_point_y) &&
          is_tile_map_point_empty(current_tile_map,
                                  player_collision_check_point_x_left,
                                  player_collision_check_point_y) &&
          is_tile_map_point_empty(current_tile_map,
                                  player_collision_check_point_x_right,
                                  player_collision_check_point_y)))
    {
        can_player_move = false;
    }

    // Convert player coords to tile map coords.
    if (can_player_move)
    {
        game_state->player_x = new_player_x;
        game_state->player_y = new_player_y;
    }

    game_render_gradient_to_framebuffer(
        game_offscreen_buffer, game_state->x_shift, game_state->y_shift);

    // Basic tile map rendering.

    for (u32 y = 0; y < TILE_MAP_DIM_Y; y++)
    {
        for (u32 x = 0; x < TILE_MAP_DIM_X; x++)
        {
            f32 tile_top_left_x = (f32)(current_tile_map->top_left_x +
                                        (x * current_tile_map->tile_width));
            f32 tile_top_left_y = (f32)(current_tile_map->top_left_y +
                                        (y * current_tile_map->tile_height));

            f32 color = 0.0f;
            if (current_tile_map->tile_map[y][x] == 1u)
            {
                color = 1.0f;
            }

            game_render_rectangle(game_offscreen_buffer, tile_top_left_x,
                                  tile_top_left_y, current_tile_map->tile_width,
                                  current_tile_map->tile_height, color, color,
                                  color, 1.0f);
        }
    }

    // Render the player.
    game_render_rectangle(game_offscreen_buffer, game_state->player_x,
                          game_state->player_y, game_state->player_width,
                          game_state->player_height, 0.1f, 0.2f, 1.0f, 1.0f);
}
