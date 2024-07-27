#ifndef __TYPES__H__
#define __TYPES__H__

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

typedef uint8_t b8;

typedef float f32;
typedef double f64;

// #defines for the static keyword for explicitness.
#define internal static
#define global_variable static
#define local_persist static

// #defines that are related to memory.
// note(rtarun9) : Does it make sense to have these macros in types.h?
#define KILOBYTE(x) (x * 1024LL)
#define MEGABYTE(x) (KILOBYTE(x) * 1024LL)
#define GIGABYTE(x) (MEGABYTE(x) * 1024LL)
#define TERABYTE(x) (GIGABYTE(x) * 1024LL)

#endif
