CFLAGS = -Wall -Wextra -ggdb
SRCDIR = ./src
BINDIR = ./bin
SRCEXT = c

SRCS := $(shell find ./src ! -name '*_test.c' -type f -name '*.c')
LIBS = -lssl -lcrypto

OBJS := $(patsubst $(SRCDIR)/%.$(SRCEXT),$(BINDIR)/%.o,$(SRCS))

BINNAME = btclient

all: $(BINDIR)/$(BINNAME)

$(BINDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(@D)
	clang $(CFLAGS) -c $< -o $@ 

$(BINDIR)/$(BINNAME): $(OBJS)
	clang $(LIBS) $(CFLAGS) $^ -o $@ 

clean:
	rm -rf $(BINDIR)/*

.PHONY: clean
