#include "game.h"

#include "common.h"

// color values are between 0 and 1.0f.
internal void game_render_rectangle(game_offscreen_buffer_t *const buffer,
                                    f32 top_left_x, f32 top_left_y, u32 width,
                                    u32 height, f32 normalized_red,
                                    f32 normalized_green, f32 normalized_blue,
                                    f32 normalized_alpha)
{
    ASSERT(buffer);

    if (top_left_x < -1.0f)
    {
        top_left_x = -1.0f;
    }

    if (top_left_x > 1.0f)
    {
        top_left_x = 1.0f;
    }

    if (top_left_y < -1.0f)
    {
        top_left_y = -1.0f;
    }

    if (top_left_y > 1.0f)
    {
        top_left_y = 1.0f;
    }

    // Convert coords to framebuffer coords.
    u32 fb_top_left_x =
        round_f32_to_u32(((top_left_x + 1.0f) / 2.0f) * buffer->width);
    u32 fb_top_left_y =
        round_f32_to_u32(((top_left_y + 1.0f) / 2.0f) * buffer->height);

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

        game_state->player_x = 0.001f;
        game_state->player_y = 0.0f;

        game_state->is_initialized = true;
    }

#define TILE_MAP_DIM_X 16
#define TILE_MAP_DIM_Y 9

    // clang-format off
    u32 tile_map[TILE_MAP_DIM_X * TILE_MAP_DIM_Y] = {
    1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,
    1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 1,
    1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 1,
    1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 1,
    0, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,
    1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 1,
    1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 1,
    1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 1,
    1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1};

    // clang-format on
    u32 top_left_x = 0;
    u32 top_left_y = 0;

    u32 tile_width = 90;
    u32 tile_height = 90;

    const f32 player_movement_speed = game_input->delta_time * 0.001f;
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

    // Check for player and tilemap collision.
    // Convert player coords to fb coords.
    i32 player_tile_map_coords_x =
        truncate_f32_to_i32((((new_player_x + 1.0f) / 2.0f) - top_left_x) *
                            game_offscreen_buffer->width / tile_width);
    i32 player_tile_map_coords_y =
        truncate_f32_to_i32((((new_player_y + 1.0f) / 2.0f) - top_left_y) *
                            game_offscreen_buffer->height / tile_height);

    if (player_tile_map_coords_x >= 0 &&
        player_tile_map_coords_x < (i32)TILE_MAP_DIM_X &&
        player_tile_map_coords_y >= 0 &&
        player_tile_map_coords_y < (i32)TILE_MAP_DIM_Y)
    {
        if (tile_map[player_tile_map_coords_x +
                     player_tile_map_coords_y * TILE_MAP_DIM_X] == 1u)
        {
            can_player_move = false;
        }
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
            f32 tile_top_left_x = (f32)(top_left_x + (x * tile_width));
            f32 tile_top_left_y = (f32)(top_left_y + (y * tile_height));

            // Convert top left position's in range of -1 to 1.
            const f32 buffer_space_top_left_x =
                (tile_top_left_x / (game_offscreen_buffer->width)) * 2.0f -
                1.0f;

            const f32 buffer_space_top_left_y =
                tile_top_left_y / (game_offscreen_buffer->height) * 2.0f - 1.0f;

            f32 color = 0.0f;
            if (tile_map[x + y * TILE_MAP_DIM_X] == 1u)
            {
                color = 1.0f;
            }

            game_render_rectangle(game_offscreen_buffer,
                                  buffer_space_top_left_x,
                                  buffer_space_top_left_y, tile_width,
                                  tile_height, color, color, color, 1.0f);
        }
    }

    // Render the player.
    game_render_rectangle(game_offscreen_buffer, game_state->player_x,
                          game_state->player_y, 50, 50, 0.1f, 0.2f, 1.0f, 1.0f);
}
