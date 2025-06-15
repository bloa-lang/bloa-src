CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -Wno-c11-extensions

BLOA = bloa
SOURCES = main.c lexer.c parser.c compiler.c vm.c gc.c chunk.c

all: $(BLOA)

$(BLOA): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BLOA)

.PHONY: all clean
