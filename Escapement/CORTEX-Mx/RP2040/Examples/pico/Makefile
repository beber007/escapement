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

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

CPU = cortex-m0plus
# The firmware runs from SRAM: no second stage bootloader to write.
LINKERFILE = ../../RP2040_SRAM.ld

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -fdata-sections -ffunction-sections \
         -ffreestanding -Werror=comment -O2 -MMD -MP
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -MMD -MP

# Build variants, which the CI also runs:
#   make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING    the other scheduling algorithm
#   make KERNEL=SOFT                                the soft real-time kernel
#   make KERNEL=PA                                  the power-aware kernel
#   make KERNEL=PA UNDERVOLT=1                      the same, below the specified voltage
#   make KERNEL=PA POWER=DRA                        another power-management policy
# Also: make TRACE=1, a trace of the scheduling in RAM (not built by the CI).
ifdef SCHEDULER
   CFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
   AFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
endif
ifeq ($(KERNEL),SOFT)
   CFLAGS += -DESCAPEMENT_VERSION_SOFT
   AFLAGS += -DESCAPEMENT_VERSION_SOFT
endif
ifeq ($(KERNEL),PA)
   CFLAGS += -DESCAPEMENT_VERSION_HARD_PA
   AFLAGS += -DESCAPEMENT_VERSION_HARD_PA
endif
ifdef UNDERVOLT
   CFLAGS += -DESCAPEMENT_RP2040_UNDERVOLT
endif
# make KERNEL=PA POWER=DRA picks another power-management policy than OTE: DRA or DR_OTE,
# which impose EDF*, or DM_SLACK, which imposes deadline-monotonic scheduling.
ifdef POWER
   CFLAGS += -DPOWER_MANAGEMENT=$(POWER)
endif
# make KERNEL=PA SLEEP_SPEED=0 lets the idle task sleep at 12 MHz rather than 125.
ifdef SLEEP_SPEED
   CFLAGS += -DOS_SLEEP_SPEED=$(SLEEP_SPEED)
endif
# make TRACE=1 records a trace of the scheduling in RAM, read over SWD
# (Escapement_Trace.h, tools/read_trace.py).
ifdef TRACE
   CFLAGS += -DESCAPEMENT_TRACE
endif

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

all: build/TaskLEDPico.elf build/UARTEchoPico.elf build/TaskWrapPico.elf \
     build/TestTimerEventPico.elf build/FourSlotCoresPico.elf

clean:
	rm -f build/*.o build/*.elf build/*.bin build/*.d

bin: all build/TaskLEDPico.bin build/UARTEchoPico.bin build/TaskWrapPico.bin \
     build/TestTimerEventPico.bin build/FourSlotCoresPico.bin

build/%.bin : build/%.elf
	$(OBJCOPY) -O binary $< $@

COMMON = build/Escapement_RamEntry.o \
          build/Escapement_CortexMx.o \
          build/Escapement_CortexMx_a.o \
          build/Escapement_Atomic.o \
          build/Escapement_Interrupts.o \
          build/Escapement_Timer.o \
          build/Escapement_Processor.o \
          build/EscapementHard.o \
          build/EscapementSoft.o \
          build/EscapementHardPA.o

build/Escapement_RamEntry.o : ../../Escapement_RamEntry.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_CortexMx.o : ../../../Escapement_CortexMx.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_CortexMx_a.o : ../../../Escapement_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Atomic.o : ../../../Escapement_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Interrupts.o : ../../Escapement_Interrupts.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Timer.o : ../../Escapement_Timer.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Processor.o : ../../Escapement_Processor.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_UART.o : ../../Escapement_UART.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_TimerEvent.o : ../../Escapement_TimerEvent.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Core1.o : ../../Escapement_Core1.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TaskLEDPico.o : TaskLEDPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/UARTEchoPico.o : UARTEchoPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TaskWrapPico.o : TaskWrapPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventPico.o : TestTimerEventPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/FourSlotCoresPico.o : FourSlotCoresPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/BenchDVFSPico.o : BenchDVFSPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/BenchVregPico.o : BenchVregPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDPico.elf : $(COMMON) build/TaskLEDPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/TaskLEDPico.o -T$(LINKERFILE) -lgcc
build/UARTEchoPico.elf : $(COMMON) build/Escapement_UART.o build/UARTEchoPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/Escapement_UART.o build/UARTEchoPico.o -T$(LINKERFILE) -lgcc
build/TaskWrapPico.elf : $(COMMON) build/TaskWrapPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/TaskWrapPico.o -T$(LINKERFILE) -lgcc
build/TestTimerEventPico.elf : $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventPico.o -T$(LINKERFILE) -lgcc
build/FourSlotCoresPico.elf : $(COMMON) build/Escapement_Core1.o build/FourSlotCoresPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/Escapement_Core1.o build/FourSlotCoresPico.o -T$(LINKERFILE) -lgcc

# make KERNEL=PA bench: times the changes of operating point of the DVFS driver, which
# only the power-aware kernel builds (BenchDVFSPico.c), and the settling of the core
# regulator (BenchVregPico.c, which runs the chip below its specified voltage).
bench: build/BenchDVFSPico.elf build/BenchVregPico.elf
build/BenchDVFSPico.elf : $(COMMON) build/BenchDVFSPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/BenchDVFSPico.o -T$(LINKERFILE) -lgcc
build/BenchVregPico.elf : $(COMMON) build/BenchVregPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/BenchVregPico.o -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
