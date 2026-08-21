# Host-side unit tests for the transport-agnostic core.
# No ESP-IDF, no hardware: these run on any machine and in CI.
#
#   make && ./run_tests

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g
CORE    := ../../components/device_core
SRCS    := $(CORE)/device_core.c test_device_core.c

run_tests: $(SRCS) $(CORE)/include/device_core.h
	$(CC) $(CFLAGS) -I$(CORE)/include -o $@ $(SRCS)

.PHONY: test clean
test: run_tests
	./run_tests

clean:
	rm -f run_tests
