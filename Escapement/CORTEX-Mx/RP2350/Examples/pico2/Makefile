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

# The Raspberry Pi Pico 2 (RP2350, Cortex-M33), transposed from ../../../RP2040/Examples/pico.
# Built for a Cortex-M33 without its floating-point unit, so that the compiler can never
# use it: the generic layer saves the context of a Cortex-M33 as that of a Cortex-M4
# without one (Escapement_CortexMx_a.S).

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

CPU = cortex-m33+nofp
LINKERFILE = ../../RP2350_SRAM.ld

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -fdata-sections \
         -ffunction-sections -ffreestanding -Werror=comment -O2 -MMD -MP
AFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -MMD -MP
LDFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wl,--gc-sections -nostdlib

# Build variants, which the CI also runs:
#   make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING    the other scheduling algorithm
#   make KERNEL=SOFT                                the soft real-time kernel
# Also: make TRACE=1, a trace of the scheduling in RAM (not built by the CI).
# The power-aware kernel is not ported yet.
ifeq ($(KERNEL),PA)
   $(error the power-aware kernel is not ported to the RP2350 yet)
endif
ifdef SCHEDULER
   CFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
   AFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
endif
ifeq ($(KERNEL),SOFT)
   CFLAGS += -DESCAPEMENT_VERSION_SOFT
   AFLAGS += -DESCAPEMENT_VERSION_SOFT
endif
# make TRACE=1 records a trace of the scheduling in RAM, read over SWD
# (Escapement_Trace.h, tools/read_trace.py).
ifdef TRACE
   CFLAGS += -DESCAPEMENT_TRACE
endif

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

all: build/TaskLEDPico2.elf build/UARTEchoPico2.elf build/TestTimerEventPico2.elf \
     build/FourSlotCoresPico2.elf build/ThreeSlotCoresPico2.elf build/TaskWrapPico2.elf \
     build/FIFOCoresPico2.elf build/IPCPico2.elf build/SoakPico2.elf

clean:
	rm -f build/*.o build/*.elf build/*.bin build/*.d

bin: all build/TaskLEDPico2.bin build/UARTEchoPico2.bin build/TestTimerEventPico2.bin \
     build/FourSlotCoresPico2.bin build/ThreeSlotCoresPico2.bin build/TaskWrapPico2.bin \
     build/FIFOCoresPico2.bin build/IPCPico2.bin build/SoakPico2.bin

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
build/Escapement_CoreQueue.o : ../../Escapement_CoreQueue.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TaskLEDPico2.o : TaskLEDPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/UARTEchoPico2.o : UARTEchoPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventPico2.o : TestTimerEventPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/FourSlotCoresPico2.o : FourSlotCoresPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/ThreeSlotCoresPico2.o : ThreeSlotCoresPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/FIFOCoresPico2.o : FIFOCoresPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TaskWrapPico2.o : TaskWrapPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/IPCPico2.o : IPCPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/SoakPico2.o : SoakPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDPico2.elf : $(COMMON) build/TaskLEDPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/TaskLEDPico2.o -T$(LINKERFILE) -lgcc
build/UARTEchoPico2.elf : $(COMMON) build/Escapement_UART.o build/UARTEchoPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/Escapement_UART.o build/UARTEchoPico2.o -T$(LINKERFILE) -lgcc
build/TestTimerEventPico2.elf : $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventPico2.o -T$(LINKERFILE) -lgcc
build/FourSlotCoresPico2.elf : $(COMMON) build/Escapement_Core1.o build/FourSlotCoresPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/Escapement_Core1.o build/FourSlotCoresPico2.o -T$(LINKERFILE) -lgcc
build/ThreeSlotCoresPico2.elf : $(COMMON) build/Escapement_Core1.o build/ThreeSlotCoresPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/Escapement_Core1.o build/ThreeSlotCoresPico2.o -T$(LINKERFILE) -lgcc
build/FIFOCoresPico2.elf : $(COMMON) build/Escapement_Core1.o build/Escapement_CoreQueue.o build/FIFOCoresPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/Escapement_Core1.o build/Escapement_CoreQueue.o build/FIFOCoresPico2.o -T$(LINKERFILE) -lgcc
build/TaskWrapPico2.elf : $(COMMON) build/TaskWrapPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/TaskWrapPico2.o -T$(LINKERFILE) -lgcc
build/IPCPico2.elf : $(COMMON) build/IPCPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/IPCPico2.o -T$(LINKERFILE) -lgcc
build/SoakPico2.elf : $(COMMON) build/Escapement_Core1.o build/Escapement_TimerEvent.o build/SoakPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) build/Escapement_Core1.o build/Escapement_TimerEvent.o build/SoakPico2.o -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
