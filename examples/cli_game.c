#include "poison.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void clear_input_buffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

static int read_int_or_default(int fallback)
{
    int value;
    if (scanf("%d", &value) != 1)
    {
        clear_input_buffer();
        return fallback;
    }
    clear_input_buffer();
    return value;
}

static uint16_t read_choice(uint16_t max_value)
{
    int choice;
    while (1)
    {
        printf("Your choice (0-%u): ", max_value);
        if (scanf("%d", &choice) != 1)
        {
            clear_input_buffer();
            printf("Invalid input.\n");
            continue;
        }
        clear_input_buffer();
        if (choice >= 0 && choice <= max_value)
            return (uint16_t)choice;
        printf("Out of range.\n");
    }
}

static void print_card(const Card *card)
{
    if (card->type == CARD_TYPE_POISON)
    {
        printf("X%d", card->value);
        return;
    }

    const char *symbol = "_";
    switch (card->color)
    {
    case COLOR_RED:
        symbol = "R";
        break;
    case COLOR_BLUE:
        symbol = "B";
        break;
    case COLOR_PURPLE:
        symbol = "P";
        break;
    default:
        break;
    }

    printf("%s%d", symbol, card->value);
}

static void print_cauldrons(const GameState *state)
{
    static const char *color_names[] = {"Empty", "Red", "Blue", "Purple"};

    printf("\nCauldrons:\n");
    for (uint8_t i = 0; i < NUM_CAULDRONS; i++)
    {
        uint8_t total_value = game_get_cauldron_total_value(state, i);
        uint8_t num_cards = game_get_cauldron_num_cards(state, i);
        Color color = game_get_cauldron_color(state, i);

        printf("  [%u] %-6s %2u/%-2u (%u cards): ",
               i, color_names[color], total_value,
               CAULDRON_THRESHOLD, num_cards);

        for (uint8_t j = 0; j < num_cards; j++)
        {
            Card card;
            if (!game_get_cauldron_card(state, i, j, &card))
                break;
            print_card(&card);
            printf(" ");
        }
        printf("\n");
    }
}

static void print_hand(const GameState *state, uint8_t player)
{
    uint8_t hand_size = game_get_player_hand_size(state, player);
    printf("\nYour hand (%u cards):\n  ", hand_size);
    for (uint8_t i = 0; i < hand_size; i++)
    {
        Card card;
        if (!game_get_player_hand_card(state, player, i, &card))
            break;
        print_card(&card);
        printf(" ");
    }
    printf("\n");
}

static void print_scores(const GameState *state)
{
    uint8_t num_players = game_get_num_players(state);
    printf("\nScores:\n");
    for (uint8_t i = 0; i < num_players; i++)
    {
        printf("  Player %u: %d (collected %u)\n",
               (unsigned)(i + 1),
               (int)game_get_player_score(state, i),
               (unsigned)game_get_player_collected_size(state, i));
    }
}

static StepResult play_human_turn(GameState *state, uint8_t player)
{
    StepResult result = {0};
    uint16_t action_space = game_action_space_size();
    uint8_t *mask = calloc(action_space, sizeof(*mask));
    uint16_t *legal_ids = malloc(action_space * sizeof(*legal_ids));
    uint16_t legal_count = 0;

    if (!mask || !legal_ids)
    {
        free(mask);
        free(legal_ids);
        printf("Out of memory. Skipping turn.\n");
        return game_step_action(state, 0);
    }

    printf("\nPlayer %u (Round %u)\n", (unsigned)(player + 1),
           (unsigned)game_get_round(state));

    print_cauldrons(state);
    print_hand(state, player);

    game_get_legal_action_mask(state, mask, action_space);
    for (uint16_t action_id = 0; action_id < action_space; action_id++)
    {
        if (mask[action_id])
            legal_ids[legal_count++] = action_id;
    }

    if (legal_count == 0)
    {
        printf("No legal actions. Skipping turn.\n");
        free(mask);
        free(legal_ids);
        return game_step_action(state, 0);
    }

    printf("\nLegal actions:\n");
    for (uint16_t i = 0; i < legal_count; i++)
    {
        uint8_t card_index = (uint8_t)(legal_ids[i] / NUM_CAULDRONS);
        uint8_t cauldron_index = (uint8_t)(legal_ids[i] % NUM_CAULDRONS);
        Card card;
        if (!game_get_player_hand_card(state, player, card_index, &card))
            continue;

        printf("  [%u] Play ", (unsigned)i);
        print_card(&card);
        printf(" to cauldron %u\n", (unsigned)cauldron_index);
    }

    uint16_t choice = read_choice((uint16_t)(legal_count - 1));
    result = game_step_action(state, legal_ids[choice]);

    if (result.reward < 0)
    {
        printf("Overflow! You collected %d cards.\n", (int)(-result.reward));
    }
    else
    {
        printf("Card played.\n");
    }

    free(mask);
    free(legal_ids);
    return result;
}

