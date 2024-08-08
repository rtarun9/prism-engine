#ifndef __COMMON__H__
#define __COMMON__H__

#include <stdint.h>

// Primitive data type aliases.
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint32_t b32;

typedef float f32;
typedef double f64;

// #defines for the static keyword for explicitness.
// Since a unity build is used, marking the functions as static will let the
// compiler know that no external linkage is required.
#define internal static
#define global_variable static
#define local_persist static

// #defines that are related to memory.
#define KILOBYTE(x) (x * 1024LL)
#define MEGABYTE(x) (KILOBYTE(x) * 1024LL)
#define GIGABYTE(x) (MEGABYTE(x) * 1024LL)
#define TERABYTE(x) (GIGABYTE(x) * 1024LL)

// #defines that are related to assets.
#ifdef PRISM_DEBUG
#define ASSERT(x)                                                              \
    if (!(x))                                                                  \
    {                                                                          \
        int *null_ptr = NULL;                                                  \
        *null_ptr = 0;                                                         \
    }
#endif

#ifndef PRISM_DEBUG
#define ASSERT(x)
#endif

internal u64 get_nearest_multiple(u64 value, u64 multiple)
{
    if (value % multiple == 0)
    {
        return value;
    }

    return value + multiple - (value % multiple);
}

#endif
