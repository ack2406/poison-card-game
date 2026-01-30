CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude

SRC = $(wildcard src/*.c)

all: demo

demo:
	$(CC) $(CFLAGS) $(SRC) examples/cli_game.c -o demo

lib:
	$(CC) $(CFLAGS) -fPIC -shared $(SRC) -o libpoison.so

compile_commands:
	bear -- make clean all

clean:
	rm -f demo libpoison.so compile_commands.json

.PHONY: all demo lib compile_commands clean

