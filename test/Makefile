.PHONY: all clean

CC=g++
CFLAGS=-Wall -Wextra -Wpedantic -std=c++17 -pedantic-errors
LFLAGS=-pthread

all: bin/test
	@#

bin/test: obj/test_main.o obj/test_tcp.o
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

obj/%.o: %.cpp
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj bin
