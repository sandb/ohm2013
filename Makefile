CC=gcc
LIBS=-lEGL -lX11 -lm -lGLESv2

all: ohm2013

ohm2013: ohm2013.c 
	$(CC) $(CFLAGS) ohm2013.c -o ohm2013 $(LIBS)

clean: 
	rm -f ohm2013
