#ifndef __COMMON_H__
#define __COMMON_H__

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PRISM_DEBUG
#define ASSERT(x)                                                              \
    if (!(x))                                                                  \
    {                                                                          \
        int *ptr = NULL;                                                       \
        *ptr = 0;                                                              \
    }
#else
#define ASSERT(x)
#endif

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

// Size related #defines.
#define KILOBYTE(x) (x * 1024LL)
#define MEGABYTE(x) (KILOBYTE(x) * 1024LL)
#define GIGABYTE(x) (MEGABYTE(x) * 1024LL)

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

internal u32 truncate_u64_to_u32(const u64 value)
{
    ASSERT(value <= 0xffffffff);

    u32 result = (u32)value;
    return result;
}

internal i32 truncate_i64_to_i32(const i64 value)
{
    ASSERT(value <= 0xffffffff);

    i32 result = (i32)value;
    return result;
}

internal i32 truncate_f32_to_i32(const f32 value)
{
    ASSERT(value <= 0xffffffff);

    i32 result = (i32)value;
    return result;
}

internal u32 truncate_f32_to_u32(const f32 value)
{
    ASSERT(value <= 0xffffffff);

    u32 result = (u32)value;
    return result;
}

internal u8 truncate_f32_to_u8(const f32 value)
{
    ASSERT(value <= 0xff);

    u8 result = (u8)value;
    return result;
}

internal i32 round_f32_to_i32(const f32 value)
{
    ASSERT(value <= 0xffffffff);

    i32 result = (i32)(value + 0.5f);
    return result;
}

internal u32 round_f32_to_u32(const f32 value)
{
    ASSERT(value <= 0xffffffff);

    u32 result = (u32)(value + 0.5f);
    return result;
}

internal u8 round_f32_to_u8(const f32 value)
{
    ASSERT(value <= 0xff);

    u8 result = (u8)(value + 0.5f);
    return result;
}
#endif
