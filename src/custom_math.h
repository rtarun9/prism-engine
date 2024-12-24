#ifndef __CUSTOM_MATH_H__
#define __CUSTOM_MATH_H__

// NOTE: This header file should be removed in future!!
#include <math.h>

// Trunc / floor /  round functions.
inline u32 truncate_u64_to_u32(const u64 value)
{
    ASSERT(value <= 0xffffffff);

    u32 result = (u32)value;
    return result;
}

inline i32 truncate_i64_to_i32(const i64 value)
{
    ASSERT(value <= 0x7fffffff);

    i32 result = (i32)value;
    return result;
}

inline i32 truncate_f32_to_i32(const f32 value)
{
    ASSERT(value <= 0x7fffffff);

    i32 result = (i32)value;
    return result;
}

inline u32 truncate_f32_to_u32(const f32 value)
{
    ASSERT(value <= 0xffffffff);

    u32 result = (u32)value;
    return result;
}

inline u8 truncate_f32_to_u8(const f32 value)
{
    ASSERT(value <= 0xff);

    u8 result = (u8)value;
    return result;
}

inline i32 round_f32_to_i32(const f32 value)
{
    ASSERT(value <= 0x7fffffff);

    i32 result = (i32)(value + 0.5f);
    return result;
}

inline u32 round_f32_to_u32(const f32 value)
{
    ASSERT(value <= 0xffffffff);

    u32 result = (u32)(value + 0.5f);
    return result;
}

inline u8 round_f32_to_u8(const f32 value)
{
    ASSERT(value <= 0xff);

    u8 result = (u8)(value + 0.5f);
    return result;
}

#endif
