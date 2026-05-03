CFLAGS := -std=c11 -g -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L
LDLIBS := -lpthread -lm -lcrypto

EXECS := $(patsubst %.c,%,$(wildcard *.c))
DEPS := $(wildcard common/*.h)
OBJS := $(patsubst %.c,%.o,$(wildcard common/*.c))

.PHONY: all

all: $(EXECS)

%: %.c # remove the implicit rule that refuses to use dependencies

%: %.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDLIBS)

common/%.o: common/%.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean: 
	rm -f $(EXECS) $(OBJS)
