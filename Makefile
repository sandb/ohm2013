CC=gcc
LIBS=-lEGL -lX11 -lm -lGL

all: ohm2013

ohm2013: 
	$(CC) $(CFLAGS) $(LIBS) es2tri.c -o ohm2013
