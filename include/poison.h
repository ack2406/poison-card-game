#ifndef POISON_H
#define POISON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define NUM_PLAYERS_MIN 3
#define NUM_PLAYERS_MAX 6
#define NUM_CAULDRONS 3
#define CAULDRON_THRESHOLD 13

typedef enum
{
    COLOR_NONE = 0,
    COLOR_RED = 1,
    COLOR_BLUE = 2,
    COLOR_PURPLE = 3
} Color;

typedef enum
{
    CARD_TYPE_POTION = 0,
    CARD_TYPE_POISON = 1
} CardType;

typedef enum
{
    GAME_VARIANT_CLASSIC = 0,
    GAME_VARIANT_DRAW = 1
} GameVariant;

typedef enum
{
    GAME_OBS_FULL = 0,
    GAME_OBS_PARTIAL = 1
} GameObsMode;

typedef struct
{
    CardType type;
    Color color;
    uint8_t value;
} Card;

typedef struct GameState GameState;

typedef struct
{
    float reward;
    bool done;
    bool round_done;
    bool action_legal;
    int8_t winner;
} StepResult;

// lifecycle
GameState *game_init(uint8_t num_players, GameVariant variant, uint32_t seed);
void game_destroy(GameState *state);
void game_reset(GameState *state);

// step
StepResult game_step_action(GameState *state, uint16_t action_id);

// rounds & scoring
void game_start_new_round(GameState *state);
void game_apply_round_scores(GameState *state, int32_t *scores);
int8_t game_get_winner(const GameState *state);

// game status
bool game_is_game_over(const GameState *state);
uint8_t game_get_num_players(const GameState *state);
uint8_t game_get_current_player(const GameState *state);
uint8_t game_get_dealer(const GameState *state);
uint8_t game_get_round(const GameState *state);

// player info
uint8_t game_get_player_hand_size(const GameState *state, uint8_t player);
uint8_t game_get_player_collected_size(const GameState *state, uint8_t player);
int32_t game_get_player_score(const GameState *state, uint8_t player);
bool game_get_player_hand_card(const GameState *state, uint8_t player, uint8_t card_index, Card *out);
bool game_get_player_collected_card(const GameState *state, uint8_t player, uint8_t card_index, Card *out);

// cauldrons
uint8_t game_get_cauldron_num_cards(const GameState *state, uint8_t cauldron);
uint8_t game_get_cauldron_total_value(const GameState *state, uint8_t cauldron);
Color game_get_cauldron_color(const GameState *state, uint8_t cauldron);
bool game_get_cauldron_card(const GameState *state, uint8_t cauldron, uint8_t card_index, Card *out);

// observation
size_t game_observation_size(void);
size_t game_get_observation(const GameState *state, uint8_t perspective_player, GameObsMode mode, float *out, size_t out_len);

// actions
uint16_t game_action_space_size(void);
size_t game_get_legal_action_mask(const GameState *state, uint8_t *out_mask, size_t out_len);

#endif // POISON_H
