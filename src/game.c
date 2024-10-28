#include "game.h"
#include "common.h"

// The game outputs to a single buffer that is always linear.
// The platform layer takes care of intricacies like circular audio buffers
// (like for direct sound).
internal void game_output_sound_buffer(
    game_sound_buffer_t *const restrict sound_buffer)
{
    ASSERT(sound_buffer);

    local_persist f32 t_sine = 0;
    const u32 max_volume = 2000;

    const u32 frequency = 256;
    const u32 period_in_samples = sound_buffer->samples_per_second / frequency;

    i16 *sample_region = (i16 *)sound_buffer->buffer;

    for (u32 sample_count = 0; sample_count < sound_buffer->samples_to_output;
         sample_count++)
    {
        t_sine += (2.0f * pi32 * 1.0f) / period_in_samples;

        i16 sample_value = (i16)(sinf(t_sine) * max_volume);

        *sample_region++ = sample_value;
        ASSERT(sample_region);

        *sample_region++ = sample_value;
        ASSERT(sample_region);
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
            u8 red = ((x + x_shift) & 0xff);
            u8 blue = ((y + y_shift) & 0xff);
            u8 green = 0;
            u8 alpha = 0xff;

            // Layout in memory is : XX RR GG BB.
            *pixel++ = blue | (green << 8) | (red << 16) | (alpha << 24);
        }

        row += pitch;
    }
}

internal void game_update_and_render(
    game_offscreen_buffer_t *const restrict game_offscreen_buffer,
    game_sound_buffer_t *const restrict game_sound_buffer,
    game_input_t *const restrict game_input)
{
    ASSERT(game_offscreen_buffer);
    ASSERT(game_sound_buffer);

    local_persist i32 x_shift = 0;
    local_persist i32 y_shift = 0;

    if (game_input->keyboard_state.key_w.is_key_down)
    {
        x_shift++;
    }

    if (game_input->keyboard_state.key_s.is_key_down)
    {
        x_shift--;
    }

    game_output_sound_buffer(game_sound_buffer);
    game_render_gradient_to_framebuffer(game_offscreen_buffer, x_shift,
                                        y_shift);
}