static StepResult play_ai_turn(GameState *state, uint8_t player)
{
    StepResult result = {0};
    uint16_t action_space = game_action_space_size();
    uint8_t *mask = calloc(action_space, sizeof(*mask));
    uint16_t *legal_ids = malloc(action_space * sizeof(*legal_ids));
    uint16_t legal_count = 0;

    if (!mask || !legal_ids)
    {
        free(mask);
        free(legal_ids);
        printf("Player %u skipped (out of memory).\n", (unsigned)(player + 1));
        return game_step_action(state, 0);
    }

    game_get_legal_action_mask(state, mask, action_space);
    for (uint16_t action_id = 0; action_id < action_space; action_id++)
    {
        if (mask[action_id])
            legal_ids[legal_count++] = action_id;
    }

    if (legal_count == 0)
    {
        printf("Player %u skips.\n", (unsigned)(player + 1));
        free(mask);
        free(legal_ids);
        return game_step_action(state, 0);
    }

    uint16_t chosen = legal_ids[rand() % legal_count];
    uint8_t card_index = (uint8_t)(chosen / NUM_CAULDRONS);
    uint8_t cauldron_index = (uint8_t)(chosen % NUM_CAULDRONS);
    Card card;

    printf("Player %u plays ", (unsigned)(player + 1));
    if (game_get_player_hand_card(state, player, card_index, &card))
        print_card(&card);
    else
        printf("?");
    printf(" to cauldron %u\n", (unsigned)cauldron_index);

    result = game_step_action(state, chosen);

    free(mask);
    free(legal_ids);
    return result;
}

static void show_round_results(GameState *state)
{
    int32_t round_scores[NUM_PLAYERS_MAX] = {0};
    uint8_t num_players = game_get_num_players(state);

    printf("\nRound results:\n");
    game_apply_round_scores(state, round_scores);
    for (uint8_t i = 0; i < num_players; i++)
    {
        printf("  Player %u: %+d (total %d)\n",
               (unsigned)(i + 1),
               (int)round_scores[i],
               (int)game_get_player_score(state, i));
    }
}

static void show_final_results(GameState *state)
{
    uint8_t num_players = game_get_num_players(state);
    int8_t winner = game_get_winner(state);

    printf("\nFinal scores:\n");
    for (uint8_t i = 0; i < num_players; i++)
    {
        printf("  Player %u: %d\n", (unsigned)(i + 1),
               (int)game_get_player_score(state, i));
    }

    if (winner >= 0)
        printf("Winner: Player %d\n", (int)(winner + 1));
    else
        printf("Tie game.\n");
}

int main(void)
{
    srand((unsigned int)time(NULL));

    printf("Poison Card Game\n");
    printf("Players (%d-%d): ", NUM_PLAYERS_MIN, NUM_PLAYERS_MAX);
    int num_players = read_int_or_default(NUM_PLAYERS_MIN);
    if (num_players < NUM_PLAYERS_MIN || num_players > NUM_PLAYERS_MAX)
    {
        printf("Invalid, using %d players.\n", NUM_PLAYERS_MIN);
        num_players = NUM_PLAYERS_MIN;
    }

    printf("Variant (0=classic, 1=draw): ");
    int variant_choice = read_int_or_default(0);
    GameVariant variant = variant_choice ? GAME_VARIANT_DRAW : GAME_VARIANT_CLASSIC;

    GameState *game = game_init((uint8_t)num_players, variant, (uint32_t)time(NULL));
    if (!game)
    {
        printf("Failed to init game.\n");
        return 1;
    }

    uint8_t human_player = 0;

    while (!game_is_game_over(game))
    {
        StepResult step = {0};
        step.round_done = false;

        while (!step.round_done)
        {
            uint8_t current = game_get_current_player(game);
            if (current == human_player)
                step = play_human_turn(game, current);
            else
                step = play_ai_turn(game, current);
        }

        show_round_results(game);
        print_scores(game);

        if (!step.done)
        {
            game_start_new_round(game);
            if (!game_is_game_over(game))
                printf("\nStarting round %u...\n", (unsigned)game_get_round(game));
        }
    }

    show_final_results(game);
    game_destroy(game);
    return 0;
}
