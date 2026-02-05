# Makefile for HTTP Web Server

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I.

# Targets
all: server

server: server.o nethelp.o
	$(CC) $(CFLAGS) -o server server.o nethelp.o

# Object file rules
nethelp.o: nethelp.c nethelp.h
	$(CC) $(CFLAGS) -c nethelp.c

server.o: server.c nethelp.h
	$(CC) $(CFLAGS) -c server.c

# Utility rules
clean:
	rm -f *.o server
