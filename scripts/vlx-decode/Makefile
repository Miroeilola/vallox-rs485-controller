# vlx-decode — capture analysis tool for the Vallox DIGIT bus.
# Built from the same codec the firmware runs, so the two cannot disagree.
#
#   make && ./vlx-decode --demo

CC     ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
CORE   := ../../firmware/components/vallox_protocol

vlx-decode: vlx-decode.c $(CORE)/vallox_protocol.c $(CORE)/include/vallox_protocol.h
	$(CC) $(CFLAGS) -I$(CORE)/include -o $@ vlx-decode.c $(CORE)/vallox_protocol.c

.PHONY: clean check
check: vlx-decode
	./vlx-decode --demo > /dev/null

clean:
	rm -f vlx-decode
