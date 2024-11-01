#ifndef __COMMON_H__
#define __COMMON_H__

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#ifdef PRISM_DEBUG
#define ASSERT(x)                                                              \
    if (!x)                                                                    \
    {                                                                          \
        __debugbreak();                                                        \
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

#endif
