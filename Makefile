CXX      = clang
CXXFLAGS = -g3 -Wall -Wextra -Wpedantic -Wshadow -O2
LDFLAGS  = -g3 

server: server.c parser.o
	${CXX} ${LDFLAGS} -o $@ $^

parser.o: parser.c parser.h
	${CXX} $(CXXFLAGS) -c parser.c
