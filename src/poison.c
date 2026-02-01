#include "poison.h"

#include <stdlib.h>
#include <string.h>

#define TOTAL_CARDS 50
#define NUM_POISON_CARDS 8
#define NUM_COLORS 3
#define NUM_POTION_VALUES 5
#define NUM_CARD_TYPES (NUM_COLORS * NUM_POTION_VALUES + 1)
#define HAND_SIZE_DRAW 5

static const uint8_t POTION_VALUES[NUM_POTION_VALUES] = {1, 2, 4, 5, 7};

typedef struct
{
    Card cards[TOTAL_CARDS];
    uint8_t num_cards;
    uint8_t total_value;
    Color color;
} Cauldron;

typedef struct
{
    Card hand[TOTAL_CARDS];
    uint8_t hand_size;
    Card collected[TOTAL_CARDS];
    uint8_t collected_size;
    int32_t score;
} Player;

struct GameState
{
    Player players[NUM_PLAYERS_MAX];
    Cauldron cauldrons[NUM_CAULDRONS];
    uint8_t num_players;
    uint8_t current_player;
    uint8_t dealer;
    uint8_t round;
    bool game_over;
    bool round_scored;
    GameVariant variant;
    Card deck[TOTAL_CARDS];
    uint8_t deck_size;
    uint8_t deck_pos;
    uint32_t rng_state;
};

typedef struct
{
    uint8_t card_index;
    uint8_t cauldron_index;
} Action;

static uint16_t action_space_size(void)
{
    return (uint16_t)(TOTAL_CARDS * NUM_CAULDRONS);
}

static size_t observation_size(void)
{
    return 6 + NUM_PLAYERS_MAX * (3 + 2 * NUM_CARD_TYPES) +
           NUM_CAULDRONS * (3 + NUM_CARD_TYPES);
}

