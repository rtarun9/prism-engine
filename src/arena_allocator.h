#ifndef __ARENA_ALLOCATOR_H__
#define __ARENA_ALLOCATOR_H__

#include "common.h"

// The arena is a linear allocator, which has a base pointer and offset.
// When alloc is called, the pointer returned is the base_ptr + offset. Then,
// the offset is adjusted accordingly.
typedef struct
{
    u8 *base_ptr;
    size_t offset;
    size_t size;
} arena_allocator_t;

#define DEFAULT_ALIGNMENT_VALUE sizeof(i32)

internal void arena_init(arena_allocator_t *arena, u8 *arena_base_ptr,
                         size_t arena_size);

// NOTE: Alignment and size are different, as the same function can be used to
// allocate arrays. In such cases, size will be size of the array, while
// alignment will be the alignment required by each element of the array.
internal u8 *arena_alloc_array(arena_allocator_t *arena, size_t num_elements,
                               size_t size_per_element, size_t alignment);

internal u8 *arena_alloc_aligned(arena_allocator_t *arena, size_t size,
                                 size_t alignment);

internal u8 *arena_alloc_default_aligned(arena_allocator_t *arena, size_t size);

internal void arena_reset(arena_allocator_t *arena);

#endif
