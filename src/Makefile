CXX = clang++
CC = gcc

CXXFLAGS = `pkg-config --cflags --libs libelf++ libdwarf++`


all: cmain dump-lines main segfault

cmain: main.c
	$(CC) -c -o main.o main.c

dump-lines: dump-lines
	$(CXX) -c --std=c++11 -o dump-lines.o dump-lines.cc $(CXXFLAGS)

main: main.c
	$(CXX) --std=c++1 -o gdb dump-lines.o main.o $(CXXFLAGS)

segfault: segfault.c
	$(CC) -g segfault.c -o seg

clean:
	rm -f dump-lines.o main.o seg gdb
