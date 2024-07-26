#ifndef __GAME_H__
#define __GAME_H__

// All platform - independant game related code is present here.
// These functions will be called via the platform layer specific X_main.c
// code.
#include "types.h"

typedef struct
{
    u8 *backbuffer_memory;
    i32 width;
    i32 height;
} game_framebuffer_t;

internal void game_render(game_framebuffer_t *game_framebuffer, i32 blue_offset,
                          i32 green_offset);

#endif
