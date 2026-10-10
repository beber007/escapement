# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The NUCLEO-U385RG-Q (STM32U385, Cortex-M33): the examples of the STM32U5 port,
# transposed, run from SRAM (STM32U3_SRAM.ld) and loaded by the debugger of the board's
# ST-LINK, which leaves its flash as it was. Built for a Cortex-M33 without its
# floating-point unit, so that the compiler can never use it: the generic layer saves the
# context of a Cortex-M33 as that of a Cortex-M4 without one (Escapement_CortexMx_a.S).
# Written before the board came, and run under Renode (emulation/renode/escapement_u3.robot)
# and, since 2026-10-09, on the board (docs/stm32u3.md).

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

CPU = cortex-m33+nofp
LINKERFILE = ../../STM32U3_SRAM.ld
B = build
# make FLASH=1 links the same examples into the flash, in build-flash/ (STM32U3_FLASH.ld), as
# Sleep*Flash.elf already is.
ifdef FLASH
   LINKERFILE = ../../STM32U3_FLASH.ld
   B = build-flash
endif

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
# make PHASES=30, SleepU3 alternating 30 s in Stop 2 and 30 s in Sleep (SleepU3.c).
# The power-aware kernel is not ported yet.
ifeq ($(KERNEL),PA)
   $(error the power-aware kernel is not ported to the STM32U3 yet)
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
# event rather than OS_STOP2_WAKE_US (SleepU3.c).
ifdef WAKE
   CFLAGS += -DSLEEP_WAKE_B=$(WAKE)u
endif
# make DVFS_PHASE_S=n: DVFSU3Flash n seconds at each point rather than 20 (DVFSU3.c).
ifdef DVFS_PHASE_S
   CFLAGS += -DDVFS_PHASE_S=$(DVFS_PHASE_S)u
endif
# make PHASES=30 RUN=80000: the second phase computes for 80,000 us of each instance, the
# current of the core running (SleepU3.c).
ifdef RUN
   CFLAGS += -DSLEEP_RUN_B=$(RUN)u
endif
# make FAST=1: the MSI's PLL mode kept through Stop 2 (MSIPLL0FAST), the chip waking
# locked, at the cost of the MSI's current in Stop 2 (Escapement_Processor.c).
ifdef FAST
   CFLAGS += -DOS_MSIPLL_FAST
endif
# make MHZ=48: the system clock at 48 MHz rather than 96; 24 and 12 too, each in the
# lowest voltage range that runs it (Escapement_Processor.c).
ifdef MHZ
   CFLAGS += -DOS_SYSTEM_CLOCK_HZ=$(MHZ)000000u
endif

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p $(B))

EXAMPLES = TaskLEDU3 UARTEchoU3 TestTimerEventU3 TaskWrapU3 IPCU3 SoakU3 StackGuardU3 \
           TestLPTimerU3 SleepU3 SleepFastU3 SleepWrapU3 Stop2EventWrapU3 SoakStop2U3 ClockU3 \
           SleepU3Flash DVFSU3Flash DVFSU3Short

# The soft kernel adds SoakFirmU3, SoakU3 with an (m,k)-firm task whose optional instances
# the kernel drops under load (SoakU3.c).
ifeq ($(KERNEL),SOFT)
   EXAMPLES += SoakFirmU3
endif

all: $(EXAMPLES:%=$(B)/%.elf)

