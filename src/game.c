#include "game.h"

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

// NOTE: This function takes as input the top left coords and width and height.
void draw_rectangle(game_framebuffer_t *game_framebuffer, f32 top_left_x,
                    f32 top_left_y, f32 width, f32 height, f32 normalized_red,
                    f32 normalized_green, f32 normalized_blue)
{
    i32 min_x = round_f32_to_i32(top_left_x);
    i32 min_y = round_f32_to_i32(top_left_y);

    i32 max_x = round_f32_to_i32(top_left_x + width);
    i32 max_y = round_f32_to_i32(top_left_y + height);

    if (min_x < 0)
    {
        min_x = 0;
    }

    if (min_y < 0)
    {
        min_y = 0;
    }

    if (max_x > game_framebuffer->width)
    {
        max_x = game_framebuffer->width;
    }

    if (max_y > game_framebuffer->height)
    {
        max_y = game_framebuffer->height;
    }

    u8 *row = (game_framebuffer->backbuffer_memory + min_x * 4 +
               min_y * 4 * game_framebuffer->width);

    i32 pitch = game_framebuffer->width * 4;

    u32 red = round_f32_to_u32(normalized_red * 255.0f);
    u32 green = round_f32_to_u32(normalized_green * 255.0f);
    u32 blue = round_f32_to_u32(normalized_blue * 255.0f);

    // Framebuffer format : BB GG RR xx
    u32 pixel_color = blue | (green << 8) | (red << 16);

    for (i32 y = min_y; y < max_y; y++)
    {
        u32 *pixel = (u32 *)row;
        for (i32 x = min_x; x < max_x; x++)
        {
            *pixel++ = pixel_color;
        }
        row += pitch;
    }
}

internal i8 get_tile_map_value(game_tile_map_t *tile_map, u32 x, u32 y)
{
    return *(tile_map->tile_map + x + y * tile_map->tile_map_width);
}

// NOTE: If the coordinates are out of range for this tilemap, few possibilites
// are there:
// (i) There is another tilemap (dense) that the player wants to head
// to, and if so check if in that tilemap the point is empty.
// (ii) No tilemap is present adjacent to current tile map.
// (iii) Tile map is present adjacent to current one, but the the point in that
// tilemap is not empty.
typedef struct
{
    b32 point_collides_with_tilemap;

    i32 tile_map_x;
    i32 tile_map_y;
} point_and_tilemap_collision_result_t;

internal point_and_tilemap_collision_result_t check_point_and_tilemap_collision(
    game_tile_map_t *const tile_maps, const f32 x, const f32 y,
    const i32 current_tilemap_x, const i32 current_tilemap_y)
{

    // TODO: Remove the hardcoded 2 value and put it in a world struct.
    game_tile_map_t *tile_map =
        (tile_maps + current_tilemap_x + current_tilemap_y * 2);
    point_and_tilemap_collision_result_t result = {0};

    result.tile_map_x = current_tilemap_x;
    result.tile_map_y = current_tilemap_y;

    // Get the coordinates in the world.
    i32 tile_map_coord_x = truncate_f32_to_i32(x / tile_map->tile_width);
    i32 tile_map_coord_y = truncate_f32_to_i32(y / tile_map->tile_width);

    // Check if there is a tilemap where the player wants to head to.
    if (tile_map_coord_x < 0)
    {
        tile_map_coord_x = tile_map->tile_map_width + tile_map_coord_x;
        result.tile_map_x--;
    }
    else if (tile_map_coord_x >= tile_map->tile_map_width)
    {
        tile_map_coord_x = tile_map_coord_x - tile_map->tile_map_width;
        result.tile_map_x++;
    }

    if (tile_map_coord_y < 0)
    {
        tile_map_coord_y = tile_map->tile_map_height - ;
        result.tile_map_y--;
    }
    else if (tile_map_coord_y >= tile_map->tile_map_height)
    {
        tile_map_coord_y = tile_map_coord_y - tile_map->tile_map_height;
        result.tile_map_y++;
    }

    // TODO: Move this data into a world struct instead of hardcoding it.
    if (result.tile_map_x < 0 || result.tile_map_x >= 2 ||
        result.tile_map_y < 0 || result.tile_map_y >= 2)
    {
        result.point_collides_with_tilemap = 1;

        result.tile_map_y = current_tilemap_y;
        result.tile_map_x = current_tilemap_x;

        return result;
    }
    else
    {
        tile_map = (tile_maps + result.tile_map_x + result.tile_map_y * 2);

        if (get_tile_map_value(tile_map, tile_map_coord_x, tile_map_coord_y) ==
            1)
        {
            result.point_collides_with_tilemap = 1;
        }
        else
        {
            result.point_collides_with_tilemap = 0;
        }

        return result;
    }
}

