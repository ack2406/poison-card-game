#include "poison.h"

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


