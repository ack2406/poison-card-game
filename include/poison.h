#ifndef POISON_H
#define POISON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define NUM_PLAYERS_MIN 3
#define NUM_PLAYERS_MAX 6

#define NUM_CAULDRONS 3
#define CAULDRON_THRESHOLD 13

#define NUM_POISON_CARDS 8
#define NUM_COLORS 3
#define CARDS_PER_COLOR 14

#define TOTAL_CARDS (NUM_POISON_CARDS + NUM_COLORS * CARDS_PER_COLOR)


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef enum {
    COLOR_NONE = 0,
    COLOR_RED = 1,
    COLOR_BLUE = 2,
    COLOR_PURPLE = 3
} Color;

typedef enum {
    CARD_TYPE_POTION = 0,
    CARD_TYPE_POISON = 1
} CardType;

typedef struct {
    CardType type;
    Color color;
    uint8_t value;
} Card;

typedef struct {
    Card cards[TOTAL_CARDS];
    uint8_t num_cards;
    uint8_t total_value;
    Color color;
} Cauldron;

typedef struct {
    Card hand[TOTAL_CARDS];
    uint8_t hand_size;
    Card collected[TOTAL_CARDS];
    uint8_t collected_size;
    int32_t score;
} Player;

typedef struct {
    Player players[NUM_PLAYERS_MAX];
    Cauldron cauldrons[NUM_CAULDRONS];
    uint8_t num_players;
    uint8_t current_player;
    uint8_t dealer;
    uint8_t round;
    bool game_over;
    Card deck[TOTAL_CARDS];
    uint8_t deck_size;
} GameState;

typedef struct {
    uint8_t card_index;
    uint8_t cauldron_index;
} Action;


GameState* game_init(uint8_t num_players);
void       game_destroy(GameState* state);
void       game_reset(GameState* state);
bool       game_is_action_legal(const GameState* state, const Action* action);
uint8_t    game_get_legal_actions(const GameState* state, Action* actions);
float      game_step(GameState* state, const Action* action);
bool       game_is_round_over(const GameState* state);
void       game_start_new_round(GameState* state);
int32_t    game_calculate_player_score(const Player* player);
int8_t     game_get_winner(const GameState* state);
void       game_print_state(const GameState* state);

void       deck_create(Card* deck);
void       deck_shuffle(Card*deck, uint8_t size);
void       deck_deal(GameState* state);
void       deck_card_to_string(const Card* card, char* buffer, size_t size);


#endif // POISON_H
