# Yaay, gcc hardwired:
CC=gcc
SRCS=iodisplay.c
TARGETS=$(SRCS:.c=)
ARCHFLAGS=-DARCH=$(shell uname -m)
# TODO: unbuzzwordize
CPREFLAGS=-W -Wall -Wextra -Wconversion -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wformat=2 -Wformat-security
CFLAGS=$(CPREFLAGS) -std=gnu99 -pedantic -O3 -fno-common -floop-optimize2 -funsafe-loop-optimizations -Wunsafe-loop-optimizations -funswitch-loops -fstack-protector-all
DBGFLAGS=-ggdb3
LDFLAGS=

.PHONY: clean all

all: $(TARGETS)
	@echo Built $(TARGETS)

$(TARGETS): $(SRCS)
	@echo Building $@
	@$(CC) $(CFLAGS) $(DBGFLAGS) $(LDFLAGS) $(ARCHFLAGS) -o $@ $@.c

clean:
	$(RM) -rf *.d *.o *~ *.a *.so $(TARGETS)

