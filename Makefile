CFLAGS := -std=c11 -g -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L
# Auto-dependency generation
CFLAGS += -MMD -MP
LDLIBS := -lpthread -lm -lcrypto -lssl

EXECS := $(patsubst %.c,%,$(wildcard *.c))
C_OBJS := $(patsubst %.c,%.o,$(wildcard common/*.c))

.PHONY: all

all: $(EXECS)

%: %.c # remove the implicit rule that refuses to use dependencies

%: %.c $(C_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(C_OBJS) $(LDLIBS)

DEPFILES := $(C_OBJS:.o=.d) $(EXECS:%=%.d)
-include $(DEPFILES)

clean: 
	rm -f $(EXECS) $(C_OBJS) $(DEPFILES)
