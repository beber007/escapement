## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Distributed under the terms of LICENSE at the root of this repository.

# Builds the kernel for the machine it runs on, with a simulated target layer, and
# exercises the scheduler (test_scheduler) and the inter-task communication (test_ipc).
# Time is a variable here, so a task set of any size costs nothing and the clock can jump
# from one event to the next: three wraparounds of the kernel clock, an hour of real time
# on the board, take a fraction of a second.
#
# Everything is built nine times: the hard, the soft and the power-aware kernel, each
# under earliest deadline first (edf) and deadline-monotonic scheduling (dm), and the
# power-aware kernel with its three other power-management policies.
#
#   make -C test/host run

CC ?= cc
# An integer division by zero traps on x86 but yields 0 on arm64, so a test could pass on
# one host and crash on the other: the sanitizer makes it an error on both. AddressSanitizer
# catches a read past a block: the idle task, allocated smaller than a task, was read so,
# and the result depended on whatever the heap held next to it.
CFLAGS = -I. -I../../Escapement -Wall -O1 -g \
         -fsanitize=address,integer-divide-by-zero,signed-integer-overflow -fno-sanitize-recover=all
# What OSMalloc hands out is never freed, by design; Linux would report it as leaked.
export ASAN_OPTIONS = detect_leaks=0
HARD = ../../Escapement/EscapementHard.c
SOFT = ../../Escapement/EscapementSoft.c
PA = ../../Escapement/EscapementHardPA.c
HEADERS = Escapement.h Escapement_Config.h Escapement_Types.h Escapement_Processor.h \
          Escapement_Timer.h Escapement_Interrupts.h

BUILDS = hard_edf hard_dm soft_edf soft_dm pa_edf pa_dm pa_dra pa_drote pa_dmslack
FLAGS_hard_edf = -DHOST_SCHEDULER=1
FLAGS_hard_dm  = -DHOST_SCHEDULER=2
FLAGS_soft_edf = -DHOST_SCHEDULER=1 -DHOST_KERNEL_SOFT
FLAGS_soft_dm  = -DHOST_SCHEDULER=2 -DHOST_KERNEL_SOFT
FLAGS_pa_edf   = -DHOST_SCHEDULER=1 -DHOST_KERNEL_PA
FLAGS_pa_dm    = -DHOST_SCHEDULER=2 -DHOST_KERNEL_PA
# The other power-management policies each impose their algorithm: EDF* for DRA and
# DR_OTE, deadline-monotonic scheduling for DM_SLACK.
FLAGS_pa_dra     = -DHOST_KERNEL_PA -DPOWER_MANAGEMENT=DRA
FLAGS_pa_drote   = -DHOST_KERNEL_PA -DPOWER_MANAGEMENT=DR_OTE
FLAGS_pa_dmslack = -DHOST_KERNEL_PA -DPOWER_MANAGEMENT=DM_SLACK
KERNEL_hard_edf = $(HARD)
KERNEL_hard_dm  = $(HARD)
KERNEL_soft_edf = $(SOFT)
KERNEL_soft_dm  = $(SOFT)
KERNEL_pa_edf   = $(PA)
KERNEL_pa_dm    = $(PA)
KERNEL_pa_dra     = $(PA)
KERNEL_pa_drote   = $(PA)
KERNEL_pa_dmslack = $(PA)

$(shell mkdir -p build)

run: $(BUILDS:%=build/test_scheduler_%) build/test_ipc_hard build/test_ipc_soft build/test_ipc_pa
	@for b in $(BUILDS); do \
	   echo; echo "=== $$b"; \
	   ./build/test_scheduler_$$b && ./build/test_scheduler_$$b wrap && \
	   ./build/test_scheduler_$$b events && ./build/test_scheduler_$$b busy && \
	   ./build/test_scheduler_$$b early && ./build/test_scheduler_$$b slack && \
	   ./build/test_scheduler_$$b expiry && ./build/test_scheduler_$$b reclaim || exit 1; \
	   case $$b in soft_*) ./build/test_scheduler_$$b firm || exit 1;; esac; \
	done
	@echo; echo "=== hard"; ./build/test_ipc_hard
	@echo; echo "=== soft"; ./build/test_ipc_soft
	@echo; echo "=== pa"; ./build/test_ipc_pa

.SECONDEXPANSION:
build/test_scheduler_%: test_scheduler.c host_port.c $$(KERNEL_$$*) $(HEADERS)
	$(CC) $(CFLAGS) $(FLAGS_$*) -o $@ test_scheduler.c host_port.c $(KERNEL_$*)

build/test_ipc_hard: test_ipc.c host_port.c $(HARD) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ test_ipc.c host_port.c $(HARD)

build/test_ipc_soft: test_ipc.c host_port.c $(SOFT) $(HEADERS)
	$(CC) $(CFLAGS) -DHOST_KERNEL_SOFT -o $@ test_ipc.c host_port.c $(SOFT)

build/test_ipc_pa: test_ipc.c host_port.c $(PA) $(HEADERS)
	$(CC) $(CFLAGS) -DHOST_KERNEL_PA -o $@ test_ipc.c host_port.c $(PA)

clean:
	rm -rf build

.PHONY: run clean
