#ifndef __CUSTOM_INTRINSICS__
#define __CUSTOM_INTRINSICS__

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

inline i32 floor_f32_to_i32(const f32 value)
{
    return (i32)floorf(value);
}

inline u32 get_index_of_lsb_set(const u32 value)
{
#if defined(COMPILER_MSVC)
    {
        unsigned long index = 0;
        if (_BitScanForward(&index, (unsigned long)value))
        {
            return index;
        }
        return 0;
    }
#elif defined(COMPILER_LLVM)
#error LLVM support has not been addedy yet!!
#else
#error "This code is for reference only. Control flow should not reach here!!"
    for (u32 i = 0; i < 32; i++)
    {
        if ((value >> i) & 1)
        {
            return i;
        }
    }

    return 0;
#endif
}

#endif
