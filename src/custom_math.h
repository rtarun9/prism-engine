#ifndef __CUSTOM_MATH__
#define __CUSTOM_MATH__

#include "common.h"

inline i32 round_f32_to_i32(const f32 value)
{
    return (i32)(value + 0.5f);
}

inline u32 round_f32_to_u32(const f32 value)
{
    return (u32)(value + 0.5f);
}

inline i32 truncate_f32_to_i32(const f32 value)
{
    return (i32)(value);
}

inline u32 truncate_f32_to_u32(const f32 value)
{
    return (u32)(value);
}

// TODO: Use custom functions instead.
#include <math.h>
inline f32 floor_f32(const f32 value)
{
    return floorf(value);
}

#endif
