#ifndef __COMMON__H__
#define __COMMON__H__

// Determine which compiler is being used.
#define COMPILER_MSVC 0
#define COMPILER_LLVM 0

#if defined(_MSC_VER)
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#error Only MSVC is currently supported!!!
#endif

#if defined(COMPILER_MSVC)
#pragma intrinsic(_BitScanForward)
#endif

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
#define BYTE(x) (x)
#define KILOBYTE(x) (x * 1024LL)
#define MEGABYTE(x) (KILOBYTE(x) * 1024LL)
#define GIGABYTE(x) (MEGABYTE(x) * 1024LL)
#define TERABYTE(x) (GIGABYTE(x) * 1024LL)

// #defines to get size of array.
#define ARRAY_SIZE(x) ((sizeof(x) / sizeof(x[0])))

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

internal i32 is_power_of_2(size_t value)
{ // In binary, if a number if a power of 2,
  // then only a SINGLE bit will be set.
  // Moreoever, value - 1 will be all 1's.

    return (value != 0) && (value & (value - 1)) == 0;
}

internal u8 *align_memory_address(u8 *value, size_t alignment_size)
{
    ASSERT(is_power_of_2(alignment_size) == 1);

    // As alignment size will be a power of 2, to do a fast modulus operator
    // doing value & (alignment_size - 1) will be sufficient.
    size_t modulo = (size_t)value & (alignment_size - 1);
    size_t result = (size_t)value;

    if (modulo != 0)
    {
        result += alignment_size - modulo;
    }

    return (u8 *)result;
}

#endif
