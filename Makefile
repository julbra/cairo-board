
CC = gcc
LEX = flex
SED = sed
CFLAGS = -Wall -O2 `pkg-config --cflags gtk+-2.0 cairo librsvg-2.0 gthread-2.0`
#CFLAGS = -Wall `pkg-config --cflags gtk+-2.0 cairo librsvg-2.0 gthread-2.0` -ggdb
LDFLAGS = -lpthread `pkg-config --libs gtk+-2.0 cairo librsvg-2.0 gthread-2.0`
OBJS = san_scanner.o ics_scanner.o crafty_scanner.o netstuff.o crafty-adapter.o channels.o chess-backend.o clock-widget.o clocks.o drawing-backend.o configuration.o test.o main.o

%.c: %.lex %.h
	$(LEX) -P$*_ -o $*.c $<;

cairo-board: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f $(OBJS) san_scanner.c ics_scanner.c cairo-board

