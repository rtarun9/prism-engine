#include "game.h"

internal void game_render_gradient_to_framebuffer(
    game_offscreen_buffer_t *const buffer)
{
    local_persist u32 x_shift = 0;
    local_persist u32 y_shift = 0;

    u32 *row = buffer->framebuffer_memory;
    u32 pitch = buffer->width;

    for (u32 y = 0; y < buffer->height; y++)
    {
        u32 *pixel = row;
        for (u32 x = 0; x < buffer->width; x++)
        {
            u8 red = ((x + x_shift) & 0xff);
            u8 blue = ((y + y_shift) & 0xff);
            u8 green = 0;
            u8 alpha = 0xff;

            // Layout in memory is : XX RR GG BB.
            *pixel++ = blue | (green << 8) | (red << 16) | (alpha << 24);
        }

        row += pitch;
    }

    x_shift++;
}

internal void game_update_and_render(game_offscreen_buffer_t* restrict game_offscreen_buffer)
{
    game_render_gradient_to_framebuffer(game_offscreen_buffer);
}
