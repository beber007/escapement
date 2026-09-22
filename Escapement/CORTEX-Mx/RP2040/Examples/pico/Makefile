## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

CPU = cortex-m0plus
LINKERFILE = ../../RP2040_SRAM.ld

# The firmware runs from SRAM: no second stage bootloader to write.
INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -fdata-sections -ffunction-sections \
         -ffreestanding -Werror=comment -O2 -MMD -MP
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -MMD -MP

# Build variants, which the CI also runs:
#   make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING    the other scheduling algorithm
#   make KERNEL=SOFT                                the soft real-time kernel
#   make KERNEL=PA                                  the power-aware kernel
#   make KERNEL=PA UNDERVOLT=1                      the same, below the specified voltage
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

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

all: build/TaskLEDPico.elf build/UARTEchoPico.elf build/TaskWrapPico.elf \
     build/TestTimerEventPico.elf

clean:
	rm -f build/*.o build/*.elf build/*.bin build/*.d

bin: all build/TaskLEDPico.bin build/UARTEchoPico.bin build/TaskWrapPico.bin \
     build/TestTimerEventPico.bin

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

build/TaskLEDPico.elf : $(COMMON) build/TaskLEDPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/TaskLEDPico.o -T$(LINKERFILE) -lgcc
build/UARTEchoPico.elf : $(COMMON) build/Escapement_UART.o build/UARTEchoPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/Escapement_UART.o build/UARTEchoPico.o -T$(LINKERFILE) -lgcc
build/TaskWrapPico.elf : $(COMMON) build/TaskWrapPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/TaskWrapPico.o -T$(LINKERFILE) -lgcc
build/TestTimerEventPico.elf : $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventPico.o
	$(CC) -mcpu=$(CPU) -Wl,--gc-sections -mthumb -nostdlib -o $@ $(COMMON) build/Escapement_TimerEvent.o build/TestTimerEventPico.o -T$(LINKERFILE) -lgcc

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
