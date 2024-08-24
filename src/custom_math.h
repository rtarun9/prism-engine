#ifndef __CUSTOM_MATH__
#define __CUSTOM_MATH__

#include "common.h"

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
#endif
