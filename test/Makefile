.PHONY: all clean

CC=g++
CFLAGS=-Wall -Wextra -Wpedantic -pedantic-errors -std=c++11
LFLAGS=-pthread

all: bin/test
	@#

bin/test: obj/test_main.o obj/test_tcp.o obj/test_udp.o
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

obj/%.o: %.cpp ../inetstream.hpp
	@mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf obj bin
