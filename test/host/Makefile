## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Builds the kernel for the machine it runs on, with a simulated target layer, and
# exercises the scheduler (test_scheduler) and the inter-task communication (test_ipc).
# Time is a variable here, so a task set of any size costs nothing and the clock can jump
# from one event to the next: three wraparounds of the kernel clock, an hour of real time
# on the board, take a fraction of a second.
#
# Everything is built four times: the hard and the soft kernel, each under earliest
# deadline first (edf) and deadline-monotonic scheduling (dm).
#
#   make -C test/host run

CC ?= cc
# An integer division by zero traps on x86 but yields 0 on arm64, so a test could pass on
# one host and crash on the other: the sanitizer makes it an error on both.
CFLAGS = -I. -I../../Escapement -Wall -O1 -g -fsanitize=integer-divide-by-zero \
         -fno-sanitize-recover=all
HARD = ../../Escapement/EscapementHard.c
SOFT = ../../Escapement/EscapementSoft.c
HEADERS = Escapement.h Escapement_Config.h Escapement_Types.h Escapement_Processor.h \
          Escapement_Timer.h Escapement_Interrupts.h

BUILDS = hard_edf hard_dm soft_edf soft_dm
FLAGS_hard_edf = -DHOST_SCHEDULER=1
FLAGS_hard_dm  = -DHOST_SCHEDULER=2
FLAGS_soft_edf = -DHOST_SCHEDULER=1 -DHOST_KERNEL_SOFT
FLAGS_soft_dm  = -DHOST_SCHEDULER=2 -DHOST_KERNEL_SOFT
KERNEL_hard_edf = $(HARD)
KERNEL_hard_dm  = $(HARD)
KERNEL_soft_edf = $(SOFT)
KERNEL_soft_dm  = $(SOFT)

$(shell mkdir -p build)

run: $(BUILDS:%=build/test_scheduler_%) build/test_ipc_hard build/test_ipc_soft
	@for b in $(BUILDS); do \
	   echo; echo "=== $$b"; \
	   ./build/test_scheduler_$$b && ./build/test_scheduler_$$b wrap && \
	   ./build/test_scheduler_$$b events || exit 1; \
	   case $$b in soft_*) ./build/test_scheduler_$$b firm || exit 1;; esac; \
	done
	@echo; echo "=== hard"; ./build/test_ipc_hard
	@echo; echo "=== soft"; ./build/test_ipc_soft

.SECONDEXPANSION:
build/test_scheduler_%: test_scheduler.c host_port.c $$(KERNEL_$$*) $(HEADERS)
	$(CC) $(CFLAGS) $(FLAGS_$*) -o $@ test_scheduler.c host_port.c $(KERNEL_$*)

build/test_ipc_hard: test_ipc.c host_port.c $(HARD) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ test_ipc.c host_port.c $(HARD)

build/test_ipc_soft: test_ipc.c host_port.c $(SOFT) $(HEADERS)
	$(CC) $(CFLAGS) -DHOST_KERNEL_SOFT -o $@ test_ipc.c host_port.c $(SOFT)

clean:
	rm -rf build

.PHONY: run clean
