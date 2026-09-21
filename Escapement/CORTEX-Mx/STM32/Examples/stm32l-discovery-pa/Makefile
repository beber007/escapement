## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Version identifier: August 2012

FAMILLY = STM32L1xx
LINKERFILE = ../stm32l-discovery/STM32L152RB_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = Escapement_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I../stm32l-discovery -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Core/Include -DSTM32L152xB -DUSER_VECT_TAB_ADDRESS

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m3

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

all: build/TaskLEDPA.elf build/IccMeasure.elf

clean: 
	rm -f build/*.o build/*.elf build/*.bin build/*.d

# Raw image, for the Renode emulation (see emulation/renode/).
OBJCOPY = $(CROSS_COMPILE)objcopy

bin: all build/TaskLEDPA.bin

build/%.bin : build/%.elf
	$(OBJCOPY) -O binary $< $@

INCLUDE_ESCAPEMENT = -I. -I../stm32l-discovery -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g -fdata-sections -ffunction-sections -ffreestanding -Werror=comment -O2 -MMD -MP
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -MMD -MP

# Build variants, which the CI also runs:
#   make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING    the other scheduling algorithm
ifdef SCHEDULER
   CFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
   AFLAGS += -DSCHEDULER_REAL_TIME_MODE=$(SCHEDULER)
endif

OBJECTS_ESCAPEMENT = build/Escapement_UART.o \
          build/Escapement_TimerEvent.o \
          build/Escapement_Interrupts.o \
          build/Escapement_Timer.o \
          build/Escapement_Atomic.o \
          build/Escapement_CortexMx.o \
          build/Escapement_CortexMx_a.o \
          build/EscapementHardPA.o \
          build/Escapement_Processor.o \
          
OBJECTS_LIB = build/system_stm32l1xx.o

build/Escapement_Atomic.o : ../../../Escapement_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Interrupts.o : ../../Escapement_Interrupts.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Timer.o : ../../Escapement_Timer.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 
build/Escapement_UART.o : ../../Escapement_UART.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 
build/Escapement_CortexMx.o : ../../../Escapement_CortexMx.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/EscapementHardPA.o : ../../../../EscapementHardPA.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_Processor.o : ../../Escapement_Processor.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 
build/Escapement_TimerEvent.o : ../../Escapement_TimerEvent.c 
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_CortexMx_a.o : ../../../Escapement_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 


build/system_stm32l1xx.o : ../stm32l-discovery/system_stm32l1xx.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<


build/TaskLEDPA.o : ../stm32l-discovery/TaskLEDL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
# IccMeasure changes the processor speed, which only the power-aware kernel provides.
build/IccMeasure.o : ../stm32l-discovery/IccMeasure.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDPA.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDPA.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDPA.o -T${LINKERFILE} -lgcc
build/IccMeasure.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/IccMeasure.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/IccMeasure.o -T${LINKERFILE} -lgcc
	

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
