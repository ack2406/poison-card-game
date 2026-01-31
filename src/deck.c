#include "poison.h"

#include <stdlib.h>
#include <stdio.h>

void deck_create(Card *deck)
{
    uint8_t idx = 0;

    Color colors[] = {COLOR_BLUE, COLOR_RED, COLOR_PURPLE};
    uint8_t values[] = {1, 2, 5, 7};

    for (uint8_t i = 0; i < ARRAY_SIZE(colors); i++)
    {
        for (uint8_t j = 0; j < ARRAY_SIZE(values); j++)
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

    for (uint8_t i = 0; i < 8; i++)
    {
        deck[idx].type = CARD_TYPE_POISON;
        deck[idx].color = COLOR_NONE;
        deck[idx].value = 4;
        idx++;
    }
}

void deck_shuffle(Card *deck, uint8_t size)
{
    for (uint8_t i = size - 1; i > 0; i--)
    {
        uint8_t j = rand() % (i + 1);

        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

void deck_deal(GameState *state)
{
    uint8_t card_idx = 0;
    uint8_t player_idx = (state->dealer + 1) % state->num_players;

    while (card_idx < TOTAL_CARDS)
    {
        Player *player = &state->players[player_idx];
        player->hand[player->hand_size] = state->deck[card_idx];
        player->hand_size++;

        card_idx++;
        player_idx = (player_idx + 1) % state->num_players;
    }
}

void deck_card_to_string(const Card *card, char *buffer, size_t size)
{
    if (card->type == CARD_TYPE_POISON)
    {
        snprintf(buffer, size, "X%d", card->value);
    }
    else
    {
        const char *symbol;
        switch (card->color)
        {
        case COLOR_BLUE:
            symbol = "B";
            break;
        case COLOR_RED:
            symbol = "R";
            break;
        case COLOR_PURPLE:
            symbol = "P";
            break;
        default:
            symbol = "_";
            break;
        }
        snprintf(buffer, size, "%s%d", symbol, card->value);
    }
}
