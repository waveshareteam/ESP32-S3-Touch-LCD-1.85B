#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { GAME_2048_SIDE = 4 };
typedef enum {
    GG_2048_MOVE_LEFT,
    GG_2048_MOVE_RIGHT,
    GG_2048_MOVE_UP,
    GG_2048_MOVE_DOWN,
} gg_2048_move_direction;

typedef struct {
    uint32_t cells[GAME_2048_SIDE][GAME_2048_SIDE];
    uint32_t score;
    uint32_t best;
    uint32_t moves;
    bool won;
    bool over;
} game_2048_state_t;

/* Reset preserves the session best score. Randomness is supplied by the caller. */
void game_2048_reset(game_2048_state_t *game);
bool game_2048_spawn(game_2048_state_t *game, uint32_t position_random, uint32_t value_random);
bool game_2048_slide(game_2048_state_t *game, gg_2048_move_direction direction);
bool game_2048_can_move(const game_2048_state_t *game);

#ifdef __cplusplus
}
#endif
