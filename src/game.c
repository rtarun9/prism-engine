#include "game.h"

#include "common.h"

internal void game_render_rectangle(game_offscreen_buffer_t *const buffer,
                                    f32 top_left_x, f32 top_left_y, u32 width,
                                    u32 height)
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
        truncate_f32_to_u32(((top_left_x + 1.0f) / 2.0f) * buffer->width);
    u32 fb_top_left_y =
        truncate_f32_to_u32(((top_left_y + 1.0f) / 2.0f) * buffer->height);

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

    for (u32 y = 0; y <= height; y++)
    {
        u32 *pixel = row;
        for (u32 x = 0; x <= width; x++)
        {

            u8 red = 0xff;
            u8 blue = 0xf0;
            u8 green = 0xff;
            u8 alpha = 0xff;

            // Layout in memory is : XX RR GG BB.
            *pixel++ = blue | (green << 8) | (red << 16) | (alpha << 24);
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

        game_state->player_x = 0.0f;
        game_state->player_y = 0.0f;

        game_state->is_initialized = true;
    }

    const f32 player_movement_speed = game_input->delta_time * 0.001f;

    if (game_input->keyboard_state.key_w.is_key_down)
    {
        game_state->x_shift += 4;
        game_state->frequency++;

        game_state->player_y -= player_movement_speed;
    }

    if (game_input->keyboard_state.key_s.is_key_down)
    {
        game_state->x_shift--;
        game_state->frequency--;

        game_state->player_y += player_movement_speed;
    }

    if (game_input->keyboard_state.key_a.is_key_down)
    {
        game_state->y_shift++;
        game_state->frequency--;

        game_state->player_x -= player_movement_speed;
    }

    if (game_input->keyboard_state.key_d.is_key_down)
    {
        game_state->y_shift--;
        game_state->frequency++;

        game_state->player_x += player_movement_speed;
    }

    game_state->y_shift++;
    game_state->x_shift++;

    game_render_gradient_to_framebuffer(
        game_offscreen_buffer, game_state->x_shift, game_state->y_shift);

    game_render_rectangle(game_offscreen_buffer, game_state->player_x,
                          game_state->player_y, 50, 50);
}
