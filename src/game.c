#include "game.h"

#include "custom_math.h"

// NOTE: This function takes as input the top left coords and width and height.
void draw_rectangle(game_framebuffer_t *game_framebuffer, f32 top_left_x,
                    f32 top_left_y, f32 width, f32 height, f32 normalized_red,
                    f32 normalized_green, f32 normalized_blue)
{
    ASSERT(width >= 0.0f);
    ASSERT(height >= 0.0f);

    ASSERT(normalized_red >= 0.0f);
    ASSERT(normalized_green >= 0.0f);
    ASSERT(normalized_blue >= 0.0f);

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

internal u8 get_value_in_tile_chunk(game_world_t *restrict world,
                                    game_tile_chunk_t *restrict tile_chunk,
                                    u32 x, u32 y)
{
    ASSERT(world);
    ASSERT(tile_chunk);

    return *(tile_chunk->tile_chunk + x + y * world->tile_chunk_width);
}

internal u8 get_value_of_tile_in_world(game_world_t *restrict world,
                                       u32 absolute_tile_x, u32 absolute_tile_y)
{
    ASSERT(world);

    u32 chunk_x =
        truncate_f32_to_u32((f32)absolute_tile_x / world->tile_chunk_width);
    u32 chunk_y =
        truncate_f32_to_u32((f32)absolute_tile_y / world->tile_chunk_height);

    u32 tile_x = absolute_tile_x -
                 truncate_f32_to_u32((f32)chunk_x * world->tile_chunk_width);
    u32 tile_y = absolute_tile_y -
                 truncate_f32_to_u32((f32)chunk_y * world->tile_chunk_height);

    if (tile_x > TILE_CHUNK_WIDTH)
    {
        tile_x = 0;
        chunk_x++;
    }

    if (tile_y > TILE_CHUNK_HEIGHT)
    {
        tile_y = 0;
        chunk_y++;
    }

    // Assume a toroidal coordinate system, where if the tile chunk queried is
    // out of bounds, it will simply wrap over.
    if (chunk_x >= WORLD_TILE_CHUNK_WIDTH)
    {
        chunk_x = 0;
    }

    if (chunk_y >= WORLD_TILE_CHUNK_HEIGHT)
    {
        chunk_y = 0;
    }

    game_tile_chunk_t *tile_chunk =
        world->tile_chunks + chunk_x + chunk_y * WORLD_TILE_CHUNK_WIDTH;

    return get_value_in_tile_chunk(world, tile_chunk, tile_x, tile_y);
}

// Returns the 'corrected' position and deconstructs information such as the
// offset inside of a tile, the tile offset in tile chunk, etc.
// Note that x and y are relative to the current tile chunk.
internal world_position_t get_world_position(game_world_t *world, f32 x, f32 y,
                                             i32 chunk_x, i32 chunk_y)
{
    world_position_t result = {0};

    i32 tile_x = truncate_f32_to_i32(floor_f32(x / world->tile_width));
    i32 tile_y = truncate_f32_to_i32(floor_f32(y / world->tile_height));

    result.tile_relative_x = x - tile_x * world->tile_width;
    result.tile_relative_y = y - tile_y * world->tile_height;

    // Check if there is a tilemap where the player wants to head to.
    if (tile_x < 0)
    {
        tile_x = world->tile_chunk_width - 1;
        chunk_x--;
    }
    else if (tile_x >= world->tile_chunk_width)
    {
        tile_x = 0;
        chunk_x++;
    }

    if (tile_y < 0)
    {
        tile_y = world->tile_chunk_height - 1;
        chunk_y--;
    }
    else if (tile_y >= world->tile_chunk_height)
    {
        tile_y = 0;
        chunk_y++;
    }

    result.tile_index_x = SET_TILE_INDEX(result.tile_index_x, tile_x);

    result.tile_index_y = SET_TILE_INDEX(result.tile_index_y, tile_y);

    result.tile_index_x = SET_TILE_CHUNK_INDEX(result.tile_index_x, chunk_x);

    result.tile_index_y = SET_TILE_CHUNK_INDEX(result.tile_index_y, chunk_y);

    return result;
}

// Returns 1 if collision has occured, and 0 if it did not.
internal b32 check_point_and_tile_chunk_collision(
    game_world_t *const world, const f32 x, const f32 y,
    const i32 current_tile_chunk_x, const i32 current_tile_chunk_y)
{

    world_position_t position = get_world_position(
        world, x, y, current_tile_chunk_x, current_tile_chunk_y);

    i32 tile_chunk_x = GET_TILE_CHUNK_INDEX(position.tile_index_x);
    i32 tile_chunk_y = GET_TILE_CHUNK_INDEX(position.tile_index_y);

    i32 tile_x = GET_TILE_INDEX(position.tile_index_x);
    i32 tile_y = GET_TILE_INDEX(position.tile_index_y);

    if (tile_chunk_x < 0 || tile_chunk_x >= WORLD_TILE_CHUNK_WIDTH ||
        tile_chunk_y < 0 || tile_chunk_y >= WORLD_TILE_CHUNK_HEIGHT)
    {
        return 1;
    }
    else
    {
        if (get_value_of_tile_in_world(world, tile_x, tile_y) == 1)
        {
            return 1;
        }

        return 0;
    }
}

u8 *push_memory(game_memory_arena_t *arena, size_t size)
{
    ASSERT(arena);
    ASSERT(arena->memory_block);
    ASSERT(arena->memory_used + size <= arena->memory_block_size);

    u8 *result = arena->memory_block;
    arena->memory_block += size;
    arena->memory_used += size;

    return result;
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

        game_state->current_tile_chunk_x = 0;
        game_state->current_tile_chunk_y = 0;

        // For now, it is assumed that a single meter equals 1/xth of the
        // framebuffer width.
        game_state->pixels_to_meters = game_framebuffer->width / 20.0f;

        // Initialize memory arena.
        game_state->memory_arena.memory_block =
            (game_memory_allocator->permanent_memory + sizeof(game_state_t));
        game_state->memory_arena.memory_used = 0;
        game_state->memory_arena.memory_block_size =
            game_memory_allocator->permanent_memory_size - sizeof(game_state_t);

        ASSERT(platform_services != NULL);

        platform_file_read_result_t file_read_result =
            platform_services->platform_read_entire_file(__FILE__);

        platform_services->platform_write_to_file(
            "temp.txt", file_read_result.file_content_buffer,
            file_read_result.file_content_size);

        platform_services->platform_close_file(
            file_read_result.file_content_buffer);

        // Setup a basic tile chunk.
        game_tile_chunk_t *basic_tile_chunk = (game_tile_chunk_t *)push_memory(
            &game_state->memory_arena, sizeof(game_tile_chunk_t) *
                                           WORLD_TILE_CHUNK_HEIGHT *
                                           WORLD_TILE_CHUNK_WIDTH);

        basic_tile_chunk->tile_chunk = (u8 *)push_memory(
            &game_state->memory_arena,
            sizeof(u8) * TILE_CHUNK_WIDTH * TILE_CHUNK_HEIGHT);

        // Set the top border as obstacles.
        for (i32 x = 0; x < TILE_CHUNK_WIDTH; x++)
        {
            *(basic_tile_chunk->tile_chunk + x) = 1;
        }

        // Set the bottom border as obstacles.
        for (i32 x = 0; x < TILE_CHUNK_WIDTH; x++)
        {
            *(basic_tile_chunk->tile_chunk + x + TILE_CHUNK_HEIGHT -
              1 * TILE_CHUNK_WIDTH) = 1;
        }

        // Set the left border as obstacles.
        for (i32 y = 0; y < TILE_CHUNK_HEIGHT; y++)
        {
            *(basic_tile_chunk->tile_chunk + 0 + y * TILE_CHUNK_WIDTH) = 1;
        }

        // Set the right border as obstacles.
        for (i32 y = 0; y < TILE_CHUNK_HEIGHT; y++)
        {
            *(basic_tile_chunk->tile_chunk + TILE_CHUNK_WIDTH - 1 +
              y * TILE_CHUNK_WIDTH) = 1;
        }

        game_world_t *game_world = (game_world_t *)push_memory(
            &game_state->memory_arena, sizeof(game_world_t));

        game_world->tile_chunk_width = TILE_CHUNK_WIDTH;
        game_world->tile_chunk_height = TILE_CHUNK_HEIGHT;

        game_world->tile_width = game_state->pixels_to_meters;
        game_world->tile_height = game_world->tile_width;

        game_world->tile_chunks = basic_tile_chunk;

        game_state->game_world = game_world;

        game_state->is_initialized = 1;
    }

    // Clear the screen.
    game_world_t *game_world = (game_world_t *)game_state->game_world;

    // Update player position based on input.
    // Movement speed is in meters / second.
    f32 player_movement_speed = 0.25f * game_state->pixels_to_meters;
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

    player_new_x_position =
        player_new_x_position * player_movement_speed + game_state->player_x;
    player_new_y_position =
        player_new_y_position * player_movement_speed + game_state->player_y;

    // The player position will be at the middle bottom of the player sprite
    // (players feet in technical terms?)
    // The player center is not used since the player can go near a wall,
    // but will not stop immediately as soon as the player sprite collides
    // with the wall.
    const f32 player_sprite_width = game_state->pixels_to_meters * 0.75f;
    const f32 player_sprite_height = game_state->pixels_to_meters * 0.9f;

    {
        if (!(check_point_and_tile_chunk_collision(
                  game_world, player_new_x_position, player_new_y_position,
                  game_state->current_tile_chunk_x,
                  game_state->current_tile_chunk_y) ||

              check_point_and_tile_chunk_collision(
                  game_world,
                  player_new_x_position + player_sprite_width * 0.5f,
                  player_new_y_position, game_state->current_tile_chunk_x,
                  game_state->current_tile_chunk_y) ||

              check_point_and_tile_chunk_collision(
                  game_world,
                  player_new_x_position - player_sprite_width * 0.5f,
                  player_new_y_position, game_state->current_tile_chunk_x,
                  game_state->current_tile_chunk_y)))
        {

            // Player can move to new position!!! Check for change in current
            // tilemap.
            // Note that this is ONLY done by checking the players position (i.e
            // player_position_collision_check). This should make the code
            // simpler as well.
            world_position_t position = get_world_position(
                game_world, player_new_x_position, player_new_y_position,
                game_state->current_tile_chunk_x,
                game_state->current_tile_chunk_y);

            game_state->current_tile_chunk_y =
                GET_TILE_CHUNK_INDEX(position.tile_index_y);
            game_state->current_tile_chunk_x =
                GET_TILE_CHUNK_INDEX(position.tile_index_x);

            game_state->player_x =
                GET_TILE_INDEX(position.tile_index_x) * game_world->tile_width +
                position.tile_relative_x;

            game_state->player_y = GET_TILE_INDEX(position.tile_index_y) *
                                       game_world->tile_height +
                                   position.tile_relative_y;
        }
    }

    // Clear screen.
    draw_rectangle(game_framebuffer, 0.0f, 0.0f, (f32)game_framebuffer->width,
                   (f32)game_framebuffer->height, 1.0f, 0.0f, 0.0f);

    // Draw the tile map.
    // This draws all the tiles (or atleast as much as possible).
    // What I want is for the player to be at the center, so x and y is the tile
    // indices that are around the player.
    world_position_t player_position = get_world_position(
        game_world, game_state->player_x, game_state->player_y,
        game_state->current_tile_chunk_x, game_state->current_tile_chunk_y);

    // At any moment of time, the number of tiles that can be viewed on the
    // screen is FIXED.
    // Because of the offset applied, it makes sense to have a few extra tiles
    // to view (which may very well be culled) in each direction.
    const i32 tiles_viewed_x =
        truncate_f32_to_i32(game_framebuffer->width / game_world->tile_width) +
        2;

    const i32 tiles_viewed_y = truncate_f32_to_i32(game_framebuffer->height /
                                                   game_world->tile_height) +
                               4;

    // Rendering offsets. The value are computed such that the player is at the
    // center of the screen.
    f32 tile_map_rendering_upper_left_offset_x =
        game_framebuffer->width / 2.0f - player_sprite_width / 2 -
        player_new_x_position;

    f32 tile_map_rendering_upper_left_offset_y =
        game_framebuffer->height / 2.0f - player_sprite_height / 2 -
        player_new_y_position;

    for (i32 _y = -tiles_viewed_y / 2; _y <= tiles_viewed_y / 2; ++_y)
    {
        for (i32 _x = -tiles_viewed_x / 2; _x <= tiles_viewed_x / 2; ++_x)
        {
            // Compute x and y tile index such that the tiles visualized are
            // around the player.
            i32 x = _x + GET_TILE_INDEX(player_position.tile_index_x);
            i32 y = _y + GET_TILE_INDEX(player_position.tile_index_y);

            // When x and y are zero, that tile
            // should be rendered at the center
            // of the screen.
            f32 top_left_x = tile_map_rendering_upper_left_offset_x +
                             game_world->tile_width * (x);

            f32 top_left_y = tile_map_rendering_upper_left_offset_y +
                             (y)*game_world->tile_height;

            if (_x == 0 && _y == 0)
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)game_world->tile_width,
                               (f32)game_world->tile_height, 0.0f, 0.0f, 1.0f);
            }
            else if (get_value_of_tile_in_world(game_world, x, y))
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)game_world->tile_width,
                               (f32)game_world->tile_height, 1.0f, 1.0f, 1.0f);
            }
            else
            {
                draw_rectangle(game_framebuffer, (f32)top_left_x,
                               (f32)top_left_y, (f32)game_world->tile_width,
                               (f32)game_world->tile_height, 0.0f, 0.0f, 0.0f);
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
