#include "game.h"

void game_render(game_memory_allocator_t *restrict game_memory_allocator,
                 game_framebuffer_t *restrict game_framebuffer,
                 game_input_t *restrict game_input)
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
        game_state->is_initialized = 1;

        platform_file_read_result_t file_read_result =
            platform_read_entire_file(__FILE__);
        platform_write_to_file("temp.txt", file_read_result.file_content_buffer,
                               file_read_result.file_content_size);
        platform_close_file(file_read_result.file_content_buffer);
    }

    game_state->blue_offset +=
        0.1f * game_input->keyboard_state.key_a.is_key_down;
    game_state->blue_offset -=
        0.1f * game_input->keyboard_state.key_d.is_key_down;

    game_state->green_offset +=
        0.1f * game_input->keyboard_state.key_w.is_key_down;
    game_state->green_offset -=
        0.1f * game_input->keyboard_state.key_s.is_key_down;

    u8 *row = (u8 *)game_framebuffer->backbuffer_memory;
    const u32 row_stride = game_framebuffer->width * 4;

    for (i32 y = 0; y < game_framebuffer->height; ++y)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < game_framebuffer->width; x++)
        {
            // Each pixel represents a RGBX value, which in memory is lied out
            // as: BB GG RR xx
            u8 blue = x + game_state->blue_offset;
            u8 green = y + game_state->green_offset;
            *pixel++ = (green << 8) | blue;
        }

        row += row_stride;
    }
}
