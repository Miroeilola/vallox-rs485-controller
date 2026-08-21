# Host-side unit tests for the transport-agnostic protocol codec.
# No ESP-IDF, no hardware: these run on any machine and in CI.
#
#   make test

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g
CORE    := ../../components/vallox_protocol
SRCS    := $(CORE)/vallox_protocol.c test_vallox_protocol.c

run_tests: $(SRCS) $(CORE)/include/vallox_protocol.h
	$(CC) $(CFLAGS) -I$(CORE)/include -o $@ $(SRCS)

.PHONY: test clean
test: run_tests
	./run_tests

clean:
	rm -f run_tests
