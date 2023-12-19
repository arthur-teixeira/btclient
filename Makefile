CC = clang
CFLAGS = -Wall -Wextra -ggdb
SRCDIR = ./src
BINDIR = ./bin
SRCEXT = c
LIBS = -lssl -lcrypto -lunity -lcurl -lpthread -lrt

SRCS := $(shell find ./src ! -name '*_test.c' -type f -name '*.c')

OBJS := $(patsubst $(SRCDIR)/%.$(SRCEXT),$(BINDIR)/%.o,$(SRCS))

BINNAME = btclient

all: $(BINDIR)/$(BINNAME)

$(BINDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ 

$(BINDIR)/$(BINNAME): $(OBJS)
	$(CC) $(LIBS) $(CFLAGS) $^ -o $@ 

clean:
	rm -rf $(BINDIR)/*

run: clean all test


TESTFILES := $(shell find ./src -name '*_test.c' -type f)

.ONESHELL:

test:
	@echo "Building tests..."
	failed=""
	@for testfile in $(TESTFILES); do \
		testname=$$(basename $$testfile .c); \
		echo "Building $$testname..."; \
		$(CC) $(CFLAGS) $(filter-out ./bin/main.o, $(OBJS)) $$testfile -o $(BINDIR)/$$testname $(LIBS); \
		echo "Running $$testname..."; \
		if ! $(BINDIR)/$$testname; then \
			failed="$$failed\n$$testname"; \
		fi; \
	done; \

	@if [ -n "$$failed" ]; then \
		echo "The following tests failed: $$failed"; \
		exit 1; \
	else \
		echo "All tests passed!"; \
	fi

.PHONY: all test clean run

