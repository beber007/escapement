## Copyright (c) 2006-2012 MIS Institute of the HEIG-VD affiliated to the University of
## Applied Sciences of Western Switzerland. All rights reserved.
## Permission to use, copy, modify, and distribute this software and its documentation
## for any purpose, without fee, and without written agreement is hereby granted, pro-
## vided that the above copyright notice, the following three sentences and the authors
## appear in all copies of this software and in the software where it is used.
## IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG-VD NOR THE UNIVERSITY OF APPLIED
## SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
## INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
## DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG-VD OR THE UNIVERSITY OF APPLIED
## SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
## THE MIS INSTITUTE, THE HEIG-VD AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
## ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
## WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG-VD
## AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
## TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
## Authors: MIS-TIC
##
## Escapement - Lightweight Power-Aware Real-Time OS, derived from ZottaOS.
## Modifications Copyright (c) 2026 Bertrand Hurst, distributed under the same terms;
## see LICENSE and NOTICE at the root of this repository.

# The Arduino UNO Q (STM32U585, Cortex-M33): the examples of the Pico 2 transposed, run
# from SRAM, loaded by tools/unoq_load.sh; the flash holds Arduino's firmware, which the
# images leave in place. Built for a Cortex-M33 without its floating-point unit, so that the
# compiler can never use it: the generic layer saves the context of a Cortex-M33 as that
# of a Cortex-M4 without one (Escapement_CortexMx_a.S).

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

CPU = cortex-m33+nofp
LINKERFILE = ../../STM32U5_SRAM.ld

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

# -fno-strict-aliasing: the kernel reads and writes the same blocks as TCB, ETCB and the
# port's MinimalTCB, as the RP2040 Makefile says (docs/method.md, 2026-09-25).
CFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -fdata-sections \
         -ffunction-sections -ffreestanding -Werror=comment -O2 -fno-strict-aliasing -MMD -MP
AFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -MMD -MP
LDFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wl,--gc-sections -nostdlib

# Build variants, which the CI also runs:
#   make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING    the other scheduling algorithm
#   make KERNEL=SOFT                                the soft real-time kernel
# Also: make TRACE=1, a trace of the scheduling in RAM (not built by the CI), and
# make PHASES=30, SleepU5 alternating 30 s in Stop 2 and 30 s in Sleep (SleepU5.c).
# The power-aware kernel is not ported yet.
ifeq ($(KERNEL),PA)
   $(error the power-aware kernel is not ported to the STM32U5 yet)
endif
ifdef SCHEDULER
   CFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
   AFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
endif
ifeq ($(KERNEL),SOFT)
   CFLAGS += -DESCAPEMENT_VERSION_SOFT
   AFLAGS += -DESCAPEMENT_VERSION_SOFT
endif
ifdef TRACE
   CFLAGS += -DESCAPEMENT_TRACE
endif
ifdef PHASES
   CFLAGS += -DSLEEP_PHASES=$(PHASES)
endif
# make PHASES=30 WAKE=2200: the second phase in Stop 2 too, woken 2,200 us before each
# event rather than OS_STOP2_WAKE_US (SleepU5.c).
ifdef WAKE
   CFLAGS += -DSLEEP_WAKE_B=$(WAKE)u
endif
# make PHASES=30 RUN=80000: the second phase computes for 80,000 us of each instance, the
# current of the core running (SleepU5.c).
ifdef RUN
   CFLAGS += -DSLEEP_RUN_B=$(RUN)u
endif
# make MHZ=80: the system clock at 80 MHz rather than 160; 40 and 16 too, each in the
# lowest voltage range that runs it (Escapement_Processor.c).
ifdef MHZ
   CFLAGS += -DOS_SYSTEM_CLOCK_HZ=$(MHZ)000000u
endif
# make NOHSE=1: the HSE never started, PLL1 on the MSIS of range 2 at start and at every
# wake-up from Stop 2, the path of a board without the HSE (Escapement_Processor.c).
ifdef NOHSE
   CFLAGS += -DOS_NO_HSE
endif

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

EXAMPLES = TaskLEDU5 UARTEchoU5 TestTimerEventU5 TaskWrapU5 IPCU5 SoakU5 TestLPTimerU5 SleepU5 \
           SleepWrapU5 Stop2EventWrapU5 SleepNoHSEU5 StackGuardU5

all: $(EXAMPLES:%=build/%.elf)

