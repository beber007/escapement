## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Builds the kernel for the machine it runs on, with a simulated target layer, and
# exercises the scheduler. Time is a variable here, so a task set of any size costs
# nothing and the 2^30 wraparound of the kernel clock is one assignment away rather
# than eighteen minutes.
#
#   make -C test/host run

CC ?= cc
CFLAGS = -I. -I../../Escapement -Wall -O1 -g
KERNEL = ../../Escapement/EscapementHard.c

$(shell mkdir -p build)

run: build/test_scheduler
	./build/test_scheduler

build/test_scheduler: test_scheduler.c host_port.c $(KERNEL) Escapement.h Escapement_Config.h \
                      Escapement_Types.h Escapement_Processor.h Escapement_Timer.h \
                      Escapement_Interrupts.h
	$(CC) $(CFLAGS) -o $@ test_scheduler.c host_port.c $(KERNEL)

clean:
	rm -rf build

.PHONY: run clean
