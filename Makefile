TARGET = /usr/local/lib/liblal.so
CC = gcc
CFLAGS = -Wall -fpic -O2 -I/usr/include -g
HEADERS = ${wildcard *.h}
SOURCES = ${wildcard *.c}
OBJECTS = $(SOURCES:%.c=obj/%.o)

default: bin/liblal.so

bin/liblal.so: $(OBJECTS) bin
	$(CC) -shared -lbsd $(OBJECTS) -o $@ -s

bin/liblal.a: $(OBJECTS) bin
	ar rcs $@ $(OBJECTS)

bin:
	mkdir bin

obj:
	mkdir obj

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

/usr/local/include/lal/:
	sudo mkdir -p /usr/local/include/lal

headers: /usr/local/include/lal/ $(HEADERS) 
	for header in $(HEADERS); do \
		sudo cp $$header $<; \
	done

install: headers bin/liblal.so
	sudo cp bin/liblal.so /usr/local/lib

uninstall:
	sudo rm -f $(TARGET)
	sudo rm -rf /usr/local/include/lal

bin/lal: bin/liblal.a headers
	gcc test/main.c -o $@ -l:liblal.a -L./bin -lpthread -lbsd

clean:
	rm -rf bin/ obj/

test: bin/lal
