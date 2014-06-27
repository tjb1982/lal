CC = gcc
CFLAGS = -Wall -fpic -O2 -I/usr/include

default: build

build: network.c route.c
	mkdir -p bin
	mkdir -p obj
	$(CC) $(CFLAGS) -c network.c -o obj/network.o
	$(CC) $(CFLAGS) -c route.c -o obj/route.o
	$(CC) -shared -L/usr/lib obj/network.o obj/route.o -o bin/liblal.so -s

install:
	mkdir -p /usr/local/include/lal
	cp network.h /usr/local/include/lal/
	cp route.h /usr/local/include/lal/
	cp bin/liblal.so /usr/local/lib/

clean:
	rm -rf bin/ obj/

