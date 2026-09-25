## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Distributed under the terms of LICENSE at the root of this repository.

# Builds the kernel for the machine it runs on, with a simulated target layer, and
# exercises the scheduler (test_scheduler) and the inter-task communication (test_ipc).
# Time is a variable here, so a task set of any size costs nothing and the clock can jump
# from one event to the next: three wraparounds of the kernel clock, an hour of real time
# on the board, take a fraction of a second.
#
# test_scheduler is built nine times: the hard, the soft and the power-aware kernel, each
# under earliest deadline first (edf) and deadline-monotonic scheduling (dm), and the
# power-aware kernel with its three other power-management policies; test_ipc three
# times, once per kernel.
#
#   make -C test/host run

CC ?= cc
# An integer division by zero traps on x86 but yields 0 on arm64, so a test could pass on
# one host and crash on the other: the sanitizer makes it an error on both. AddressSanitizer
# catches a read past a block: the idle task, allocated smaller than a task, was read so,
# and the result depended on whatever the heap held next to it.
# The CI also builds at -O2 under the whole of UndefinedBehaviorSanitizer, into its own
# directory: the optimiser takes more from undefined behaviour at -O2, the level of the
# firmware.
#   make -C test/host run OPT=-O2 SANITIZE=address,undefined BUILD=build-O2
OPT ?= -O1
SANITIZE ?= address,integer-divide-by-zero,signed-integer-overflow,shift
BUILD ?= build
# -fno-strict-aliasing as the firmware is built (the Makefiles of the examples).
# The kernels' compiler barriers call HostCompilerBarrier, where a test may take the
# timer interrupt (host_port.c).
CFLAGS = -I. -I../../Escapement -Wall $(OPT) -fno-strict-aliasing -g -fsanitize=$(SANITIZE) \
         -fno-sanitize-recover=all '-DCompilerBarrier()=HostCompilerBarrier()'
# What OSMalloc hands out is never freed, by design; Linux would report it as leaked.
export ASAN_OPTIONS = detect_leaks=0
HARD = ../../Escapement/EscapementHard.c
SOFT = ../../Escapement/EscapementSoft.c
PA = ../../Escapement/EscapementHardPA.c
COREQUEUE = ../../Escapement/CORTEX-Mx/RP2350/Escapement_CoreQueue.c
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

$(shell mkdir -p $(BUILD))

run: $(BUILDS:%=$(BUILD)/test_scheduler_%) $(BUILD)/test_ipc_hard $(BUILD)/test_ipc_soft $(BUILD)/test_ipc_pa
	@for b in $(BUILDS); do \
	   echo; echo "=== $$b"; \
	   ./$(BUILD)/test_scheduler_$$b && ./$(BUILD)/test_scheduler_$$b create && \
	   ./$(BUILD)/test_scheduler_$$b priority && \
	   ./$(BUILD)/test_scheduler_$$b wrap && ./$(BUILD)/test_scheduler_$$b wrapsim && \
	   ./$(BUILD)/test_scheduler_$$b wrapinside && \
	   ./$(BUILD)/test_scheduler_$$b wrapevents && \
	   ./$(BUILD)/test_scheduler_$$b events && ./$(BUILD)/test_scheduler_$$b lull && \
	   ./$(BUILD)/test_scheduler_$$b suspend && ./$(BUILD)/test_scheduler_$$b signalinside && \
	   ./$(BUILD)/test_scheduler_$$b busy && \
	   ./$(BUILD)/test_scheduler_$$b early && ./$(BUILD)/test_scheduler_$$b slack && \
	   ./$(BUILD)/test_scheduler_$$b expiry && ./$(BUILD)/test_scheduler_$$b reclaim && \
	   ./$(BUILD)/test_scheduler_$$b reuse && ./$(BUILD)/test_scheduler_$$b overrun && \
	   ./$(BUILD)/test_scheduler_$$b minspeed && \
	   ./$(BUILD)/test_scheduler_$$b endinside && ./$(BUILD)/test_scheduler_$$b endinsidebusy && \
	   ./$(BUILD)/test_scheduler_$$b timewrap && ./$(BUILD)/test_scheduler_$$b timewrapbusy && \
	   ./$(BUILD)/test_scheduler_$$b timewrapidle || exit 1; \
	   case $$b in soft_*) ./$(BUILD)/test_scheduler_$$b firm && \
	                      ./$(BUILD)/test_scheduler_$$b firmwrap && \
	                      ./$(BUILD)/test_scheduler_$$b firmlong && \
	                      ./$(BUILD)/test_scheduler_$$b firmevents && \
	                      ./$(BUILD)/test_scheduler_$$b firmwait && \
	                      ./$(BUILD)/test_scheduler_$$b firmeventwait || exit 1;; esac; \
	done
	@echo; echo "=== hard"; ./$(BUILD)/test_ipc_hard
	@echo; echo "=== soft"; ./$(BUILD)/test_ipc_soft
	@echo; echo "=== pa"; ./$(BUILD)/test_ipc_pa

.SECONDEXPANSION:
$(BUILD)/test_scheduler_%: test_scheduler.c host_port.c $$(KERNEL_$$*) $(HEADERS)
	$(CC) $(CFLAGS) $(FLAGS_$*) -o $@ test_scheduler.c host_port.c $(KERNEL_$*)

$(BUILD)/test_ipc_hard: test_ipc.c host_port.c $(HARD) $(COREQUEUE) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ test_ipc.c host_port.c $(HARD) $(COREQUEUE)

$(BUILD)/test_ipc_soft: test_ipc.c host_port.c $(SOFT) $(COREQUEUE) $(HEADERS)
	$(CC) $(CFLAGS) -DHOST_KERNEL_SOFT -o $@ test_ipc.c host_port.c $(SOFT) $(COREQUEUE)

$(BUILD)/test_ipc_pa: test_ipc.c host_port.c $(PA) $(COREQUEUE) $(HEADERS)
	$(CC) $(CFLAGS) -DHOST_KERNEL_PA -o $@ test_ipc.c host_port.c $(PA) $(COREQUEUE)

clean:
	rm -rf $(BUILD)

.PHONY: run clean
