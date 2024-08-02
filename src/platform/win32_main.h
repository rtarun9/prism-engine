#ifndef __WIN32_MAIN_H__
#define __WIN32_MAIN_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../common.h"

#include "../game.h"

// NOTE: The biHeight parameter in bitmap_info_header is negative (so
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

// NOTE: As the game code will be present in a DLL and will be loaded at run
// time, the game func pointers (and DLL handle) will be encapsulated in its own
// struct.
typedef FUNC_GAME_RENDER(game_render_t);

// NOTE: Each frame the platform layer will ccheck if the last modification time
// has changed. If yes, the DLL is re-loaded.
typedef struct
{
    HMODULE game_dll_module;
    FILETIME dll_last_modification_time;
    game_render_t *game_render;
} game_code_t;

#endif
