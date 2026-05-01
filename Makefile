SOURCES = $(wildcard *.c)
EXECS = $(SOURCES:%.c=%)
CFLAGS = -std=c11 -g -lpthread -lm -lcrypto
CC=gcc

all: $(EXECS)

clean: 
	rm $(EXECS)
