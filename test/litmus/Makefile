# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The litmus bench, on an Armv8-A host (litmus.c). `make run` builds the queue with its
# seven barriers and once without each, and runs them all; SKIP=<line> builds one mutant.

CC     ?= cc
QUEUE  := ../../Escapement/CORTEX-Mx/RP2350
CFLAGS := -O2 -std=c11 -Wall -Wextra -pthread -iquote . -I $(QUEUE)
BUILD  ?= build
SKIP   ?= 0
ARGS   ?=

$(BUILD)/litmus-$(SKIP): litmus.c Escapement.h $(QUEUE)/Escapement_CoreQueue.c | $(BUILD)
	$(CC) $(CFLAGS) -DSKIP_BARRIER=$(SKIP) -o $@ litmus.c -x c $(QUEUE)/Escapement_CoreQueue.c

$(BUILD):
	mkdir -p $@

run:
	sh ./run.sh $(ARGS)

clean:
	rm -rf $(BUILD)

.PHONY: run clean
