#ifndef __CUSTOM_MATH__
#define __CUSTOM_MATH__

#include "common.h"

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
} vector2_t;
#pragma warning(pop)

inline vector2_t vector2_add(const vector2_t a, const vector2_t b)
{
    vector2_t result = {0};
    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

// Returns a - b.
inline vector2_t vector2_subtract(const vector2_t a, const vector2_t b)
{
    vector2_t result = {0};
    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline vector2_t vector2_scalar_multiply(const vector2_t a, const f32 scalar)
{
    vector2_t result = {0};
    result.x = a.x * scalar;
    result.y = a.y * scalar;

    return result;
}

inline f32 vector2_dot(const vector2_t a, const vector2_t b)
{
    return a.x * b.x + a.y * b.y;
}

inline f32 vector2_len(const vector2_t a)
{
    return vector2_dot(a, a);
}

// TODO: Remove usage of math.h functions.
inline f32 vector2_len_sq(const vector2_t a)
{
    return (f32)sqrt(vector2_len(a));
}

#endif
