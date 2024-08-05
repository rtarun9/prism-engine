#include "game.h"

i32 round_f32_to_i32(const f32 value)
{
    return (i32)(value + 0.5f);
}

u32 round_f32_to_u32(const f32 value)
{
    return (u32)(value + 0.5f);
}

// NOTE: This function takes as input the top left coords and width and height.
void draw_rectangle(game_framebuffer_t *game_framebuffer, f32 top_left_x,
                    f32 top_left_y, f32 width, f32 height, f32 normalized_red,
                    f32 normalized_green, f32 normalized_blue)
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

    if (max_x > game_framebuffer->width)
    {
        max_x = game_framebuffer->width;
    }

    if (max_y > game_framebuffer->height)
    {
        max_y = game_framebuffer->height;
    }

    u8 *row = (game_framebuffer->backbuffer_memory + min_x * 4 +
               min_y * 4 * game_framebuffer->width);

    i32 pitch = game_framebuffer->width * 4;

    u32 red = round_f32_to_u32(normalized_red * 255.0f);
    u32 green = round_f32_to_u32(normalized_green * 255.0f);
    u32 blue = round_f32_to_u32(normalized_blue * 255.0f);

    // Framebuffer format : BB GG RR xx
    u32 pixel_color = blue | (green << 8) | (red << 16);

    for (i32 y = min_y; y < max_y; y++)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = min_x; x < max_x; x++)
        {
            *pixel++ = pixel_color;
        }
        row += pitch;
    }
}

__declspec(dllexport) void game_render(
    game_memory_allocator_t *restrict game_memory_allocator,
    game_framebuffer_t *restrict game_framebuffer,
    game_input_t *restrict game_input,
    platform_services_t *restrict platform_services)
{
    ASSERT(game_input != NULL);
    ASSERT(game_framebuffer->backbuffer_memory != NULL);
    ASSERT(game_memory_allocator->permanent_memory != NULL);

    ASSERT(sizeof(game_state_t) < game_memory_allocator->permanent_memory_size)

    game_state_t *game_state =
        (game_state_t *)(game_memory_allocator->permanent_memory);

    if (!game_state->is_initialized)
    {
        game_state->player_x = 50.0f;
        game_state->player_y = 50.0f;

        game_state->is_initialized = 1;

        ASSERT(platform_services != NULL);

        platform_file_read_result_t file_read_result =
            platform_services->platform_read_entire_file(__FILE__);

        platform_services->platform_write_to_file(
            "temp.txt", file_read_result.file_content_buffer,
            file_read_result.file_content_size);

        platform_services->platform_close_file(
            file_read_result.file_content_buffer);
    }

    // Update player position based on input.
    // Movement speed is in pixels / second.
    f32 player_movement_speed = 128.0f;
    f32 player_new_x_position = 0.0f;
    f32 player_new_y_position = 0.0f;

    player_new_x_position +=
        (game_input->keyboard_state.key_a.is_key_down ? -1 : 0);
    player_new_x_position +=
        (game_input->keyboard_state.key_d.is_key_down ? 1 : 0);

    player_new_y_position +=
        (game_input->keyboard_state.key_w.is_key_down ? -1 : 0);
    player_new_y_position +=
        (game_input->keyboard_state.key_s.is_key_down ? 1 : 0);

    game_state->player_x += (player_new_x_position)*player_movement_speed *
                            game_input->dt_for_frame;
    game_state->player_y += (player_new_y_position)*player_movement_speed *
                            game_input->dt_for_frame;

    // Create a simple tilemap of dimensions 17 X 9.
    // Why 17 instead of 16? so that the tile map will have a clear center
    // pixel.
    const u8 tile_map[9][17] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1}};

    // NOTE:
    // So this math logic is a bit weird, so get ready :
    // Framebuffer's total width and height is assumed to be having a 16:9
    // aspect ratio. The width and height will be so that the tiel map entirely
    // overlaps the framebuffer.
    const f32 tile_width = (f32)game_framebuffer->width / 16.0f;
    const f32 tile_height = tile_width;

    // Clear the screen.
    draw_rectangle(game_framebuffer, 0.0f, 0.0f, (f32)game_framebuffer->width,
                   (f32)game_framebuffer->height, 1.0f, 0.0f, 0.0f);

    // This is done because the tile width and tile map width are not the same
    // (see the division by 16 for tile width and number of columns of tile
    // map).
    // The borders aren't too important, and its fine if (atleast in the X
    // direction) only 1/2 of the tile in the left and right border is seen.
    const f32 upper_left_offset_x = (f32)tile_width / 2.0f;
    const f32 upper_left_offset_y = 0.0f;

    // Draw the tile map.
    for (i32 y = 0; y < 9; ++y)
    {
        for (i32 x = 0; x < 17; ++x)
        {
            const f32 top_left_x = -upper_left_offset_x + x * tile_width;
            const f32 top_left_y = -upper_left_offset_y + y * tile_height;

            if (tile_map[y][x] == 1)
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)tile_width,
                               (f32)tile_height, 1.0f, 1.0f, 1.0f);
            }
            else
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)tile_width,
                               (f32)tile_height, 0.0f, 0.0f, 0.0f);
            }
        }
    }

    // The player position will be at the middle bottom of the player sprite
    // (players feet in technical terms?)
    // The player center is not used since the player can go near a wall, but
    // will not stop immediately as soon as the player sprite collides with the
    // wall.
    const f32 player_sprite_width = tile_width * 0.75f;
    const f32 player_sprite_height = tile_height;

    const f32 player_top_left_x =
        game_state->player_x - 0.5f * player_sprite_width;
    const f32 player_top_left_y = game_state->player_y - player_sprite_height;

    draw_rectangle(game_framebuffer, (f32)player_top_left_x,
                   (f32)player_top_left_y, (f32)player_sprite_width,
                   (f32)player_sprite_height, 1.0f, 0.0f, 0.0f);
}
