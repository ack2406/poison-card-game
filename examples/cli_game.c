#include "poison.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void print_deck(const Card* deck) {
    char buffer[32];
    for (size_t i = 0; i < TOTAL_CARDS; i++) {
        deck_card_to_string(&deck[i], buffer, sizeof(buffer));
        printf("%s ", buffer);
    }
    printf("\n");
}

void print_hand(const Card* hand, uint8_t hand_size) {
    char buffer[32];
    for (int i = 0; i < hand_size; i++) {
        deck_card_to_string(&hand[i], buffer, sizeof(buffer));
        printf("%s ", buffer);
    }
    printf("\n");
}

int main(void) {
    srand(time(NULL));
    
    Card deck[TOTAL_CARDS];
    deck_create(deck);    
    printf("Original deck:\n");
    print_deck(deck);

    printf("\n--------\n");

    deck_shuffle(deck, TOTAL_CARDS);
    printf("Shuffled deck:\n");
    print_deck(deck);

    printf("\n--------\n");

    GameState* game = malloc(sizeof(GameState));
    memset(game, 0, sizeof(GameState));
    
    game->num_players = 4;
    game->dealer = 0;
    
    memcpy(game->deck, deck, sizeof(Card) * TOTAL_CARDS);
    deck_deal(game);

    printf("Dealt hands:\n");
    for (int i = 0; i < game->num_players; i++) {
        printf("Player %d (%d cards): ", (i + 1), game->players[i].hand_size);
        print_hand(game->players[i].hand, game->players[i].hand_size);
    }
    
    free(game);
    return 0;
}

