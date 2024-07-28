#ifndef __WIN32_TYPES_H__
#define __WIN32_TYPES_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../types.h"

// note(rtarun9) : The biHeight parameter in bitmap_info_header is negative (so
// that the top left corner is the origin).
typedef struct
{
    BITMAPINFOHEADER bitmap_info_header;
    u8 *backbuffer_memory;
} win32_offscreen_framebuffer_t;

typedef struct
{
    i32 width;
    i32 height;
} win32_dimensions_t;

typedef struct
{
    b8 key_states[256];
} win32_keyboard_state_t;

// To minimize memory allocators, memory is allocated upfront and used.
typedef struct
{
    u64 permanent_memory_size;
    u8 *permanent_memory;
} win32_memory_allocator_t;

internal win32_dimensions_t get_dimensions_for_window(const HWND window_handle);

internal void win32_resize_bitmap(const i32 width, const i32 height);

// Update the backbuffer by blitting (and stretching the framebuffer if
// required) to the device context of window.
// This is done because the bitmap's dimensions is FIXED.
internal void win32_update_backbuffer(const HDC device_context,
                                      const i32 window_width,
                                      const i32 window_height);

#endif
