TARGET = /usr/local/lib/liblal.so
CC = gcc
CXXC = g++
CFLAGS = -Wall -O2 -fPIC -I/usr/include -I. -Ilib -g -DXLOG_USE_COLOR -D_GNU_SOURCE
HEADERS = ${wildcard *.h}
SOURCES = ${wildcard *.c}
OBJECTS = $(SOURCES:%.c=obj/%.o)
LIBS = -pthread -lbsd
TEST_LIBS = -luuid -lpqxx
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

lal: bin/lal
bin/lal: bin/liblal.a headers
	$(CC) test/main.c -o $@ -l:liblal.a -L./bin $(LIBS) $(TEST_LIBS)

lalpp: bin/lalpp
bin/lalpp: bin/liblal.a headers
	$(CXXC) test/main.cpp -o $@ -Ilib -l:liblal.a -L./bin $(LIBS) $(TEST_LIBS)

llal: bin/llal
bin/llal: install
	$(CC) test/main.c -o $@ -llal -L./bin $(LIBS) $(TEST_LIBS)

clean:
	rm -rf bin/ obj/

test: bin/lal

testpp: bin/lalpp

sudo:
	echo $(SUDO)
