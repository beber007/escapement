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
B = build

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

# -fno-strict-aliasing: the kernel reads and writes the same blocks as TCB, ETCB and the
# port's MinimalTCB, and GCC does use the rule it breaks: every image changes without it.
# It costs 68 bytes and 0.1 us per round on the Pico, as the RP2040 Makefile says (docs/method.md, 2026-09-25).
CFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -fdata-sections \
         -ffunction-sections -ffreestanding -Werror=comment -O2 -fno-strict-aliasing -MMD -MP
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
# make FLASH=1 links the same examples into the flash, in build-flash/, for the bootrom to
# boot them (RP2350_FLASH.ld, Escapement_ImageDef.S; tools/pico2_flash.sh writes them).
ifdef FLASH
   LINKERFILE = ../../RP2350_FLASH.ld
   B = build-flash
endif
# make TRACE=1 records a trace of the scheduling in RAM, read over SWD
# (Escapement_Trace.h, tools/read_trace.py).
ifdef TRACE
   CFLAGS += -DESCAPEMENT_TRACE
endif

# make SLEEP_GATE=1 has IdlePico2's idle task sleep in SLEEP, PLL_SYS stopped
# (Escapement_SleepGate.c); without it, the same image's plain WFI.
ifdef SLEEP_GATE
   CFLAGS += -DSLEEP_GATE
   SLEEPGATE = $(B)/Escapement_SleepGate.o $(B)/Escapement_Core1.o
endif
# make DEEP=1: SleepPico2's cheaper sleeps, PLL_SYS stopped or the crystal's start shorter.
ifdef DEEP
   CFLAGS += -DSLEEP_DEEP
endif
# make RUN=1 [RUN_VSEL=9]: SleepPico2 computing at four operating points rather than
# sleeping, 1.10 V or, below it, 1.00 V (SleepPico2.c).
ifdef RUN
   CFLAGS += -DSLEEP_RUN
endif
ifdef RUN_VSEL
   CFLAGS += -DRUN_VSEL=$(RUN_VSEL)u
endif
# make PHASE_PERIODS=20: SleepPico2's phases 2 s long rather than 30, to check them.
ifdef PHASE_PERIODS
   CFLAGS += -DPHASE_PERIODS=$(PHASE_PERIODS)u
endif

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p $(B))

all: $(B)/TaskLEDPico2.elf $(B)/UARTEchoPico2.elf $(B)/TestTimerEventPico2.elf \
     $(B)/FourSlotCoresPico2.elf $(B)/ThreeSlotCoresPico2.elf $(B)/TaskWrapPico2.elf \
     $(B)/FIFOCoresPico2.elf $(B)/IPCPico2.elf $(B)/SoakPico2.elf $(B)/LitmusPico2.elf \
     $(B)/UARTSendersPico2.elf $(B)/SleepPico2.elf $(B)/IdlePico2.elf \
     $(B)/StackGuardPico2.elf $(B)/LposcPico2.elf $(B)/DormantPico2.elf \
     $(B)/DormantCalAsleepPico2.elf $(B)/AwakeCalAsleepPico2.elf \
     $(B)/RoscCalAsleepPico2.elf
# The soft kernel adds SoakFirmPico2, SoakPico2 with an (m,k)-firm task whose optional
# instances the kernel drops under load (SoakPico2.c).
ifeq ($(KERNEL),SOFT)
all: $(B)/SoakFirmPico2.elf
endif

