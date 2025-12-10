CC       = cc
CPPFLAGS = -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS   = -std=c99 -Wall -Wextra -O3 -march=native $(CPPFLAGS)
LDFLAGS  = -lX11 -lXtst -lXi -lpthread

PREFIX = /usr/local

SRC = xmouseleast.c
BIN = xmouseleast

all: $(BIN)

$(BIN): $(SRC) config.h
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

install: all
	mkdir -p $(PREFIX)/bin
	cp -f $(BIN) $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)
