#ifndef __GAME_H__
#define __GAME_H__

#include <math.h>
#include <stdint.h>
#include <stdio.h>

// NOTE: Move this over to a common header file.

// Typedefs to primitive datatypes.
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t b32;

typedef float f32;
typedef double f64;

#define true 1
#define false 0

#define pi32 3.14159265f

// #defines to make usage of 'static' based on purpose more clear.
#define internal static
#define global_variable static
#define local_persist static

typedef struct
{
    u32 *framebuffer_memory;

    u32 width;
    u32 height;
} game_offscreen_buffer_t;

typedef struct
{
    i16 *buffer;
    u32 period_in_samples;
} game_sound_buffer_t;

#endif
