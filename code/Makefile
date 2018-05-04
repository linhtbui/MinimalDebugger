CXX = clang++
CC = gcc

CXXFLAGS = `pkg-config --cflags --libs libelf++ libdwarf++`


all: dump-lines main segfault

dump-lines: dump-lines.cc
	$(CXX) --std=c++11 -o dump-lines dump-lines.cc $(CXXFLAGS)

main: main.c
	$(CC) -o main main.c

segfault: segfault.c
	$(CC) -g segfault.c -o seg

clean:
	rm -f dump-lines main seg
