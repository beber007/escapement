# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The litmus bench, on an Armv8-A host: the queue between the cores (litmus.c) and the
# kernel's 3- and 4-slot buffers (slots.c, -4 for the latter). `make run` builds each with
# its barriers and once without each, and runs them all; SKIP=<line> builds one mutant.

CC     ?= cc
QUEUE  := ../../Escapement/CORTEX-Mx/RP2350
CFLAGS := -O2 -std=c11 -Wall -Wextra -pthread -iquote . -I $(QUEUE)
BUILD  ?= build
SKIP   ?= 0
ARGS   ?=

$(BUILD)/litmus-$(SKIP): litmus.c Escapement.h $(QUEUE)/Escapement_CoreQueue.c | $(BUILD)
	$(CC) $(CFLAGS) -DSKIP_BARRIER=$(SKIP) -o $@ litmus.c -x c $(QUEUE)/Escapement_CoreQueue.c

# The slot buffers: the kernel as it stands, with the host port of test/host built for
# the bench (HOST_LITMUS), not the queue's Escapement.h of this directory.
HARD   := ../../Escapement/EscapementHard.c
SLOTS_CFLAGS := -O2 -std=c11 -Wall -pthread -fno-strict-aliasing -I ../host -I ../../Escapement \
                -DHOST_LITMUS

$(BUILD)/slots-$(SKIP): slots.c $(HARD) ../host/host_port.c ../host/Escapement_Processor.h | $(BUILD)
	$(CC) $(SLOTS_CFLAGS) -DSKIP_BARRIER=$(SKIP) -o $@ slots.c ../host/host_port.c $(HARD)

slots: $(BUILD)/slots-$(SKIP)

$(BUILD):
	mkdir -p $@

run:
	sh ./run.sh $(ARGS)

clean:
	rm -rf $(BUILD)

.PHONY: run clean slots