__declspec(dllexport) void game_render(
    game_memory_allocator_t *restrict game_memory_allocator,
    game_framebuffer_t *restrict game_framebuffer,
    game_input_t *restrict game_input,
    platform_services_t *restrict platform_services)
{
    ASSERT(game_input != NULL);
    ASSERT(game_framebuffer->backbuffer_memory != NULL);
    ASSERT(game_memory_allocator->permanent_memory != NULL);

    ASSERT(sizeof(game_state_t) < game_memory_allocator->permanent_memory_size)

    game_state_t *game_state =
        (game_state_t *)(game_memory_allocator->permanent_memory);

    if (!game_state->is_initialized)
    {
        game_state->player_x = 150.0f;
        game_state->player_y = 150.0f;

        game_state->is_initialized = 1;

        game_state->current_tile_map_x = 0;
        game_state->current_tile_map_y = 0;

        ASSERT(platform_services != NULL);

        platform_file_read_result_t file_read_result =
            platform_services->platform_read_entire_file(__FILE__);

        platform_services->platform_write_to_file(
            "temp.txt", file_read_result.file_content_buffer,
            file_read_result.file_content_size);

        platform_services->platform_close_file(
            file_read_result.file_content_buffer);
    }

// Create a simple tilemap of dimensions 17 X 9.
// Why 17 instead of 16? so that the tile map will have a clear center
// pixel.
#define TILE_MAP_WIDTH 17
#define TILE_MAP_HEIGHT 9

    // NOTE: Test of dense tile map (2x2).
    // Index of the tile maps:
    // 00 01
    // 10 11
    const u8 tile_map_level_10[TILE_MAP_HEIGHT][TILE_MAP_WIDTH] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

    const u8 tile_map_level_11[TILE_MAP_HEIGHT][TILE_MAP_WIDTH] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1}};

    const u8 tile_map_level_00[TILE_MAP_HEIGHT][TILE_MAP_WIDTH] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

    const u8 tile_map_level_01[TILE_MAP_HEIGHT][TILE_MAP_WIDTH] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};

    // NOTE: Index is Y X, not X Y
    game_tile_map_t world_tile_maps[2][2];

    // The tile map changes based on where player wants to move.
    world_tile_maps[0][0].tile_map = (u8 *)tile_map_level_00;
    world_tile_maps[0][0].tile_map_width = TILE_MAP_WIDTH;
    world_tile_maps[0][0].tile_map_height = TILE_MAP_HEIGHT;

    // NOTE:
    // So this math logic is a bit weird, so get ready :
    // Framebuffer's total width and height is assumed to be having a 16:9
    // aspect ratio. The width and height will be so that the tiel map entirely
    // overlaps the framebuffer.
    world_tile_maps[0][0].tile_width =
        (f32)game_framebuffer->width / (TILE_MAP_WIDTH - 1);
    world_tile_maps[0][0].tile_height = world_tile_maps[0][0].tile_width;

    // Clear the screen.

    // Rendering offsets.
    // This is done because the tile width and tile map width are not the same
    // (see the division by 16 for tile width and number of columns of tile
    // map).
    // The borders aren't too important, and its fine if (atleast in the X
    // direction) only 1/2 of the tile in the left and right border is seen.

    f32 tile_map_rendering_upper_left_offset_x =
        -(f32)world_tile_maps[0][0].tile_width / 2.0f;
    f32 tile_map_rendering_upper_left_offset_y = 0.0f;

    world_tile_maps[0][1] = world_tile_maps[0][0];
    world_tile_maps[0][1].tile_map = (u8 *)tile_map_level_01;

    world_tile_maps[1][1] = world_tile_maps[0][0];
    world_tile_maps[1][1].tile_map = (u8 *)tile_map_level_11;

    world_tile_maps[1][0] = world_tile_maps[0][0];
    world_tile_maps[1][0].tile_map = (u8 *)tile_map_level_10;

    // Update player position based on input.
    // Movement speed is in pixels / second.
    f32 player_movement_speed = 4.0f;
    f32 player_new_x_position = 0.0f;
    f32 player_new_y_position = 0.0f;

    player_new_x_position +=
        (game_input->keyboard_state.key_a.is_key_down ? -1 : 0);
    player_new_x_position +=
        (game_input->keyboard_state.key_d.is_key_down ? 1 : 0);

    player_new_y_position +=
        (game_input->keyboard_state.key_w.is_key_down ? -1 : 0);
    player_new_y_position +=
        (game_input->keyboard_state.key_s.is_key_down ? 1 : 0);

    player_new_x_position *= player_movement_speed;
    player_new_y_position *= player_movement_speed;

    player_new_x_position += game_state->player_x;
    player_new_y_position += game_state->player_y;

    game_tile_map_t *tile_map = (game_tile_map_t *)world_tile_maps;

    // The player position will be at the middle bottom of the player sprite
    // (players feet in technical terms?)
    // The player center is not used since the player can go near a wall,
    // but will not stop immediately as soon as the player sprite collides
    // with the wall.
    const f32 player_sprite_width = tile_map->tile_width * 0.75f;
    const f32 player_sprite_height = tile_map->tile_height * 0.9f;

    {
        point_and_tilemap_collision_result_t player_position_collision_check =
            check_point_and_tilemap_collision(
                tile_map, player_new_x_position, player_new_y_position,
                game_state->current_tile_map_x, game_state->current_tile_map_y);

        point_and_tilemap_collision_result_t
            player_bottom_right_corner_collision_check =
                check_point_and_tilemap_collision(
                    tile_map,
                    player_new_x_position + player_sprite_width * 0.5f,
                    player_new_y_position, game_state->current_tile_map_x,
                    game_state->current_tile_map_y);

        point_and_tilemap_collision_result_t
            player_bottom_left_corner_collision_check =
                check_point_and_tilemap_collision(
                    tile_map,
                    player_new_x_position - player_sprite_width * 0.5f,
                    player_new_y_position, game_state->current_tile_map_x,
                    game_state->current_tile_map_y);

        // The player is 'collided' if even 1 of the checks fail.
        if ((player_position_collision_check.point_collides_with_tilemap == 0 &&
             player_bottom_left_corner_collision_check
                     .point_collides_with_tilemap == 0 &&
             player_bottom_right_corner_collision_check
                     .point_collides_with_tilemap == 0))
        {
            // Player can move to new position!!! Check for change in current
            // tilemap.
            // Note that this is ONLY done by checking the players position (i.e
            // player_position_collision_check). This should make the code
            // simpler as well.
            if (game_state->current_tile_map_x !=
                    player_position_collision_check.tile_map_x ||
                game_state->current_tile_map_y !=
                    player_position_collision_check.tile_map_y)
            {
                game_state->current_tile_map_x =
                    player_position_collision_check.tile_map_x;
                game_state->current_tile_map_y =
                    player_position_collision_check.tile_map_y;
            }

            // Check if the new position is out of bounds.
            // Since the current tile map coords are updated, the player
            // position can be set back to tile map local space.
            if (player_new_x_position >
                tile_map->tile_width * tile_map->tile_map_width)
            {
                game_state->player_x = player_new_x_position -=
                    tile_map->tile_width * tile_map->tile_map_width;
            }
            else
            {
                game_state->player_x = player_new_x_position;
            }

            if (player_new_y_position >
                tile_map->tile_height * tile_map->tile_map_height)
            {
                game_state->player_y = player_new_y_position -=
                    tile_map->tile_height * tile_map->tile_map_height;
            }
            else
            {
                // BUG: This won't work, because in the above (if block)
                // player_y will become in tile map coord range, but when moving
                // to a tile map with lower y coord, this piece of code will
                // fail.
                game_state->player_y = player_new_y_position;
            }
        }
    }

    tile_map =
        ((game_tile_map_t *)world_tile_maps + game_state->current_tile_map_x +
         game_state->current_tile_map_y * 2);

    // Clear screen.
    draw_rectangle(game_framebuffer, 0.0f, 0.0f, (f32)game_framebuffer->width,
                   (f32)game_framebuffer->height, 1.0f, 0.0f, 0.0f);
    // Draw the tile map.
    for (i32 y = 0; y < TILE_MAP_HEIGHT; ++y)
    {
        for (i32 x = 0; x < TILE_MAP_WIDTH; ++x)
        {
            const f32 top_left_x = tile_map_rendering_upper_left_offset_x +
                                   x * tile_map->tile_width;
            const f32 top_left_y = tile_map_rendering_upper_left_offset_y +
                                   y * tile_map->tile_height;

            if (get_tile_map_value(tile_map, x, y) == 1)
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)tile_map->tile_width,
                               (f32)tile_map->tile_height, 1.0f, 1.0f, 1.0f);
            }
            else
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)tile_map->tile_width,
                               (f32)tile_map->tile_height, 0.0f, 0.0f, 0.0f);
            }
        }
    }

    const f32 player_top_left_x =
        game_state->player_x - 0.5f * player_sprite_width;
    const f32 player_top_left_y = game_state->player_y - player_sprite_height;

    draw_rectangle(
        game_framebuffer,
        (f32)tile_map_rendering_upper_left_offset_x + player_top_left_x,
        (f32)tile_map_rendering_upper_left_offset_y + player_top_left_y,
        (f32)player_sprite_width, (f32)player_sprite_height, 1.0f, 0.0f, 0.0f);
}
