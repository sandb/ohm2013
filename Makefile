CC=gcc
LIBS=-lEGL -lX11 -lm -lGL

all: ohm2013

ohm2013: 
	$(CC) $(CFLAGS) $(LIBS) ohm2013.c -o ohm2013

clean:
	rm ohm2013
