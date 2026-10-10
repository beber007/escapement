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
B = build

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

# -fno-strict-aliasing: the kernel reads and writes the same blocks as TCB, ETCB and the
# port's MinimalTCB, and GCC does use the rule it breaks: every image changes without it.
# It costs 68 bytes and 0.1 us per round on the Pico (docs/method.md, 2026-09-25).
CFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -fdata-sections -ffunction-sections \
         -ffreestanding -Werror=comment -O2 -fno-strict-aliasing -MMD -MP
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
# make SLEEP_GATE=1 has TaskLEDPico and TestTimerEventPico sleep in SLEEP, every clock
# but the timer's gated (Escapement_SleepGate.c), for the bench: not built by the CI.
ifdef SLEEP_GATE
   CFLAGS += -DSLEEP_GATE
   SLEEPGATE = $(B)/Escapement_SleepGate.o $(B)/Escapement_Core1.o
endif
# make FLASH=1 links the same examples into the flash, in build-flash/, after the second
# stage of the boot (RP2040_FLASH.ld, Escapement_Boot2.S, tools/rp2040_boot2.py), for
# tools/pico_flash.sh to write.
ifdef FLASH
   LINKERFILE = ../../RP2040_FLASH.ld
   B = build-flash
   CFLAGS += -DIMAGE_IN_FLASH
endif
# make TRACE=1 records a trace of the scheduling in RAM, read over SWD
# (Escapement_Trace.h, tools/read_trace.py).
ifdef TRACE
   CFLAGS += -DESCAPEMENT_TRACE
endif
ifdef RUN
   CFLAGS += -DSLEEP_RUN
endif
ifdef PHASE_PERIODS
   CFLAGS += -DPHASE_PERIODS=$(PHASE_PERIODS)u
endif

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p $(B))

all: $(B)/TaskLEDPico.elf $(B)/UARTEchoPico.elf $(B)/UARTSendersPico.elf $(B)/TaskWrapPico.elf \
     $(B)/TestTimerEventPico.elf $(B)/FourSlotCoresPico.elf $(B)/IPCPico.elf \
     $(B)/SoakPico.elf $(B)/StackGuardPico.elf
# The program of the README, built from the page itself, so that the two cannot part; it
# is written for the hard kernel.
ifeq ($(KERNEL),)
all: $(B)/ReadmeExample.elf
endif
# The soft kernel adds SoakFirmPico, SoakPico with an (m,k)-firm task whose optional
# instances the kernel drops under load (SoakPico.c).
ifeq ($(KERNEL),SOFT)
all: $(B)/SoakFirmPico.elf
endif

