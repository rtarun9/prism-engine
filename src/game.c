#include "game.h"

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
        game_state->blue_offset = 0.0f;
        game_state->green_offset = 0.0f;

        game_state->player_x = 100;
        game_state->player_y = 100;

        game_state->is_initialized = 1;

        platform_file_read_result_t file_read_result =
            platform_services->platform_read_entire_file(__FILE__);

        platform_services->platform_write_to_file(
            "temp.txt", file_read_result.file_content_buffer,
            file_read_result.file_content_size);
        platform_services->platform_close_file(
            file_read_result.file_content_buffer);
    }

    game_state->blue_offset +=
        0.01f * game_input->keyboard_state.key_a.is_key_down;
    game_state->blue_offset -=
        0.01f * game_input->keyboard_state.key_d.is_key_down;

    game_state->green_offset +=
        0.01f * game_input->keyboard_state.key_w.is_key_down;
    game_state->green_offset -=
        0.01f * game_input->keyboard_state.key_s.is_key_down;

    // Update player position based on keyboard input.
    game_state->player_y +=
        (i16)(0.1f * game_input->keyboard_state.key_s.is_key_down);
    game_state->player_y -=
        (i16)(0.1f * game_input->keyboard_state.key_w.is_key_down);

    game_state->player_x +=
        (i16)(0.1f * game_input->keyboard_state.key_d.is_key_down);
    game_state->player_x -=
        (i16)(0.1f * game_input->keyboard_state.key_a.is_key_down);

    if (game_input->keyboard_state.key_space.is_key_down &&
        game_input->keyboard_state.key_space.state_changed)
    {
        game_state->player_y -= 90;
    }

    // Hacky way of implementing gravity.
    game_state->player_y += 1;

    // Clamp the player position.
    if (game_state->player_x < 0)
    {
        game_state->player_x = 0;
    }

    if (game_state->player_x + 10 > game_framebuffer->width)
    {
        game_state->player_x = (i16)game_framebuffer->width - 10;
    }

    if (game_state->player_y < 0)
    {
        game_state->player_y = 0;
    }

    if (game_state->player_y + 10 > game_framebuffer->height)
    {
        game_state->player_y = (i16)game_framebuffer->height - 10;
    }

    u8 *row = (u8 *)game_framebuffer->backbuffer_memory;
    const u32 row_stride = game_framebuffer->width * 4;

    for (i32 y = 0; y < game_framebuffer->height; ++y)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < game_framebuffer->width; x++)
        {
            // Each pixel represents a RGBX value, which in memory is lied out
            // as: BB GG RR xx
            u8 blue = (u8)(x + game_state->blue_offset);
            u8 green = (u8)(y + game_state->green_offset);
            *pixel++ = (green << 8) | blue;
        }

        row += row_stride;
    }

    // Draw the player based on player position.
    i16 top_left_x = game_state->player_x;
    i16 top_left_y = game_state->player_y;
    i16 bottom_left_x = game_state->player_x + 10;
    i16 bottom_left_y = game_state->player_y + 10;

    for (i16 y = top_left_y; y < bottom_left_y; y++)
    {
        for (i16 x = top_left_x; x < bottom_left_x; x++)
        {
            u32 *pixel = ((u32 *)game_framebuffer->backbuffer_memory + x +
                          y * game_framebuffer->width);
            *pixel = 0xFFFFFFFF;
        }
    }
}