clean:
	rm -f build/*.o build/*.elf build/*.bin build/*.d

bin: all $(EXAMPLES:%=build/%.bin)

build/%.bin : build/%.elf
	$(OBJCOPY) -O binary $< $@

COMMON = build/Escapement_CortexMx.o \
          build/Escapement_CortexMx_a.o \
          build/Escapement_Atomic.o \
          build/Escapement_Interrupts.o \
          build/Escapement_Timer.o \
          build/Escapement_Processor.o \
          build/EscapementHard.o \
          build/EscapementSoft.o \
          build/EscapementHardPA.o \
          build/Escapement_RamEntry.o

build/Escapement_CortexMx.o : ../../../Escapement_CortexMx.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_CortexMx_a.o : ../../../Escapement_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Atomic.o : ../../../Escapement_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_RamEntry.o : ../../Escapement_RamEntry.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_%.o : ../../Escapement_%.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/%U5.o : %U5.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDU5.elf : $(COMMON) build/TaskLEDU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/StackGuardU5.elf : $(COMMON) build/StackGuardU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/UARTEchoU5.elf : $(COMMON) build/Escapement_UART.o build/UARTEchoU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/TestTimerEventU5.elf : $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/TaskWrapU5.elf : $(COMMON) build/TaskWrapU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/IPCU5.elf : $(COMMON) build/IPCU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/TestLPTimerU5.elf : $(COMMON) build/Escapement_LPTimer.o build/TestLPTimerU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# LPUART1 at 115,200 baud: SleepU5's link wakes the chip with a byte before each frame
# (SleepU5.c), and SoakU5 never enters Stop 2; an image that does without that byte keeps
# 57,600 (Escapement_UART.c).
build/Escapement_UARTLink.o : ../../Escapement_UART.c
	$(CC) $(CFLAGS) -DOS_LPUART1_BAUD_RATE=115200u $(INCLUDE_ESCAPEMENT) -o $@ $<
build/SleepU5.elf : $(COMMON) build/Escapement_LPTimer.o build/Escapement_Stop2.o \
                    build/Escapement_UARTLink.o build/Escapement_TimerEvent.o build/SleepU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# SleepU5 as on a board without the HSE (OS_NO_HSE, Escapement_Processor.c): PLL1 on the
# MSIS of range 2 at start and at every wake-up from Stop 2, which no board on the bench
# runs otherwise, both U5 boards having their HSE.
build/Escapement_ProcessorNoHSE.o : ../../Escapement_Processor.c
	$(CC) $(CFLAGS) -DOS_NO_HSE $(INCLUDE_ESCAPEMENT) -o $@ $<
build/SleepNoHSEU5.elf : $(filter-out build/Escapement_Processor.o,$(COMMON)) \
                         build/Escapement_ProcessorNoHSE.o build/Escapement_LPTimer.o \
                         build/Escapement_Stop2.o build/Escapement_UARTLink.o \
                         build/Escapement_TimerEvent.o build/SleepU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# SleepU5 with its times a thousand times longer, for the wrap under Renode (SleepU5.c),
# the margins of the idle task in Stop 2 with them; the part kept to raise the clock is
# 1.2 ms there, the sleep on the MSIS before it held under 60,000 ticks of LPTIM1.
SCALED = -DTIME_SCALE=1000 -DOS_STOP2_WAKE_US=3000000u -DOS_STOP2_MIN_US=5000000u \
         -DOS_STOP2_RAISE_US=1200000u
build/SleepWrapU5.o : SleepU5.c
	$(CC) $(CFLAGS) $(SCALED) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Stop2Wrap.o : ../../Escapement_Stop2.c
	$(CC) $(CFLAGS) $(SCALED) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/SleepWrapU5.elf : $(COMMON) build/Escapement_LPTimer.o build/Escapement_Stop2Wrap.o \
                        build/Escapement_UARTLink.o build/Escapement_TimerEvent.o \
                        build/SleepWrapU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# The idle task in Stop 2 while an event-driven task's arrival lies beyond the wrap, scaled
# as SleepWrapU5 for Renode (Stop2EventWrapU5.c).
build/Stop2EventWrapU5.o : Stop2EventWrapU5.c
	$(CC) $(CFLAGS) $(SCALED) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Stop2EventWrapU5.elf : $(COMMON) build/Escapement_LPTimer.o build/Escapement_Stop2Wrap.o \
                             build/Escapement_TimerEvent.o build/Stop2EventWrapU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
build/SoakU5.elf : $(COMMON) build/Escapement_TimerEvent.o build/Escapement_UARTLink.o \
                   build/SoakU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
