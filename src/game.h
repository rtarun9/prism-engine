#ifndef __GAME_H__
#define __GAME_H__

#include "common.h"

typedef struct
{
    u32 *framebuffer_memory;

    u32 width;
    u32 height;
} game_offscreen_buffer_t;

typedef struct
{
    // buffer is an in-out parameter.
    i16 *buffer;
    u32 period_in_samples;
    u32 samples_to_output;
} game_sound_buffer_t;

#endif
