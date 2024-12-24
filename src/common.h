#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#ifdef PRISM_DEBUG
#define ASSERT(x)                                                              \
    if (!(x))                                                                  \
    {                                                                          \
        int *ptr = NULL;                                                       \
        *ptr = 0;                                                              \
    }
#define INVALID_CODE_PATH(x) ASSERT(0 && x)

#else

#define ASSERT(x)
#define INVALID_CODE_PATH(x)
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

#include "custom_math.h"

#endif
