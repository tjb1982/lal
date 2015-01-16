CC = clang
CFLAGS = -Wall -fpic -O2 -I/usr/include -g

default: build

build: network.c route.c utils.c
	mkdir -p bin
	mkdir -p obj
	$(CC) $(CFLAGS) -c network.c -o obj/network.o
	$(CC) $(CFLAGS) -c route.c -o obj/route.o
	$(CC) $(CFLAGS) -c request.c -o obj/request.o
	$(CC) $(CFLAGS) -c response.c -o obj/response.o
	$(CC) $(CFLAGS) -c utils.c -o obj/utils.o
	$(CC) -shared -L/usr/lib -lbsd obj/network.o obj/route.o obj/request.o obj/response.o obj/utils.o -o bin/liblal.so -s

install: bin/liblal.so
	mkdir -p /usr/local/include/lal
	cp network.h /usr/local/include/lal/
	cp route.h /usr/local/include/lal/
	cp request.h /usr/local/include/lal/
	cp response.h /usr/local/include/lal/
	cp utils.h /usr/local/include/lal/
	cp bin/liblal.so /usr/local/lib/

clean:
	rm -rf bin/ obj/

