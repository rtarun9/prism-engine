#include "game.h"

void game_render(game_framebuffer_t *game_framebuffer, i32 blue_offset,
                 i32 green_offset)
{
    u8 *row = (u8 *)game_framebuffer->backbuffer_memory;
    const u32 row_stride = game_framebuffer->width * 4;

    for (i32 y = 0; y < game_framebuffer->height; ++y)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < game_framebuffer->width; x++)
        {
            // Each pixel represents a RGBX value, which in memory is lied out
            // as: BB GG RR xx
            u8 blue = x + blue_offset;
            u8 green = y + green_offset;
            *pixel++ = (green << 8) | blue;
        }

        row += row_stride;
    }
}
