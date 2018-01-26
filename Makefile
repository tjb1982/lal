TARGET = /usr/local/lib/liblal.so
CC = gcc
CFLAGS = -Wall -O2 -fPIC -I/usr/include -g -DXLOG_USE_COLOR -D_GNU_SOURCE
HEADERS = ${wildcard *.h}
SOURCES = ${wildcard *.c}
OBJECTS = $(SOURCES:%.c=obj/%.o)
LIBS = -pthread -lbsd
SUDO = $(shell which sudo)

default: bin/liblal.so

bin/liblal.so: $(OBJECTS) bin
	$(CC) -shared $(LIBS) $(OBJECTS) -o $@ -s

bin/liblal.a: $(OBJECTS) bin
	ar rcs $@ $(OBJECTS)

bin:
	mkdir bin

obj:
	mkdir obj

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

/usr/local/include/lal/:
	$(SUDO) mkdir -p /usr/local/include/lal

headers: /usr/local/include/lal/ $(HEADERS) 
	for header in $(HEADERS); do \
		$(SUDO) cp $$header $<; \
	done

install: headers bin/liblal.so
	$(SUDO) cp bin/liblal.so /usr/local/lib

uninstall:
	$(SUDO) rm -f $(TARGET)
	$(SUDO) rm -rf /usr/local/include/lal

bin/lal: bin/liblal.a headers
	gcc test/main.c -o $@ -l:liblal.a -L./bin $(LIBS)

bin/llal: install
	gcc test/main.c -o $@ -llal -L./bin $(LIBS)

clean:
	rm -rf bin/ obj/

test: bin/lal

sudo:
	echo $(SUDO)
