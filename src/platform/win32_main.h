#ifndef __WIN32_MAIN_H__
#define __WIN32_MAIN_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../common.h"

#include "../game.h"

typedef struct
{
    BITMAPINFOHEADER bitmap_info_header;
    u8 *backbuffer_memory;
} win32_framebuffer_t;

typedef struct
{
    i32 width;
    i32 height;
} win32_dimensions_t;

typedef struct
{
    u8 key_states[256];
} win32_keyboard_state_t;

// All memory required by the game and platform (in the future*) will be
// obtained from win32_memory_t, which contains a single pointer that points to
// memory allocated upfront. No more allocations will be done by the game (in
// future***)
typedef struct
{
    u64 permanent_memory_size;
    u8 *permanent_memory;
} win32_memory_t;

// NOTE: As the game code will be present in a DLL and will be loaded at run
// time, the game func pointers (and DLL handle) will be encapsulated in its own
// struct.
typedef FUNC_GAME_UPDATE_AND_RENDER(game_update_and_render_t);

// NOTE: Each frame the platform layer will ccheck if the last modification time
// has changed. If yes, the DLL is re-loaded.
typedef struct
{
    HMODULE game_dll_module;
    FILETIME dll_last_modification_time;
    game_update_and_render_t *game_update_and_render;
} game_code_t;

// NOTE: To support loop recording, the following info is required:
// File handles for writing keyboard input to, and for reading input from the
// file. Also, the 'amount of data' recorded is required, so that we can loop
// back whenever we playback the data and read all the recorded data. The
// 'state' that the engine is in, either recording, or playing back.

// For game memory, a different approach is taken.
// The win32 state struct has its own memory, that equals the size of game
// permanent memory.
// When recording starts, take a snapshot (i.e memcpy) game state into this
// game_memory pointer. From then on, when playback starts, you use the
// win32_state game memory pointer. When recording stops, you can start using
// the actual game memory pointer.
enum WIN32_STATES
{
    WIN32_STATE_RECORDING,
    WIN32_STATE_PLAYBACK,
    WIN32_STATE_NONE,
};

typedef struct
{
    HANDLE input_recording_file_handle;

    HANDLE input_playback_file_handle;

    u8 *game_memory;
    u64 game_memory_size;

    enum WIN32_STATES current_state;

} win32_state_t;

#endif
