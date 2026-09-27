# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The NUCLEO-U575ZI-Q (STM32U575, Cortex-M33), borrowed to measure the MCU's current, which
# the UNO Q cannot show alone (docs/stm32u5.md): SleepU5 of Examples/uno-q, its outputs on
# pins of this board that are no LED (BoardU5.h, BOARD_NUCLEO_U575), built from the same
# sources and run from SRAM as there. The board has no HSE fitted as shipped (UM2861,
# 6.7): PLL1 takes the MSIS locked on the LSE, which the port falls back to by itself.
# Loaded through the board's ST-LINK V3E with the reset held, as for the UNO Q:
#
#   openocd -f interface/stlink-dap.cfg -c 'transport select dapdirect_swd' \
#       -f target/stm32u5x.cfg \
#       -c 'reset_config srst_only srst_nogate connect_assert_srst' \
#       -c init -c 'reset halt' -c 'load_image build/SleepU5.elf' \
#       -c 'resume 0x20000000' -c exit
#
# The current of the MCU through JP5, the IDD jumper, taken off for the ammeter (UM2861,
# 6.4.5). Not yet tried on the board.

CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m33+nofp
LINKERFILE = ../../STM32U5_SRAM.ld
UNOQ = ../uno-q

INCLUDE_ESCAPEMENT = -I$(UNOQ) -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -fdata-sections \
         -ffunction-sections -ffreestanding -Werror=comment -O2 -fno-strict-aliasing -MMD -MP \
         -DBOARD_NUCLEO_U575
AFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wall -c -g -MMD -MP
LDFLAGS = -mcpu=$(CPU) -mthumb -mfloat-abi=soft -Wl,--gc-sections -nostdlib

# make PHASES=30: Stop 2 and Sleep in turn, D13 telling them apart (SleepU5.c).
ifdef PHASES
   CFLAGS += -DSLEEP_PHASES=$(PHASES)
endif
# make SMPS=1: the SMPS of the STM32U575ZIT6Q rather than the LDO reset leaves
# (Escapement_Processor.c), to measure both; never for the UNO Q's U585, which has none.
ifdef SMPS
   CFLAGS += -DOS_SMPS
endif

$(shell mkdir -p build)

all: build/SleepU5.elf

clean:
	rm -f build/*.o build/*.elf build/*.d

OBJECTS = build/Escapement_CortexMx.o build/Escapement_CortexMx_a.o \
          build/Escapement_Atomic.o build/Escapement_Interrupts.o \
          build/Escapement_Timer.o build/Escapement_Processor.o build/EscapementHard.o \
          build/Escapement_RamEntry.o build/Escapement_LPTimer.o build/Escapement_Stop2.o \
          build/Escapement_UART.o build/Escapement_TimerEvent.o build/SleepU5.o

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
build/SleepU5.o : $(UNOQ)/SleepU5.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/SleepU5.elf : $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -T$(LINKERFILE) -lgcc

-include $(wildcard build/*.d)
