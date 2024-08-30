#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "custom_math.h"

// NOTE: The bottom left corner is 0, 0.
typedef struct
{
    u8 *backbuffer_memory;
    u32 width;
    u32 height;
} game_framebuffer_t;

typedef struct
{
    // Given a pixel color, use the following shift values using (pixel >>
    // shift) & 0xff to extract individual channel values.
    u32 red_shift;
    u32 green_shift;
    u32 blue_shift;
    u32 alpha_shift;

    u32 height;
    u32 width;

    u32 *pointer;
} game_texture_t;

internal void draw_rectangle(game_framebuffer_t *game_framebuffer,
                             vector2_t bottom_left_offset,
                             vector2_t width_and_height, f32 normalized_red,
                             f32 normalized_green, f32 normalized_blue)
{
    ASSERT(game_framebuffer);

    ASSERT(width_and_height.x >= 0.0f);
    ASSERT(width_and_height.y >= 0.0f);

    ASSERT(normalized_red >= 0.0f);
    ASSERT(normalized_green >= 0.0f);
    ASSERT(normalized_blue >= 0.0f);

    i32 min_x = round_f32_to_i32(bottom_left_offset.x);
    i32 min_y = round_f32_to_i32(bottom_left_offset.y);

    i32 max_x = round_f32_to_i32(bottom_left_offset.x + width_and_height.x);
    i32 max_y = round_f32_to_i32(bottom_left_offset.y + width_and_height.y);

    if (min_x < 0)
    {
        min_x = 0;
    }

    if (min_y < 0)
    {
        min_y = 0;
    }

    if (max_x >= (i32)game_framebuffer->width)
    {
        max_x = game_framebuffer->width - 1;
    }

    if (max_y >= (i32)game_framebuffer->height)
    {
        max_y = game_framebuffer->height - 1;
    }

    u32 *row = (u32 *)(game_framebuffer->backbuffer_memory + min_x * 4 +
                       min_y * 4 * game_framebuffer->width);

    i32 pitch = game_framebuffer->width;

    u32 red = round_f32_to_u32(normalized_red * 255.0f);
    u32 green = round_f32_to_u32(normalized_green * 255.0f);
    u32 blue = round_f32_to_u32(normalized_blue * 255.0f);

    // Framebuffer format : xx RR GG BB
    u32 pixel_color = blue | (green << 8) | (red << 16);

    for (i32 y = min_y; y <= max_y; y++)
    {
        u32 *pixel = row;
        for (i32 x = min_x; x <= max_x; x++)
        {
            *pixel++ = pixel_color;
        }
        row += pitch;
    }
}

internal void draw_texture(game_texture_t *restrict texture,
                           game_framebuffer_t *restrict framebuffer,
                           vector2_t bottom_left_offset)
{
    i32 min_x = round_f32_to_i32(bottom_left_offset.x);
    i32 min_y = round_f32_to_i32(bottom_left_offset.y);

    i32 clip_x = 0;
    i32 clip_y = 0;

    if (min_x < 0)
    {
        clip_x = -min_x;
        min_x = 0;
    }

    if (min_y < 0)
    {
        clip_y = -min_y;
        min_y = 0;
    }

    i32 max_x = round_f32_to_i32(bottom_left_offset.x + texture->width);
    i32 max_y = round_f32_to_i32(bottom_left_offset.y + texture->height);

    if (max_x >= (i32)framebuffer->width)
    {
        clip_x = max_x - framebuffer->width;
        max_x = framebuffer->width - 1;
    }

    if (max_y >= (i32)framebuffer->height)
    {
        clip_y = max_y - framebuffer->height;
        max_y = framebuffer->height - 1;
    }

    u32 blit_width = (texture->width > framebuffer->width ? framebuffer->width
                                                          : texture->width);
    u32 blit_height = texture->height > framebuffer->height
                          ? framebuffer->height
                          : texture->height;

    if ((i32)blit_width - clip_x < 0)
    {
        blit_width = 0;
    }
    else
    {
        blit_width -= clip_x;
    }

    if ((i32)blit_height - clip_y < 0)
    {
        blit_height = 0;
    }
    else
    {
        blit_height -= clip_y;
    }

    u32 *source = texture->pointer;
    u32 *destination_row = ((u32 *)framebuffer->backbuffer_memory + (u32)min_x +
                            framebuffer->width * min_y);

    u32 destination_pitch = framebuffer->width;

    for (u32 y = 0; y < blit_height; y++)
    {
        u32 *destination = destination_row;

        for (u32 x = 0; x < blit_width; x++)
        {
            u32 pixel_color = *source++;

            u8 src_alpha = (u8)(pixel_color >> texture->alpha_shift);

            u8 src_red = (u8)(pixel_color >> texture->red_shift);
            u8 src_green = (u8)(pixel_color >> texture->green_shift);
            u8 src_blue = (u8)(pixel_color >> texture->blue_shift);

            // Framebuffer format : xx RR GG BB
            u32 destination_color = *destination;

            u8 dst_red = (u8)(destination_color >> 16);
            u8 dst_green = (u8)(destination_color >> 8);
            u8 dst_blue = (u8)(destination_color);

            f32 t = src_alpha / 255.0f;

            u32 red = round_f32_to_u32(src_red * t + (1.0f - t) * dst_red);
            u32 green =
                round_f32_to_u32(src_green * t + (1.0f - t) * dst_green);
            u32 blue = round_f32_to_u32(src_blue * t + (1.0f - t) * dst_blue);

            // Framebuffer format : xx RR GG BB
            *(destination)++ = blue | (green << 8) | (red << 16) | (255 << 24);
        }

        destination_row += destination_pitch;
    }
}

#endif
