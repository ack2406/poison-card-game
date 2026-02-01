CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -Wall -Wextra -Werror -std=c11 -O2 -g
LDFLAGS ?=

SRC = src/poison.c

all: demo

demo: $(SRC) examples/cli_game.c include/poison.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SRC) examples/cli_game.c -o demo

lib: $(SRC) include/poison.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -fPIC -shared $(SRC) -o libpoison.so

compile_commands:
	bear -- make clean all

clean:
	rm -f demo libpoison.so compile_commands.json

.PHONY: all demo lib compile_commands clean
