CC := gcc
CFLAGS := -g
GTKFLAGS := $(shell pkg-config --cflags --libs gtk+-3.0 gmodule-export-2.0)

all: messangerClnt server

messangerClnt: messangerClnt.c
	$(CC) $(CFLAGS) $< -o $@ $(GTKFLAGS)

server: server.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f messangerClnt server

