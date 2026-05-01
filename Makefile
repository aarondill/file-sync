CFLAGS = -std=c11 -g -lpthread -lm -lcrypto
CC=gcc

EXECS = file-server file-client
DEPS = util.h protocol.h
OBJS = util.o protocol.o

.PHONY: all

all: $(EXECS)

file-server: file-server.c $(DEPS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS)

file-client: file-client.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS)

clean: 
	rm -f $(EXECS) $(OBJS)