clean:
	rm -f $(B)/*.o $(B)/*.elf $(B)/*.bin $(B)/*.d

bin: all $(EXAMPLES:%=$(B)/%.bin)

$(B)/%.bin : $(B)/%.elf
	$(OBJCOPY) -O binary $< $@

COMMON = $(B)/Escapement_CortexMx.o \
          $(B)/Escapement_CortexMx_a.o \
          $(B)/Escapement_Atomic.o \
          $(B)/Escapement_Interrupts.o \
          $(B)/Escapement_Timer.o \
          $(B)/Escapement_Processor.o \
          $(B)/EscapementHard.o \
          $(B)/EscapementSoft.o \
          $(B)/EscapementHardPA.o \
          $(B)/Escapement_RamEntry.o
# Without the entry code of an image in SRAM, under make FLASH=1.
ifdef FLASH
   COMMON := $(filter-out $(B)/Escapement_RamEntry.o,$(COMMON))
endif

$(B)/Escapement_CortexMx.o : ../../../Escapement_CortexMx.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_CortexMx_a.o : ../../../Escapement_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Atomic.o : ../../../Escapement_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_RamEntry.o : ../../Escapement_RamEntry.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_%.o : ../../Escapement_%.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/%U3.o : %U3.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

$(B)/TaskLEDU3.elf : $(COMMON) $(B)/TaskLEDU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/StackGuardU3.elf : $(COMMON) $(B)/StackGuardU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/UARTEchoU3.elf : $(COMMON) $(B)/Escapement_UART.o $(B)/UARTEchoU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/TestTimerEventU3.elf : $(COMMON) $(B)/Escapement_TimerEvent.o $(B)/TestTimerEventU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/TaskWrapU3.elf : $(COMMON) $(B)/TaskWrapU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/IPCU3.elf : $(COMMON) $(B)/IPCU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/SoakU3.elf : $(COMMON) $(B)/Escapement_TimerEvent.o $(B)/Escapement_UART.o \
                   $(B)/SoakU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# SoakU3 with the idle task of Stop 2 installed, which its load never lets enter Stop 2
# (SoakU3.c).
$(B)/SoakStop2U3.o : SoakU3.c
	$(CC) $(CFLAGS) -DSOAK_STOP2 $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakStop2U3.elf : $(COMMON) $(B)/Escapement_TimerEvent.o $(B)/Escapement_UART.o \
                        $(B)/Escapement_LPTimer.o $(B)/Escapement_Stop2.o $(B)/SoakStop2U3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/SoakFirmU3.o : SoakU3.c
	$(CC) $(CFLAGS) -DSOAK_FIRM $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakFirmU3.elf : $(COMMON) $(B)/Escapement_TimerEvent.o $(B)/Escapement_UART.o \
                       $(B)/SoakFirmU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/TestLPTimerU3.elf : $(COMMON) $(B)/Escapement_LPTimer.o $(B)/TestLPTimerU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# LPUART1 at 115,200 baud: SleepU3's link wakes the chip with a byte before each frame
# (SleepU3.c); an image that does without that byte keeps 57,600 (Escapement_UART.c).
$(B)/Escapement_UARTLink.o : ../../Escapement_UART.c
	$(CC) $(CFLAGS) -DOS_LPUART1_BAUD_RATE=115200u $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SleepU3.elf : $(COMMON) $(B)/Escapement_LPTimer.o $(B)/Escapement_Stop2.o \
                    $(B)/Escapement_UARTLink.o $(B)/Escapement_TimerEvent.o $(B)/SleepU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# SleepU3 run from the flash (STM32U3_FLASH.ld), for the current measured after a power
# cycle with no debugger since: the same objects, linked at 0x08000000 without the entry
# code of an image in SRAM. Written with OpenOCD's "program ... verify reset".
$(B)/SleepU3Flash.elf : $(filter-out $(B)/Escapement_RamEntry.o,$(COMMON)) \
                         $(B)/Escapement_LPTimer.o $(B)/Escapement_Stop2.o \
                         $(B)/Escapement_UARTLink.o $(B)/Escapement_TimerEvent.o \
                         $(B)/SleepU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T../../STM32U3_FLASH.ld -lgcc
# The operating points for the PPK2, before a DVFS driver (DVFSU3.c): no kernel, the
# port's clock set-up at 12 MHz in range 2 whatever MHZ says, linked into the flash for
# the power cycle the measurement needs, as SleepU3Flash.
$(B)/Escapement_Processor12.o : ../../Escapement_Processor.c
	$(CC) $(filter-out -DOS_SYSTEM_CLOCK_HZ=%,$(CFLAGS)) -DOS_SYSTEM_CLOCK_HZ=12000000u \
	      $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/DVFSU3Flash.elf : $(filter-out $(B)/Escapement_RamEntry.o $(B)/Escapement_Processor.o,$(COMMON)) \
                        $(B)/Escapement_Processor12.o $(B)/Escapement_LPTimer.o $(B)/DVFSU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T../../STM32U3_FLASH.ld -lgcc
# The same, 1 ms a point, for escapement_u3.robot.
$(B)/DVFSU3Short.o : DVFSU3.c
	$(CC) $(CFLAGS) -DDVFS_PHASE_TICKS=33u $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/DVFSU3Short.elf : $(filter-out $(B)/Escapement_RamEntry.o $(B)/Escapement_Processor.o,$(COMMON)) \
                        $(B)/Escapement_Processor12.o $(B)/Escapement_LPTimer.o $(B)/DVFSU3Short.o
	$(CC) $(LDFLAGS) -o $@ $^ -T../../STM32U3_FLASH.ld -lgcc
# The steps of the clock set-up timed, OS_CLOCK_TIMES, for ClockU3 alone.
$(B)/Escapement_ProcessorTimed.o : ../../Escapement_Processor.c
	$(CC) $(CFLAGS) -DOS_CLOCK_TIMES $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/ClockU3.elf : $(filter-out $(B)/Escapement_Processor.o,$(COMMON)) \
                    $(B)/Escapement_ProcessorTimed.o $(B)/Escapement_UART.o $(B)/ClockU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# SleepU3 with MSIPLL0FAST, as make FAST=1 builds it, for escapement_u3.robot.
$(B)/Escapement_ProcessorFast.o : ../../Escapement_Processor.c
	$(CC) $(CFLAGS) -DOS_MSIPLL_FAST $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SleepFastU3.elf : $(filter-out $(B)/Escapement_Processor.o,$(COMMON)) \
                        $(B)/Escapement_ProcessorFast.o $(B)/Escapement_LPTimer.o \
                        $(B)/Escapement_Stop2.o $(B)/Escapement_UARTLink.o \
                        $(B)/Escapement_TimerEvent.o $(B)/SleepU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# SleepU3 with its times a thousand times longer, for the wrap under Renode (SleepU3.c),
# the margins of the idle task in Stop 2 with them; the part kept for the raise is 1.2 ms
# there, the sleep before it held under 60,000 ticks of LPTIM1, and the wait for the
# lock, in ticks of an LPTIM1 a thousand times faster, near its 16 bits.
SCALED = -DTIME_SCALE=1000 -DOS_STOP2_WAKE_US=3000000u -DOS_STOP2_MIN_US=5000000u \
         -DOS_STOP2_RAISE_US=1200000u -DOS_STOP2_LOCK_TICKS=60000u
$(B)/SleepWrapU3.o : SleepU3.c
	$(CC) $(CFLAGS) $(SCALED) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Escapement_Stop2Wrap.o : ../../Escapement_Stop2.c
	$(CC) $(CFLAGS) $(SCALED) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SleepWrapU3.elf : $(COMMON) $(B)/Escapement_LPTimer.o $(B)/Escapement_Stop2Wrap.o \
                        $(B)/Escapement_UARTLink.o $(B)/Escapement_TimerEvent.o \
                        $(B)/SleepWrapU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
# The idle task in Stop 2 while an event-driven task's arrival lies beyond the wrap, scaled
# as SleepWrapU3 for Renode (Stop2EventWrapU3.c).
$(B)/Stop2EventWrapU3.o : Stop2EventWrapU3.c
	$(CC) $(CFLAGS) $(SCALED) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/Stop2EventWrapU3.elf : $(COMMON) $(B)/Escapement_LPTimer.o $(B)/Escapement_Stop2Wrap.o \
                             $(B)/Escapement_TimerEvent.o $(B)/Stop2EventWrapU3.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard $(B)/*.d)
