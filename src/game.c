#include "poison.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GameState *game_init(uint8_t num_players)
{
    if (num_players < NUM_PLAYERS_MIN || num_players > NUM_PLAYERS_MAX)
        return NULL;

    GameState *state = malloc(sizeof(*state));
    if (!state)
        return NULL;

    memset(state, 0, sizeof(*state));

    state->num_players = num_players;
    state->dealer = 0;
    state->round = 1;
    state->game_over = false;

    deck_create(state->deck);
    deck_shuffle(state->deck, TOTAL_CARDS);
    deck_deal(state);

    state->current_player = (state->dealer + 1) % num_players;

    for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
    {
        state->cauldrons[i].num_cards = 0;
        state->cauldrons[i].total_value = 0;
        state->cauldrons[i].color = COLOR_NONE;
    }

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

    deck_create(state->deck);
    deck_shuffle(state->deck, TOTAL_CARDS);
    deck_deal(state);

    state->current_player = (state->dealer + 1) % state->num_players;
}

bool game_is_action_legal(const GameState *state, const Action *action)
{
    Player *player = (Player *)&state->players[state->current_player];

    if (action->card_index >= player->hand_size)
        return false;
    if (action->cauldron_index >= NUM_CAULDRONS)
        return false;

    Card *card = &player->hand[action->card_index];
    Cauldron *cauldron = (Cauldron *)&state->cauldrons[action->cauldron_index];

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

uint8_t game_get_legal_actions(const GameState *state, Action *actions)
{
    uint8_t count = 0;
    Player *player = (Player *)&state->players[state->current_player];

    for (uint8_t card_idx = 0; card_idx < player->hand_size; card_idx++)
    {
        for (uint8_t cauldron_idx = 0; cauldron_idx < NUM_CAULDRONS; cauldron_idx++)
        {
            Action action = {card_idx, cauldron_idx};
            if (game_is_action_legal(state, &action))
            {
                actions[count++] = action;
            }
        }
    }

    return count;
}

float game_step(GameState *state, const Action *action)
{
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

    state->current_player = (state->current_player + 1) % state->num_players;

    return reward;
}

bool game_is_round_over(const GameState *state)
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

void game_start_new_round(GameState *state)
{
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

    deck_create(state->deck);
    deck_shuffle(state->deck, TOTAL_CARDS);
    deck_deal(state);

    state->current_player = (state->dealer + 1) % state->num_players;

    if (state->round > state->num_players)
    {
        state->game_over = true;
    }
}

int32_t game_calculate_player_score(const Player *player)
{
    int color_counts[NUM_COLORS + 1] = {0};

    for (uint8_t i = 0; i < player->collected_size; i++)
    {
        Card card = player->collected[i];
        if (card.type == CARD_TYPE_POTION)
        {
            color_counts[card.color]++;
        }
    }

    int max_count = 0;
    Color immune_color = COLOR_NONE;
    bool tie = false;

    for (Color c = COLOR_RED; c <= COLOR_PURPLE; c++)
    {
        if (color_counts[c] > max_count)
        {
            max_count = color_counts[c];
            immune_color = c;
            tie = false;
        }
        else if (color_counts[c] == max_count && max_count > 0)
        {
            tie = true;
        }
    }

    if (tie)
    {
        immune_color = COLOR_NONE;
    }

    int32_t score = 0;

    for (uint8_t i = 0; i < player->collected_size; i++)
    {
        Card card = player->collected[i];

        if (card.type == CARD_TYPE_POISON)
        {
            score -= 2;
        }
        else if (card.color != immune_color)
        {
            score -= 1;
        }
    }

    return score;
}

int8_t game_get_winner(const GameState *state)
{
    if (!state->game_over)
    {
        return -1;
    }

    int8_t winner = 0;
    uint32_t best_score = state->players[0].score;

    for (uint8_t i = 1; i < state->num_players; i++)
    {
        if (state->players[i].score > best_score)
        {
            best_score = state->players[i].score;
            winner = i;
        }
    }

    return winner;
}

void game_print_state(const GameState *state)
{
    printf("\n=== Round %d ===\n", state->round);
    printf("Dealer: Player %d\n", state->dealer + 1);
    printf("Current: Player %d\n", state->current_player + 1);

    printf("\nCauldrons:\n");
    for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
    {
        printf("  [%d] Value: %d, Cards: %d, Color: %d\n",
               i, state->cauldrons[i].total_value,
               state->cauldrons[i].num_cards,
               state->cauldrons[i].color);
    }

    printf("\nPlayers:\n");
    for (uint8_t i = 0; i < state->num_players; i++)
    {
        printf("  Player %d: Hand=%d, Collected=%d, Score=%d\n",
               i + 1,
               state->players[i].hand_size,
               state->players[i].collected_size,
               state->players[i].score);
    }
}
