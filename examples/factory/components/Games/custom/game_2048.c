/*
 * 2048 rules used by the GUI Guider port.
 * A move compacts a row/column, merges each tile once, then compacts again.
 * No LVGL or hardware dependency: the same code is exercised by host tests.
 */
#include "game_2048.h"
#include <string.h>

bool game_2048_can_move(const game_2048_state_t *game)
{
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const uint32_t n = game->cells[y][x];
            if (!n || (x < 3 && n == game->cells[y][x + 1]) ||
                (y < 3 && n == game->cells[y + 1][x])) {
                return true;
            }
        }
    }
    return false;
}

static void update_status(game_2048_state_t *game)
{
    game->over = !game_2048_can_move(game);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (game->cells[y][x] >= 2048) game->won = true;
        }
    }
    if (game->score > game->best) game->best = game->score;
}

void game_2048_reset(game_2048_state_t *game)
{
    const uint32_t best = game->best;
    memset(game, 0, sizeof(*game));
    game->best = best;
}

bool game_2048_spawn(game_2048_state_t *game, uint32_t position_random, uint32_t value_random)
{
    unsigned empty[16];
    unsigned count = 0;
    for (unsigned i = 0; i < 16; ++i) {
        if (game->cells[i / 4][i % 4] == 0) empty[count++] = i;
    }
    if (count == 0) {
        update_status(game);
        return false;
    }
    const unsigned pos = empty[position_random % count];
    game->cells[pos / 4][pos % 4] = value_random % 10 == 9 ? 4 : 2;
    update_status(game);
    return true;
}

static uint32_t *line_cell(game_2048_state_t *game, gg_2048_move_direction direction, int line, int index)
{
    switch (direction) {
    case GG_2048_MOVE_LEFT:  return &game->cells[line][index];
    case GG_2048_MOVE_RIGHT: return &game->cells[line][3 - index];
    case GG_2048_MOVE_UP:    return &game->cells[index][line];
    case GG_2048_MOVE_DOWN:  return &game->cells[3 - index][line];
    default: return NULL;
    }
}

bool game_2048_slide(game_2048_state_t *game, gg_2048_move_direction direction)
{
    if (direction < GG_2048_MOVE_LEFT || direction > GG_2048_MOVE_DOWN || game->won || game->over) {
        return false;
    }
    bool changed = false;
    for (int line = 0; line < 4; ++line) {
        uint32_t packed[4] = {0};
        uint32_t result[4] = {0};
        int count = 0;
        int out = 0;
        for (int i = 0; i < 4; ++i) {
            const uint32_t n = *line_cell(game, direction, line, i);
            if (n) packed[count++] = n;
        }
        for (int i = 0; i < count; ++i) {
            uint32_t value = packed[i];
            if (i + 1 < count && packed[i] == packed[i + 1]) {
                value *= 2;
                game->score += value;
                ++i; // A newly merged tile cannot merge again during this move.
            }
            result[out++] = value;
        }
        for (int i = 0; i < 4; ++i) {
            uint32_t *cell = line_cell(game, direction, line, i);
            if (*cell != result[i]) changed = true;
            *cell = result[i];
        }
    }
    if (changed) ++game->moves;
    update_status(game);
    return changed;
}
