# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The NUCLEO-U575ZI-Q (STM32U575, Cortex-M33), borrowed to measure the MCU's current, which
# the UNO Q cannot show alone (docs/stm32u5.md): SleepU5 of Examples/uno-q, its outputs on
# pins of this board that are no LED (BoardU5.h, BOARD_NUCLEO_U575), built from the same
# sources and run from SRAM as there. The board on the bench has its HSE crystal, X3,
# fitted (read on 2026-10-02; UM2861, 6.7, leaves it to the variant): PLL1 runs on it as
# on the UNO Q, and the port falls back to the MSIS locked on the LSE by itself without.
# Loaded through the board's ST-LINK V3E with the reset held, as for the UNO Q:
#
#   openocd -f interface/stlink-dap.cfg -c 'transport select dapdirect_swd' \
#       -f target/stm32u5x.cfg \
#       -c 'reset_config srst_only srst_nogate connect_assert_srst' \
#       -c init -c 'reset halt' -c 'load_image build/SleepU5.elf' \
#       -c 'resume 0x20000000' -c exit
#
# SleepU5Flash.elf is SleepU5 linked into the flash (STM32U5_FLASH.ld), for the current
# measured after a power cycle with no debugger connected since:
#
#       ... -c init -c 'reset halt' -c 'program build/SleepU5Flash.elf verify reset exit'
#
# That OpenOCD, Debian's 0.12.0, sets DBG_STOP and DBG_STANDBY, which OSInitStop2 clears
# (Escapement_Stop2.c); the reset reaches the MCU only with JP2 fitted (UM2861). SleepU5
# ran on the board on 2026-10-02 (docs/stm32u5.md, "SleepU5 on the NUCLEO-U575ZI-Q"). The
# current of the MCU through JP5, the IDD jumper, taken off for the ammeter (UM2861,
# 6.4.5): not yet measured.

CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m33+nofp
LINKERFILE = ../../STM32U5_SRAM.ld
B = build
# make FLASH=1 links the same examples into the flash, in build-flash/ (STM32U5_FLASH.ld), as
# Sleep*Flash.elf already is.
ifdef FLASH
   LINKERFILE = ../../STM32U5_FLASH.ld
   B = build-flash
endif
UNOQ = ../uno-q

INCLUDE_ESCAPEMENT = -I$(UNOQ) -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -fdata-sections \
         -ffunction-sections -ffreestanding -Werror=comment -O2 -fno-strict-aliasing -MMD -MP \
         -DBOARD_NUCLEO_U575
AFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -MMD -MP
LDFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wl,--gc-sections -nostdlib

# make KERNEL=SOFT: the soft real-time kernel, and SoakFirmU5 beside the others, SoakU5
# with an (m,k)-firm task (SoakU5.c), for the soft kernel's long run on this board.
ifeq ($(KERNEL),SOFT)
   CFLAGS += -DESCAPEMENT_VERSION_SOFT
   AFLAGS += -DESCAPEMENT_VERSION_SOFT
   FIRM = $(B)/SoakFirmU5.elf
endif
# make PHASES=30: Stop 2 and Sleep in turn, D13 telling them apart (SleepU5.c).
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
# make NOHSE=1: the HSE never started, as on a board without it (Examples/uno-q/Makefile).
ifdef NOHSE
   CFLAGS += -DOS_NO_HSE
endif
# make SMPS=1: the SMPS of the STM32U575ZIT6Q rather than the LDO reset leaves
# (Escapement_Processor.c), to measure both; never for the UNO Q's U585, which has none.
ifdef SMPS
   CFLAGS += -DOS_SMPS
endif

$(shell mkdir -p $(B))

all: $(B)/SleepU5.elf $(B)/SoakU5.elf $(B)/SleepU5Flash.elf $(FIRM)

clean:
	rm -f $(B)/*.o $(B)/*.elf $(B)/*.d

OBJECTS = $(B)/Escapement_CortexMx.o $(B)/Escapement_CortexMx_a.o \
          $(B)/Escapement_Atomic.o $(B)/Escapement_Interrupts.o \
          $(B)/Escapement_Timer.o $(B)/Escapement_Processor.o $(B)/EscapementHard.o \
          $(B)/EscapementSoft.o \
          $(B)/Escapement_RamEntry.o $(B)/Escapement_LPTimer.o $(B)/Escapement_Stop2.o \
          $(B)/Escapement_UART.o $(B)/Escapement_TimerEvent.o $(B)/SleepU5.o
# Without the entry code of an image in SRAM, under make FLASH=1.
ifdef FLASH
   OBJECTS := $(filter-out $(B)/Escapement_RamEntry.o,$(OBJECTS))
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
$(B)/SleepU5.o : $(UNOQ)/SleepU5.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

$(B)/SleepU5.elf : $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc

# SleepU5 run from the flash (STM32U5_FLASH.ld), for the current measured after a power
# cycle with no debugger since: the same objects, linked at 0x08000000 without the entry
# code of an image in SRAM. Written with OpenOCD's "program ... verify reset".
$(B)/SleepU5Flash.elf : $(filter-out $(B)/Escapement_RamEntry.o,$(OBJECTS))
	$(CC) $(LDFLAGS) -o $@ $^ -T../../STM32U5_FLASH.ld -lgcc

# SoakU5, the endurance test, its reports and link on USART1 to the ST-LINK's virtual COM
# port (SoakU5.c), for a long run beside the UNO Q's checks (tools/soak.py nucleo).
$(B)/SoakU5.o : $(UNOQ)/SoakU5.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakU5.elf : $(filter-out $(B)/SleepU5.o,$(OBJECTS)) $(B)/SoakU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc
$(B)/SoakFirmU5.o : $(UNOQ)/SoakU5.c
	$(CC) $(CFLAGS) -DSOAK_FIRM $(INCLUDE_ESCAPEMENT) -o $@ $<
$(B)/SoakFirmU5.elf : $(filter-out $(B)/SleepU5.o,$(OBJECTS)) $(B)/SoakFirmU5.o
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc

-include $(wildcard $(B)/*.d)
