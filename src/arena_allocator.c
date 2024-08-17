#include "arena_allocator.h"

internal void arena_init(arena_allocator_t *restrict arena,
                         u8 *restrict arena_base_ptr, size_t arena_size)
{
    ASSERT(arena);

    arena->base_ptr = arena_base_ptr;
    arena->offset = 0;
    arena->size = arena_size;
}

// NOTE: Alignment and size are different, as the same function can be used to
// allocate arrays. In such cases, size will be size of the array, while
// alignment will be the alignment required by each element of the array.
internal u8 *arena_alloc_array(arena_allocator_t *arena, size_t num_elements,
                               size_t size_per_element, size_t alignment)
{
    ASSERT(is_power_of_2(alignment));

    ASSERT(arena);
    ASSERT(arena->base_ptr);

    size_t size = num_elements * size_per_element;

    // Corrected result with the pointer returned takes into account memory
    // alignment.
    // NOTE: This computation should note take size into account, as size will
    // only determine the alignemnt and new offset for future arena allocations.
    u8 *memory_aligned_result =
        align_memory_address(arena->base_ptr + arena->offset, alignment);

    // Get the relative offset from the base_ptr.
    size_t base_ptr_relative_offset = memory_aligned_result - arena->base_ptr;

    if (base_ptr_relative_offset + size > arena->size)
    {
        return NULL;
    }

    arena->offset = base_ptr_relative_offset + size;

    ASSERT((size_t)(memory_aligned_result) % alignment == 0);

    return memory_aligned_result;
}

internal u8 *arena_alloc_aligned(arena_allocator_t *arena, size_t size,
                                 size_t alignment)
{
    return arena_alloc_array(arena, 1, size, alignment);
}

internal u8 *arena_alloc_default_aligned(arena_allocator_t *arena, size_t size)
{
    return arena_alloc_array(arena, 1, size, DEFAULT_ALIGNMENT_VALUE);
}

internal void arena_reset(arena_allocator_t *arena)
{
    ASSERT(arena);
    ASSERT(arena->base_ptr);

    arena->offset = 0;
}
