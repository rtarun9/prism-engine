#ifndef __CUSTOM_INTRINSICS__
#define __CUSTOM_INTRINSICS__

#include "common.h"
#include <immintrin.h>

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

// TODO: Determine if using simd calculations for floor of single value is worth
// it. This is done only for educational purposes, and will need to be
// rethinked.
inline f32 floor_f32(const f32 value)
{
    __m128 floor_value = _mm_floor_ps(_mm_set_ps1(value));
    f32 result = 0.0f;
    _mm_store_ps1(&result, floor_value);

    return result;
}

inline i32 floor_f32_to_i32(const f32 value)
{
    return (i32)floor_f32(value);
}

inline u32 floor_f32_to_u32(const f32 value)
{
    return (u32)floor_f32(value);
}

inline f32 square_root_f32(const f32 value)
{
    __m128 sq_root_value = _mm_sqrt_ps(_mm_set_ps1(value));
    f32 result = 0.0f;
    _mm_store_ps1(&result, sq_root_value);

    return result;
}

inline i64 round_f64_to_i64(const f64 value)
{
    return (i64)(value + 0.5);
}

inline u64 round_f64_to_u64(const f64 value)
{
    return (u64)(value + 0.5);
}

inline i64 truncate_f64_to_i64(const f64 value)
{
    return (i64)(value);
}

inline u64 truncate_f64_to_u64(const f64 value)
{
    return (u64)(value);
}

inline f64 floor_f64(const f64 value)
{
    __m128d floor_value = _mm_floor_pd(_mm_set_pd1(value));
    f64 result = 0.0;
    _mm_store_pd1(&result, floor_value);

    return result;
}

inline i64 floor_f64_to_i64(const f64 value)
{
    return (i64)floor_f64(value);
}

inline f64 square_root_f64(const f64 value)
{
    __m128d sq_root_value = _mm_sqrt_pd(_mm_set_pd1(value));
    f64 result = 0.0f;
    _mm_store_pd1(&result, sq_root_value);

    return result;
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
#error LLVM support has not been added yet!!
#else
#error "This code is for reference only. Control flow should not reach here!!"
    {
        for (u32 i = 0; i < 32; i++)
        {
            if ((value >> i) & 1)
            {
                return i;
            }
        }

        return 0;
    }
#endif
}

#endif
