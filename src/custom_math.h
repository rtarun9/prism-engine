#ifndef __CUSTOM_MATH__
#define __CUSTOM_MATH__

#include "common.h"
#include "custom_intrinsics.h"

#define MIN(a, b) ((a < b ? a : b))
#define MAX(a, b) ((a > b ? a : b))

#pragma warning(push)
#pragma warning(disable : 4201)
typedef struct
{
    union {
        struct
        {
            f32 x;
            f32 y;
        };

        struct
        {
            f32 width;
            f32 height;
        };

        f32 m[2];
    };
    // v2f32_t -> vector2 float 32.
} v2f32_t;
#pragma warning(pop)

inline v2f32_t v2f32_add(const v2f32_t a, const v2f32_t b)
{
    v2f32_t result = {0};
    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

// Returns a - b.
inline v2f32_t v2f32_subtract(const v2f32_t a, const v2f32_t b)
{
    v2f32_t result = {0};
    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline v2f32_t v2f32_scalar_multiply(const v2f32_t a, const f32 scalar)
{
    v2f32_t result = {0};
    result.x = a.x * scalar;
    result.y = a.y * scalar;

    return result;
}

inline f32 v2f32_dot(const v2f32_t a, const v2f32_t b)
{
    return a.x * b.x + a.y * b.y;
}

inline f32 v2f32_len(const v2f32_t a)
{
    return v2f32_dot(a, a);
}

inline f32 v2f32_len_sq(const v2f32_t a)
{
    return (f32)square_root_f32(v2f32_len(a));
}

#pragma warning(push)
#pragma warning(disable : 4201)
typedef struct
{
    union {
        struct
        {
            f64 x;
            f64 y;
        };

        struct
        {
            f64 width;
            f64 height;
        };

        f64 m[2];
    };
} v2f64_t;
#pragma warning(pop)

inline v2f64_t v2f64_add(const v2f64_t a, const v2f64_t b)
{
    v2f64_t result = {0};
    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

// Returns a - b.
inline v2f64_t v2f64_subtract(const v2f64_t a, const v2f64_t b)
{
    v2f64_t result = {0};
    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline v2f64_t v2f64_scalar_multiply(const v2f64_t a, const f64 scalar)
{
    v2f64_t result = {0};
    result.x = a.x * scalar;
    result.y = a.y * scalar;

    return result;
}

inline f64 v2f64_dot(const v2f64_t a, const v2f64_t b)
{
    return a.x * b.x + a.y * b.y;
}

inline f64 v2f64_len(const v2f64_t a)
{
    return v2f64_dot(a, a);
}

inline f64 v2f64_len_sq(const v2f64_t a)
{
    return (f64)square_root_f64(v2f64_len(a));
}

inline v2f64_t v2f64_normalize(const v2f64_t a)
{
    v2f64_t result = a;
    f64 len = v2f64_len_sq(a);
    result = v2f64_scalar_multiply(a, 1.0f / len);

    return result;
}

inline v2f64_t convert_to_v2f64(const v2f32_t a)
{
    v2f64_t result = {0};
    result.x = (f64)(a.x);
    result.y = (f64)(a.y);

    return result;
}

inline v2f32_t convert_to_v2f32(const v2f64_t a)
{
    v2f32_t result = {0};
    result.x = (f32)(a.x);
    result.y = (f32)(a.y);

    return result;
}

typedef struct
{
    f32 bottom_left_x;
    f32 bottom_left_y;
    f32 width;
    f32 height;
} rectangle_t;

rectangle_t rectangle_from_center_and_origin(v2f32_t center, v2f32_t dimension)
{
    rectangle_t result = {0};

    v2f32_t half_dimension = v2f32_scalar_multiply(dimension, 0.5f);

    result.bottom_left_x = v2f32_subtract(center, half_dimension).x;
    result.bottom_left_y = v2f32_subtract(center, half_dimension).y;

    result.width = dimension.width;
    result.height = dimension.height;

    return result;
}

#endif