static int8_t game_potion_value_index(uint8_t value)
{
    for (uint8_t i = 0; i < NUM_POTION_VALUES; i++)
    {
        if (POTION_VALUES[i] == value)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

static uint8_t game_color_index(Color color)
{
    switch (color)
    {
    case COLOR_RED:
        return 0;
    case COLOR_BLUE:
        return 1;
    case COLOR_PURPLE:
        return 2;
    default:
        return 0;
    }
}

static uint8_t game_card_type_index(const Card *card)
{
    if (card->type == CARD_TYPE_POISON)
        return (uint8_t)(NUM_CARD_TYPES - 1);

    int8_t value_idx = game_potion_value_index(card->value);
    if (value_idx < 0)
        return 0;

    return (uint8_t)(game_color_index(card->color) * NUM_POTION_VALUES + value_idx);
}

static void game_count_cards(const Card *cards, uint8_t count, uint8_t *out_counts)
{
    memset(out_counts, 0, NUM_CARD_TYPES * sizeof(uint8_t));
    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t idx = game_card_type_index(&cards[i]);
        if (idx < NUM_CARD_TYPES)
        {
            out_counts[idx]++;
        }
    }
}

static uint32_t rng_next(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0)
        x = 0x6D2B79F5u;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void deck_create(Card *deck)
{
    uint8_t idx = 0;

    const Color colors[] = {COLOR_BLUE, COLOR_RED, COLOR_PURPLE};
    const uint8_t values[] = {1, 2, 5, 7};

    for (uint8_t i = 0; i < (uint8_t)(sizeof(colors) / sizeof(colors[0])); i++)
    {
        for (uint8_t j = 0; j < (uint8_t)(sizeof(values) / sizeof(values[0])); j++)
        {
            for (uint8_t k = 0; k < 3; k++)
            {
                deck[idx].type = CARD_TYPE_POTION;
                deck[idx].color = colors[i];
                deck[idx].value = values[j];
                idx++;
            }
        }

        for (uint8_t k = 0; k < 2; k++)
        {
            deck[idx].type = CARD_TYPE_POTION;
            deck[idx].color = colors[i];
            deck[idx].value = 4;
            idx++;
        }
    }

    for (uint8_t i = 0; i < NUM_POISON_CARDS; i++)
    {
        deck[idx].type = CARD_TYPE_POISON;
        deck[idx].color = COLOR_NONE;
        deck[idx].value = 4;
        idx++;
    }
}

static void deck_shuffle(Card *deck, uint8_t size, uint32_t *rng_state)
{
    if (size < 2)
        return;
    if (!rng_state)
        return;

    for (uint8_t i = size - 1; i > 0; i--)
    {
        uint8_t j = (uint8_t)(rng_next(rng_state) % (i + 1));

        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

static void deck_deal(GameState *state)
{
    if (!state)
        return;

    uint8_t player_idx = (state->dealer + 1) % state->num_players;
    while (state->deck_pos < state->deck_size)
    {
        Player *player = &state->players[player_idx];
        player->hand[player->hand_size++] = state->deck[state->deck_pos++];
        player_idx = (player_idx + 1) % state->num_players;
    }
}

static uint8_t game_max_rounds(const GameState *state)
{
    if (state->variant == GAME_VARIANT_DRAW)
        return 1;
    if (state->num_players == 3)
        return (uint8_t)(state->num_players * 2);
    return state->num_players;
}

static void game_prepare_deck(GameState *state)
{
    deck_create(state->deck);
    deck_shuffle(state->deck, TOTAL_CARDS, &state->rng_state);

    state->deck_size = TOTAL_CARDS;
    state->deck_pos = 0;

    if (state->num_players != 3)
        return;

    uint8_t write_idx = 0;
    for (uint8_t read_idx = 0; read_idx < TOTAL_CARDS; read_idx++)
    {
        if ((read_idx % 4) == 3)
            continue;
        state->deck[write_idx++] = state->deck[read_idx];
    }

    state->deck_size = write_idx;
}

static void game_deal_draw_variant(GameState *state)
{
    uint8_t player_idx = (state->dealer + 1) % state->num_players;
    uint8_t cards_to_deal = (uint8_t)(HAND_SIZE_DRAW * state->num_players);

    for (uint8_t i = 0; i < cards_to_deal && state->deck_pos < state->deck_size; i++)
    {
        Player *player = &state->players[player_idx];
        player->hand[player->hand_size++] = state->deck[state->deck_pos++];
        player_idx = (player_idx + 1) % state->num_players;
    }
}

GameState *game_init(uint8_t num_players, GameVariant variant, uint32_t seed)
{
    if (num_players < NUM_PLAYERS_MIN || num_players > NUM_PLAYERS_MAX)
        return NULL;

    GameState *state = malloc(sizeof(*state));
    if (!state)
        return NULL;

    memset(state, 0, sizeof(*state));
    state->num_players = num_players;
    state->variant = variant;
    state->rng_state = seed ? seed : 0x9E3779B9u;
    game_reset(state);

    return state;
}

void game_destroy(GameState *state)
{
    if (state)
    {
        free(state);
    }
}

void game_reset(GameState *state)
{
    if (!state)
        return;

    for (uint8_t i = 0; i < state->num_players; i++)
    {
        state->players[i].hand_size = 0;
        state->players[i].collected_size = 0;
        state->players[i].score = 0;
    }

    for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
    {
        state->cauldrons[i].num_cards = 0;
        state->cauldrons[i].total_value = 0;
        state->cauldrons[i].color = COLOR_NONE;
    }

    state->dealer = 0;
    state->round = 1;
    state->game_over = false;
    state->round_scored = false;

    game_prepare_deck(state);

    if (state->variant == GAME_VARIANT_DRAW)
    {
        game_deal_draw_variant(state);
    }
    else
    {
        deck_deal(state);
    }

    state->current_player = (state->dealer + 1) % state->num_players;
}

static bool game_is_action_legal(const GameState *state, const Action *action)
{
    if (!state || !action)
        return false;

    const Player *player = &state->players[state->current_player];

    if (action->card_index >= player->hand_size)
        return false;
    if (action->cauldron_index >= NUM_CAULDRONS)
        return false;

    const Card *card = &player->hand[action->card_index];
    const Cauldron *cauldron = &state->cauldrons[action->cauldron_index];

    if (card->type == CARD_TYPE_POISON)
        return true;

    if (cauldron->color == COLOR_NONE)
    {
        for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
        {
            if (state->cauldrons[i].color == card->color)
            {
                return false;
            }
        }
        return true;
    }

    return cauldron->color == card->color;
}

static float game_step(GameState *state, const Action *action)
{
    if (!game_is_action_legal(state, action))
        return 0.0f;

    Player *player = &state->players[state->current_player];
    Card card = player->hand[action->card_index];

    for (uint8_t i = action->card_index; i < player->hand_size - 1; i++)
    {
        player->hand[i] = player->hand[i + 1];
    }
    player->hand_size--;

    Cauldron *cauldron = &state->cauldrons[action->cauldron_index];

    cauldron->cards[cauldron->num_cards++] = card;
    cauldron->total_value += card.value;

    if (cauldron->color == COLOR_NONE && card.type == CARD_TYPE_POTION)
    {
        cauldron->color = card.color;
    }

    float reward = 0.0f;

    if (cauldron->total_value > CAULDRON_THRESHOLD)
    {
        uint8_t cards_to_collect = cauldron->num_cards - 1;

        for (uint8_t i = 0; i < cards_to_collect; i++)
        {
            player->collected[player->collected_size++] = cauldron->cards[i];
        }

        reward = -(float)cards_to_collect;

        cauldron->cards[0] = card;
        cauldron->num_cards = 1;
        cauldron->total_value = card.value;

        if (card.type == CARD_TYPE_POTION)
        {
            cauldron->color = card.color;
        }
        else
        {
            cauldron->color = COLOR_NONE;
        }
    }

    if (state->variant == GAME_VARIANT_DRAW && state->deck_pos < state->deck_size)
    {
        if (player->hand_size < TOTAL_CARDS)
        {
            player->hand[player->hand_size++] = state->deck[state->deck_pos++];
        }
    }

    state->current_player = (state->current_player + 1) % state->num_players;

    return reward;
}

static void game_advance_turn(GameState *state)
{
    if (!state)
        return;

    state->current_player = (state->current_player + 1) % state->num_players;
}

static bool game_is_round_over(const GameState *state)
{
    for (uint8_t i = 0; i < state->num_players; i++)
    {
        if (state->players[i].hand_size > 0)
        {
            return false;
        }
    }
    return true;
}

static Action game_decode_action(uint16_t action_id)
{
    Action action;
    if (action_id >= action_space_size())
    {
        action.card_index = UINT8_MAX;
        action.cauldron_index = UINT8_MAX;
        return action;
    }

    action.card_index = (uint8_t)(action_id / NUM_CAULDRONS);
    action.cauldron_index = (uint8_t)(action_id % NUM_CAULDRONS);
    return action;
}

StepResult game_step_action(GameState *state, uint16_t action_id)
{
    StepResult result = {0};
    result.winner = -1;

    if (!state)
        return result;
    if (state->game_over)
    {
        result.done = true;
        result.winner = game_get_winner(state);
        return result;
    }

    const Player *player = &state->players[state->current_player];
    if (player->hand_size == 0)
    {
        game_advance_turn(state);
        result.action_legal = false;
    }
    else
    {
        Action action = game_decode_action(action_id);
        result.action_legal = game_is_action_legal(state, &action);
        if (result.action_legal)
        {
            result.reward = game_step(state, &action);
        }
    }

    result.round_done = game_is_round_over(state);
    if (result.round_done)
    {
        game_apply_round_scores(state, NULL);
    }

    if (result.round_done && state->round >= game_max_rounds(state))
    {
        state->game_over = true;
    }

    result.done = state->game_over;
    if (result.done)
    {
        result.winner = game_get_winner(state);
    }

    return result;
}

void game_start_new_round(GameState *state)
{
    if (!state)
        return;

    if (state->round >= game_max_rounds(state))
    {
        state->game_over = true;
        return;
    }

    for (uint8_t i = 0; i < state->num_players; i++)
    {
        state->players[i].collected_size = 0;
        state->players[i].hand_size = 0;
    }

    for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
    {
        state->cauldrons[i].num_cards = 0;
        state->cauldrons[i].total_value = 0;
        state->cauldrons[i].color = COLOR_NONE;
    }

    state->dealer = (state->dealer + 1) % state->num_players;
    state->round++;
    state->game_over = false;
    state->round_scored = false;

    game_prepare_deck(state);

    if (state->variant == GAME_VARIANT_DRAW)
    {
        game_deal_draw_variant(state);
    }
    else
    {
        deck_deal(state);
    }

    state->current_player = (state->dealer + 1) % state->num_players;
}

static void game_calculate_round_scores(const GameState *state, int32_t *scores)
{
    if (!state || !scores)
        return;

    int color_counts[NUM_PLAYERS_MAX][NUM_COLORS + 1] = {0};
    bool immune[NUM_PLAYERS_MAX][NUM_COLORS + 1] = {false};

    for (uint8_t p = 0; p < state->num_players; p++)
    {
        const Player *player = &state->players[p];
        for (uint8_t i = 0; i < player->collected_size; i++)
        {
            const Card card = player->collected[i];
            if (card.type == CARD_TYPE_POTION)
            {
                color_counts[p][card.color]++;
            }
        }
    }

    for (Color c = COLOR_RED; c <= COLOR_PURPLE; c++)
    {
        int max_count = 0;
        int max_players = 0;
        uint8_t best_player = 0;

        for (uint8_t p = 0; p < state->num_players; p++)
        {
            int count = color_counts[p][c];
            if (count > max_count)
            {
                max_count = count;
                best_player = p;
                max_players = 1;
            }
            else if (count == max_count && count > 0)
            {
                max_players++;
            }
        }

        if (max_count > 0 && max_players == 1)
        {
            immune[best_player][c] = true;
        }
    }

    for (uint8_t p = 0; p < state->num_players; p++)
    {
        int32_t score = 0;
        const Player *player = &state->players[p];

        for (uint8_t i = 0; i < player->collected_size; i++)
        {
            const Card card = player->collected[i];
            if (card.type == CARD_TYPE_POISON)
            {
                score -= 2;
            }
            else if (!immune[p][card.color])
            {
                score -= 1;
            }
        }

        scores[p] = score;
    }
}

void game_apply_round_scores(GameState *state, int32_t *scores)
{
    if (!state)
        return;

    int32_t local_scores[NUM_PLAYERS_MAX] = {0};
    int32_t *target = scores ? scores : local_scores;

    game_calculate_round_scores(state, target);
    if (!state->round_scored)
    {
        for (uint8_t i = 0; i < state->num_players; i++)
        {
            state->players[i].score += target[i];
        }
        state->round_scored = true;
    }
}

int8_t game_get_winner(const GameState *state)
{
    if (!state || !state->game_over)
    {
        return -1;
    }

    int8_t winner = 0;
    int32_t best_score = state->players[0].score;
    bool tie = false;

    for (uint8_t i = 1; i < state->num_players; i++)
    {
        if (state->players[i].score > best_score)
        {
            best_score = state->players[i].score;
            winner = i;
            tie = false;
        }
        else if (state->players[i].score == best_score)
        {
            tie = true;
        }
    }

    return tie ? -1 : winner;
}

bool game_is_game_over(const GameState *state)
{
    return state ? state->game_over : true;
}

uint8_t game_get_num_players(const GameState *state)
{
    return state ? state->num_players : 0;
}

uint8_t game_get_current_player(const GameState *state)
{
    return state ? state->current_player : 0;
}

uint8_t game_get_dealer(const GameState *state)
{
    return state ? state->dealer : 0;
}

uint8_t game_get_round(const GameState *state)
{
    return state ? state->round : 0;
}

uint8_t game_get_player_hand_size(const GameState *state, uint8_t player)
{
    if (!state || player >= state->num_players)
        return 0;
    return state->players[player].hand_size;
}

uint8_t game_get_player_collected_size(const GameState *state, uint8_t player)
{
    if (!state || player >= state->num_players)
        return 0;
    return state->players[player].collected_size;
}

int32_t game_get_player_score(const GameState *state, uint8_t player)
{
    if (!state || player >= state->num_players)
        return 0;
    return state->players[player].score;
}

bool game_get_player_hand_card(const GameState *state, uint8_t player, uint8_t card_index, Card *out)
{
    if (!state || !out || player >= state->num_players)
        return false;
    const Player *p = &state->players[player];
    if (card_index >= p->hand_size)
        return false;
    *out = p->hand[card_index];
    return true;
}

bool game_get_player_collected_card(const GameState *state, uint8_t player, uint8_t card_index, Card *out)
{
    if (!state || !out || player >= state->num_players)
        return false;
    const Player *p = &state->players[player];
    if (card_index >= p->collected_size)
        return false;
    *out = p->collected[card_index];
    return true;
}

uint8_t game_get_cauldron_num_cards(const GameState *state, uint8_t cauldron)
{
    if (!state || cauldron >= NUM_CAULDRONS)
        return 0;
    return state->cauldrons[cauldron].num_cards;
}

uint8_t game_get_cauldron_total_value(const GameState *state, uint8_t cauldron)
{
    if (!state || cauldron >= NUM_CAULDRONS)
        return 0;
    return state->cauldrons[cauldron].total_value;
}

Color game_get_cauldron_color(const GameState *state, uint8_t cauldron)
{
    if (!state || cauldron >= NUM_CAULDRONS)
        return COLOR_NONE;
    return state->cauldrons[cauldron].color;
}

bool game_get_cauldron_card(const GameState *state, uint8_t cauldron, uint8_t card_index, Card *out)
{
    if (!state || !out || cauldron >= NUM_CAULDRONS)
        return false;
    const Cauldron *c = &state->cauldrons[cauldron];
    if (card_index >= c->num_cards)
        return false;
    *out = c->cards[card_index];
    return true;
}

size_t game_observation_size(void)
{
    return observation_size();
}

size_t game_get_observation(const GameState *state, uint8_t perspective_player, GameObsMode mode, float *out, size_t out_len)
{
    if (!state || !out)
        return 0;
    const size_t obs_size = observation_size();
    if (out_len < obs_size)
        return 0;

    size_t idx = 0;
    const size_t player_stride = 3 + 2 * NUM_CARD_TYPES;
    uint8_t base = (perspective_player < state->num_players) ? perspective_player : 0;
    uint8_t rel_current = (uint8_t)((state->current_player + state->num_players - base) % state->num_players);
    uint8_t rel_dealer = (uint8_t)((state->dealer + state->num_players - base) % state->num_players);

    out[idx++] = (float)state->num_players;
    out[idx++] = (float)rel_current;
    out[idx++] = (float)rel_dealer;
    out[idx++] = (float)state->round;
    out[idx++] = (float)state->variant;
    out[idx++] = (float)(state->deck_size - state->deck_pos);

    for (uint8_t slot = 0; slot < NUM_PLAYERS_MAX; slot++)
    {
        if (slot < state->num_players)
        {
            uint8_t player_idx = (uint8_t)((base + slot) % state->num_players);
            const Player *player = &state->players[player_idx];
            uint8_t counts[NUM_CARD_TYPES];

            out[idx++] = (float)player->hand_size;
            out[idx++] = (float)player->collected_size;
            out[idx++] = (float)player->score;

            if (mode == GAME_OBS_PARTIAL && player_idx != base)
            {
                for (uint8_t i = 0; i < NUM_CARD_TYPES; i++)
                {
                    out[idx++] = 0.0f;
                }
            }
            else
            {
                game_count_cards(player->hand, player->hand_size, counts);
                for (uint8_t i = 0; i < NUM_CARD_TYPES; i++)
                {
                    out[idx++] = (float)counts[i];
                }
            }

            if (mode == GAME_OBS_PARTIAL && !state->round_scored)
            {
                for (uint8_t i = 0; i < NUM_CARD_TYPES; i++)
                {
                    out[idx++] = 0.0f;
                }
            }
            else
            {
                game_count_cards(player->collected, player->collected_size, counts);
                for (uint8_t i = 0; i < NUM_CARD_TYPES; i++)
                {
                    out[idx++] = (float)counts[i];
                }
            }
        }
        else
        {
            for (uint8_t i = 0; i < player_stride; i++)
            {
                out[idx++] = 0.0f;
            }
        }
    }

    for (uint8_t c = 0; c < NUM_CAULDRONS; c++)
    {
        const Cauldron *cauldron = &state->cauldrons[c];
        uint8_t counts[NUM_CARD_TYPES];

        out[idx++] = (float)cauldron->total_value;
        out[idx++] = (float)cauldron->num_cards;
        out[idx++] = (float)cauldron->color;

        game_count_cards(cauldron->cards, cauldron->num_cards, counts);
        for (uint8_t i = 0; i < NUM_CARD_TYPES; i++)
        {
            out[idx++] = (float)counts[i];
        }
    }

    return idx;
}

uint16_t game_action_space_size(void)
{
    return action_space_size();
}

size_t game_get_legal_action_mask(const GameState *state, uint8_t *out_mask, size_t out_len)
{
    if (!state || !out_mask)
        return 0;
    const uint16_t action_space = action_space_size();
    if (out_len < action_space)
        return 0;

    size_t count = 0;
    for (uint16_t action_id = 0; action_id < action_space; action_id++)
    {
        Action action = game_decode_action(action_id);
        bool legal = game_is_action_legal(state, &action);
        out_mask[action_id] = legal ? 1 : 0;
        if (legal)
            count++;
    }
    return count;
}
