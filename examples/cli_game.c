#include "poison.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#define CLEAR_SCREEN "cls"
#else
#define CLEAR_SCREEN "clear"
#endif

void print_separator(void)
{
    printf("\n");
    for (uint8_t i = 0; i < 60; i++)
        printf("=");
    printf("\n");
}

void print_card(const Card *card)
{
    char buffer[8];
    deck_card_to_string(card, buffer, sizeof(buffer));
    printf("%s", buffer);
}

void print_cauldrons(const GameState *state)
{
    const char *color_names[] = {"Empty", "Red", "Blue", "Purple"};

    printf("\nCauldrons:\n");
    for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
    {
        const Cauldron *c = &state->cauldrons[i];
        printf("  [%d] %-8s %2d/%-2d (%d cards): ",
               i, color_names[c->color], c->total_value,
               CAULDRON_THRESHOLD, c->num_cards);

        for (uint8_t j = 0; j < c->num_cards; j++)
        {
            print_card(&c->cards[j]);
            printf(" ");
        }
        printf("\n");
    }
}

void print_hand(const Player *player)
{
    printf("\nYour hand (%d cards):\n  ", player->hand_size);
    for (uint8_t i = 0; i < player->hand_size; i++)
    {
        print_card(&player->hand[i]);
        printf(" ");
    }
    printf("\n");
}

void print_scores(const GameState *state)
{
    printf("\nScores:\n");
    for (uint8_t i = 0; i < state->num_players; i++)
    {
        printf("  Player %d: %4d (collected: %2d)\n",
               i + 1, state->players[i].score,
               state->players[i].collected_size);
    }
}

void wait_for_enter(void)
{
    printf("\nPress ENTER to continue...");
    while (getchar() != '\n')
        ;
}

void clear_input_buffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

uint8_t get_player_choice(uint8_t max_value)
{
    int choice;
    while (1)
    {
        printf("Your choice (0-%d): ", max_value);
        if (scanf("%d", &choice) != 1)
        {
            clear_input_buffer();
            printf("Invalid input! Try again.\n");
            continue;
        }
        clear_input_buffer();

        if (choice >= 0 && choice <= max_value)
        {
            return (uint8_t)choice;
        }
        printf("Out of range! Try again.\n");
    }
}

void play_human_turn(GameState *state, uint8_t player_idx)
{
    Player *player = &state->players[player_idx];

    print_separator();
    printf("Player %d's Turn (Round %d)\n", player_idx + 1, state->round);
    print_separator();

    print_cauldrons(state);
    print_hand(player);

    Action actions[150];
    uint8_t num_actions = game_get_legal_actions(state, actions);

    printf("\nLegal actions:\n");
    for (uint8_t i = 0; i < num_actions; i++)
    {
        printf("  [%d] Play ", i);
        print_card(&player->hand[actions[i].card_index]);
        printf(" to Cauldron %d\n", actions[i].cauldron_index);
    }

    uint8_t choice = get_player_choice(num_actions - 1);
    float reward = game_step(state, &actions[choice]);

    printf("\n");
    if (reward < 0)
    {
        printf("OVERFLOW! You collected %d cards (%.0f points penalty)\n",
               (int)(-reward), reward);
    }
    else
    {
        printf("Card played successfully!\n");
    }

    wait_for_enter();
}

void play_ai_turn(GameState *state, uint8_t player_idx)
{
    Action actions[150];
    uint8_t num_actions = game_get_legal_actions(state, actions);

    if (num_actions == 0)
        return;

    Action chosen = actions[rand() % num_actions];
    Player *player = &state->players[player_idx];

    printf("\nPlayer %d (AI) plays ", player_idx + 1);
    print_card(&player->hand[chosen.card_index]);
    printf(" to Cauldron %d", chosen.cauldron_index);

    float reward = game_step(state, &chosen);

    if (reward < 0)
    {
        printf(" - OVERFLOW! Collected %d cards", (int)(-reward));
    }
    printf("\n");
}

void show_round_results(GameState *state)
{
    print_separator();
    printf("Round %d Complete!\n", state->round);
    print_separator();

    printf("\nRound scores:\n");
    for (uint8_t i = 0; i < state->num_players; i++)
    {
        int32_t round_score = game_calculate_player_score(&state->players[i]);
        state->players[i].score += round_score;

        printf("  Player %d: %+4d (Total: %4d)\n",
               i + 1, round_score, state->players[i].score);
    }

    wait_for_enter();
}

void show_final_results(const GameState *state)
{
    print_separator();
    printf("GAME OVER!\n");
    print_separator();

    int8_t winner = game_get_winner(state);

    printf("\nFinal Standings:\n");
    for (uint8_t i = 0; i < state->num_players; i++)
    {
        printf("  %s Player %d: %4d points\n",
               (i == winner) ? ">>>" : "   ",
               i + 1, state->players[i].score);
    }

    printf("\n");
    if (winner >= 0)
    {
        printf("Player %d WINS!\n", winner + 1);
    }
    print_separator();
}

int main(void)
{
    srand((unsigned int)time(NULL));

    printf("Poison Card Game\n");
    print_separator();

    printf("\nNumber of players (%d-%d): ", NUM_PLAYERS_MIN, NUM_PLAYERS_MAX);
    int num_players;
    scanf("%d", &num_players);
    clear_input_buffer();

    if (num_players < NUM_PLAYERS_MIN || num_players > NUM_PLAYERS_MAX)
    {
        printf("Invalid! Using %d players.\n", NUM_PLAYERS_MIN);
        num_players = NUM_PLAYERS_MIN;
    }

    int human_idx = 0;

    GameState *game = game_init((uint8_t)num_players);
    if (!game)
    {
        printf("Failed to initialize game!\n");
        return 1;
    }

    printf("\nGame starting!");
    wait_for_enter();

    while (!game->game_over)
    {
        while (!game_is_round_over(game))
        {
            if (game->current_player == human_idx)
            {
                play_human_turn(game, game->current_player);
            }
            else
            {
                play_ai_turn(game, game->current_player);
            }
        }

        show_round_results(game);

        if (game->round < game->num_players)
        {
            printf("\nStarting Round %d...\n", game->round + 1);
            game_start_new_round(game);
            wait_for_enter();
        }
        else
        {
            game->game_over = true;
        }
    }

    show_final_results(game);
    game_destroy(game);

    return 0;
}
