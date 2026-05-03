CFLAGS := -std=c11 -g -Wall -Wextra -pedantic -lpthread -lm -lcrypto -D_POSIX_C_SOURCE=200809L

EXECS := $(patsubst %.c,%,$(wildcard *.c))
DEPS := $(wildcard common/*.h)
OBJS := $(patsubst %.c,%.o,$(wildcard common/*.c))

.PHONY: all

all: $(EXECS)

%: %.c $(DEPS) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS)

clean: 
	rm -f $(EXECS) $(OBJS)
