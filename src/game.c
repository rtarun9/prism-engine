#include "game.h"

// The game outputs to a single buffer that is always linear.
// The platform layer takes care of intricasies like circular audio buffers
// (like for direct sound).
internal void game_output_sound_buffer(
    game_sound_buffer_t *const restrict sound_buffer,
    const u32 samples_to_output)
{
    local_persist f32 t_sine = 0.0f;
    const u32 max_volume = 3000;

    i16 *sample_region = (i16 *)sound_buffer->buffer;

    for (u32 sample_count = 0; sample_count < samples_to_output; sample_count++)
    {
        i16 sample_value = (i16)(sinf(t_sine) * max_volume);

        *sample_region++ = sample_value;
        *sample_region++ = sample_value;

        t_sine += (2.0f * pi32 * 1.0f) / (f32)sound_buffer->period_in_samples;
    }
}

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

internal void game_update_and_render(
    game_offscreen_buffer_t *const restrict game_offscreen_buffer,
    game_sound_buffer_t *const restrict game_sound_buffer,
    const u32 samples_to_output)
{
    game_output_sound_buffer(game_sound_buffer, samples_to_output);
    game_render_gradient_to_framebuffer(game_offscreen_buffer);
}
