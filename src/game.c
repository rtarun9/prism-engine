#include "game.h"

void game_render(game_framebuffer_t *game_framebuffer, game_input_t *game_input)
{
    local_persist f32 blue_offset = 0.0f;
    blue_offset += 0.01f * game_input->keyboard_state.key_a.is_key_down;
    blue_offset -= 0.01f * game_input->keyboard_state.key_d.is_key_down;

    local_persist f32 green_offset = 0;
    green_offset += 0.01f * game_input->keyboard_state.key_w.is_key_down;
    green_offset -= 0.01f * game_input->keyboard_state.key_s.is_key_down;

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