clean:
	rm -f $(B)/*.o $(B)/*.elf $(B)/*.bin $(B)/*.d $(B)/*.c $(B)/*.S

$(B)/ReadmeExample.c : ../../../../../README.md
	awk '/^## An application/ { a = 1 } a && /^```c$$/ { c = 1; next } c && /^```$$/ { exit } c' $< > $@
	test -s $@
$(B)/ReadmeExample.o : $(B)/ReadmeExample.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/ReadmeExample.elf : $(COMMON) $(B)/ReadmeExample.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/ReadmeExample.o -T$(LINKERFILE) -lgcc

bin: all $(B)/TaskLEDPico.bin $(B)/UARTEchoPico.bin $(B)/UARTSendersPico.bin $(B)/TaskWrapPico.bin \
     $(B)/TestTimerEventPico.bin $(B)/FourSlotCoresPico.bin $(B)/IPCPico.bin \
     $(B)/SoakPico.bin

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
   COMMON += $(B)/Escapement_Boot2.o
endif

$(B)/Escapement_RamEntry.o : ../../Escapement_RamEntry.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
# The second stage, assembled, cut to its section and given its CRC.
$(B)/Escapement_Boot2Code.o : ../../Escapement_Boot2.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Boot2.S : $(B)/Escapement_Boot2Code.o ../../../../../tools/rp2040_boot2.py
	$(OBJCOPY) -O binary -j .boot2 $< $(B)/Escapement_Boot2.bin
	python3 ../../../../../tools/rp2040_boot2.py $(B)/Escapement_Boot2.bin $@
$(B)/Escapement_Boot2.o : $(B)/Escapement_Boot2.S
	$(CC) $(AFLAGS) -o $@ $<
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
$(B)/Escapement_SleepGate.o : ../../Escapement_SleepGate.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/TaskLEDPico.o : TaskLEDPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/UARTEchoPico.o : UARTEchoPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/UARTSendersPico.o : UARTSendersPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/TaskWrapPico.o : TaskWrapPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/IPCPico.o : IPCPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakPico.o : SoakPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakFirmPico.o : SoakPico.c
	$(CC) $(CFLAGS) -DSOAK_FIRM $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/StackGuardPico.o : StackGuardPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/TestTimerEventPico.o : TestTimerEventPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/FourSlotCoresPico.o : FourSlotCoresPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/BenchDVFSPico.o : BenchDVFSPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/BenchVregPico.o : BenchVregPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

$(B)/TaskLEDPico.elf : $(COMMON) $(SLEEPGATE) $(B)/TaskLEDPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(SLEEPGATE) $(B)/TaskLEDPico.o -T$(LINKERFILE) -lgcc
$(B)/UARTEchoPico.elf : $(COMMON) $(B)/Escapement_UART.o $(B)/UARTEchoPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/Escapement_UART.o $(B)/UARTEchoPico.o -T$(LINKERFILE) -lgcc
$(B)/UARTSendersPico.elf : $(COMMON) $(B)/Escapement_UART.o $(B)/UARTSendersPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/Escapement_UART.o $(B)/UARTSendersPico.o -T$(LINKERFILE) -lgcc
$(B)/TaskWrapPico.elf : $(COMMON) $(B)/TaskWrapPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/TaskWrapPico.o -T$(LINKERFILE) -lgcc
$(B)/IPCPico.elf : $(COMMON) $(B)/IPCPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/IPCPico.o -T$(LINKERFILE) -lgcc
$(B)/SoakPico.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_TimerEvent.o $(B)/SoakPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_TimerEvent.o $(B)/SoakPico.o -T$(LINKERFILE) -lgcc
$(B)/SoakFirmPico.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/Escapement_TimerEvent.o $(B)/SoakFirmPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/StackGuardPico.elf : $(COMMON) $(B)/StackGuardPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/StackGuardPico.o -T$(LINKERFILE) -lgcc
$(B)/TestTimerEventPico.elf : $(COMMON) $(SLEEPGATE) $(B)/Escapement_TimerEvent.o $(B)/TestTimerEventPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(SLEEPGATE) $(B)/Escapement_TimerEvent.o $(B)/TestTimerEventPico.o -T$(LINKERFILE) -lgcc
$(B)/FourSlotCoresPico.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/FourSlotCoresPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/FourSlotCoresPico.o -T$(LINKERFILE) -lgcc

# make KERNEL=PA bench: times the changes of operating point of the DVFS driver, which
# only the power-aware kernel builds (BenchDVFSPico.c), and the settling of the core
# regulator (BenchVregPico.c, which runs the chip below its specified voltage).
# make KERNEL=PA sleep: SleepPico, the RP2040's sleeps for the PPK2 (SleepPico.c); with
# RUN=1 its operating points instead, UNDERVOLT=1 below the specified voltage, and
# PHASE_PERIODS=20 phases of 2 s to check them. Not built by the CI: the bench's.
sleep: $(B)/SleepPico.elf
$(B)/SleepPico.o : SleepPico.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SleepPico.elf : $(COMMON) $(B)/Escapement_Core1.o $(B)/SleepPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/Escapement_Core1.o $(B)/SleepPico.o -T$(LINKERFILE) -lgcc
bench: $(B)/BenchDVFSPico.elf $(B)/BenchVregPico.elf
$(B)/BenchDVFSPico.elf : $(COMMON) $(B)/BenchDVFSPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/BenchDVFSPico.o -T$(LINKERFILE) -lgcc
$(B)/BenchVregPico.elf : $(COMMON) $(B)/BenchVregPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) $(B)/BenchVregPico.o -T$(LINKERFILE) -lgcc

# make KERNEL=SOFT bench-admission: times the soft kernel's test of each optional instance
# under EDF (BenchAdmissionPico.c), interrupts masked while it runs and, apart, left as
# the kernel has them. The kernel is built apart
# with the hook that times it, which no other image has.
ADMISSION = $(filter-out $(B)/EscapementSoft.o,$(COMMON)) $(B)/EscapementSoftAdmission.o
bench-admission: $(B)/BenchAdmissionPico.elf $(B)/BenchAdmissionUnmaskedPico.elf
$(B)/EscapementSoftAdmission.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) -DESCAPEMENT_MEASURE_ADMISSION_COST $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/BenchAdmissionPico.o : BenchAdmissionPico.c
	$(CC) $(CFLAGS) -DESCAPEMENT_MEASURE_ADMISSION_COST $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/BenchAdmissionUnmaskedPico.o : BenchAdmissionPico.c
	$(CC) $(CFLAGS) -DESCAPEMENT_MEASURE_ADMISSION_COST -DUNMASKED $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/BenchAdmissionUnmaskedPico.elf : $(ADMISSION) $(B)/BenchAdmissionUnmaskedPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(ADMISSION) $(B)/BenchAdmissionUnmaskedPico.o -T$(LINKERFILE) -lgcc
$(B)/BenchAdmissionPico.elf : $(ADMISSION) $(B)/BenchAdmissionPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(ADMISSION) $(B)/BenchAdmissionPico.o -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard $(B)/*.d)