clean:
	rm -f $(B)/*.o $(B)/*.elf $(B)/*.bin $(B)/*.d

bin: all $(B)/TaskLEDPico2.bin $(B)/UARTEchoPico2.bin $(B)/TestTimerEventPico2.bin \
     $(B)/FourSlotCoresPico2.bin $(B)/ThreeSlotCoresPico2.bin $(B)/TaskWrapPico2.bin \
     $(B)/FIFOCoresPico2.bin $(B)/IPCPico2.bin $(B)/SoakPico2.bin $(B)/LitmusPico2.bin \
     $(B)/UARTSendersPico2.bin

$(B)/%.bin : $(B)/%.elf
	$(OBJCOPY) -O binary $< $@

COMMON = $(B)/Escapement_RamEntry.o \
          $(B)/Escapement_CortexMx.o \
          $(B)/Escapement_CortexMx_a.o \
          $(B)/Escapement_Atomic.o \
          $(B)/Escapement_Interrupts.o \
          $(B)/Escapement_Timer.o \
          $(B)/Escapement_Processor.o \
          $(B)/EscapementHard.o \
          $(B)/EscapementSoft.o \
          $(B)/EscapementHardPA.o
ifdef FLASH
   COMMON += $(B)/Escapement_ImageDef.o
endif

$(B)/Escapement_RamEntry.o : ../../Escapement_RamEntry.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_ImageDef.o : ../../Escapement_ImageDef.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_CortexMx.o : ../../../Escapement_CortexMx.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_CortexMx_a.o : ../../../Escapement_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Atomic.o : ../../../Escapement_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Interrupts.o : ../../Escapement_Interrupts.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Timer.o : ../../Escapement_Timer.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Processor.o : ../../Escapement_Processor.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_UART.o : ../../Escapement_UART.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_TimerEvent.o : ../../Escapement_TimerEvent.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Core1.o : ../../Escapement_Core1.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_CoreQueue.o : ../../Escapement_CoreQueue.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/TaskLEDPico2.o : TaskLEDPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/StackGuardPico2.o : StackGuardPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/LposcPico2.o : LposcPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/DormantPico2.o : DormantPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/DormantCalAsleepPico2.o : DormantPico2.c
	$(CC) $(CFLAGS) -DCAL_ASLEEP_CLOCKS $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/AwakeCalAsleepPico2.o : DormantPico2.c
	$(CC) $(CFLAGS) -DCAL_ASLEEP_CLOCKS -DSTAY_AWAKE $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/RoscCalAsleepPico2.o : DormantPico2.c
	$(CC) $(CFLAGS) -DCAL_ASLEEP_CLOCKS -DSLEEP_ON_ROSC $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/UARTEchoPico2.o : UARTEchoPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/UARTSendersPico2.o : UARTSendersPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/TestTimerEventPico2.o : TestTimerEventPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/FourSlotCoresPico2.o : FourSlotCoresPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/ThreeSlotCoresPico2.o : ThreeSlotCoresPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/FIFOCoresPico2.o : FIFOCoresPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/TaskWrapPico2.o : TaskWrapPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/IPCPico2.o : IPCPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakPico2.o : SoakPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakFirmPico2.o : SoakPico2.c
	$(CC) $(CFLAGS) -DSOAK_FIRM $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/LitmusPico2.o : LitmusPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SleepPico2.o : SleepPico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/IdlePico2.o : IdlePico2.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_SleepGate.o : ../../Escapement_SleepGate.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

$(B)/TaskLEDPico2.elf : $(COMMON) $(B)/TaskLEDPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/TaskLEDPico2.o -T$(LINKERFILE) -lgcc
$(B)/StackGuardPico2.elf : $(COMMON) $(B)/StackGuardPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/StackGuardPico2.o -T$(LINKERFILE) -lgcc
$(B)/LposcPico2.elf : $(COMMON) $(B)/LposcPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/LposcPico2.o -T$(LINKERFILE) -lgcc
$(B)/DormantPico2.elf : $(COMMON) $(B)/DormantPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/DormantPico2.o -T$(LINKERFILE) -lgcc
$(B)/DormantCalAsleepPico2.elf : $(COMMON) $(B)/DormantCalAsleepPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/DormantCalAsleepPico2.o -T$(LINKERFILE) -lgcc
$(B)/AwakeCalAsleepPico2.elf : $(COMMON) $(B)/AwakeCalAsleepPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/AwakeCalAsleepPico2.o -T$(LINKERFILE) -lgcc
$(B)/RoscCalAsleepPico2.elf : $(COMMON) $(B)/RoscCalAsleepPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/RoscCalAsleepPico2.o -T$(LINKERFILE) -lgcc
$(B)/UARTEchoPico2.elf : $(COMMON) $(B)/Escapement_UART.o $(B)/UARTEchoPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_UART.o $(B)/UARTEchoPico2.o -T$(LINKERFILE) -lgcc
$(B)/UARTSendersPico2.elf : $(COMMON) $(B)/Escapement_UART.o $(B)/UARTSendersPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_UART.o $(B)/UARTSendersPico2.o -T$(LINKERFILE) -lgcc
$(B)/TestTimerEventPico2.elf : $(COMMON) $(B)/Escapement_TimerEvent.o $(B)/TestTimerEventPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_TimerEvent.o $(B)/TestTimerEventPico2.o -T$(LINKERFILE) -lgcc
$(B)/FourSlotCoresPico2.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/FourSlotCoresPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/FourSlotCoresPico2.o -T$(LINKERFILE) -lgcc
$(B)/ThreeSlotCoresPico2.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/ThreeSlotCoresPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/ThreeSlotCoresPico2.o -T$(LINKERFILE) -lgcc
$(B)/FIFOCoresPico2.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_CoreQueue.o $(B)/FIFOCoresPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_CoreQueue.o $(B)/FIFOCoresPico2.o -T$(LINKERFILE) -lgcc
$(B)/TaskWrapPico2.elf : $(COMMON) $(B)/TaskWrapPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/TaskWrapPico2.o -T$(LINKERFILE) -lgcc
$(B)/IPCPico2.elf : $(COMMON) $(B)/IPCPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/IPCPico2.o -T$(LINKERFILE) -lgcc
$(B)/SoakPico2.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_TimerEvent.o $(B)/SoakPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_TimerEvent.o $(B)/SoakPico2.o -T$(LINKERFILE) -lgcc
$(B)/SoakFirmPico2.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_TimerEvent.o $(B)/SoakFirmPico2.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/LitmusPico2.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/LitmusPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/LitmusPico2.o -T$(LINKERFILE) -lgcc
# SleepPico2: no kernel, the three sleeps of the chip for the PPK2 (SleepPico2.c).
$(B)/SleepPico2.elf : $(COMMON) $(B)/SleepPico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(B)/SleepPico2.o -T$(LINKERFILE) -lgcc
$(B)/IdlePico2.elf : $(COMMON) $(SLEEPGATE) $(B)/Escapement_TimerEvent.o $(B)/IdlePico2.o
	$(CC) $(LDFLAGS) -o $@ $(COMMON) $(SLEEPGATE) $(B)/Escapement_TimerEvent.o $(B)/IdlePico2.o -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard $(B)/*.d)
