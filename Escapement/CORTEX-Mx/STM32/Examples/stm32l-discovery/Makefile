## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Version identifier: August 2012

FAMILLY = STM32L1xx
LINKERFILE = STM32L152RB_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = Escapement_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Include

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m3

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

all: build/TaskLEDL1.elf build/UARTSimpleEchoL1.elf build/TestTimerEventL1.elf build/TestTimerEventL1b.elf

clean: 
	rm -f build/*.o build/*.elf build/*.d

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g -fdata-sections -ffunction-sections -ffreestanding -Werror=comment -O2 -MMD -MP
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g -MMD -MP

OBJECTS_ESCAPEMENT = build/Escapement_UART.o \
          build/Escapement_TimerEvent.o \
          build/Escapement_Interrupts.o \
          build/Escapement_Timer.o \
          build/Escapement_Atomic.o \
          build/Escapement_CortexMx.o \
          build/Escapement_CortexMx_a.o \
          build/EscapementHard.o \
          build/EscapementSoft.o \
          
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
build/EscapementHard.o : ../../../../EscapementHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 
build/EscapementSoft.o : ../../../../EscapementSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 
build/Escapement_TimerEvent.o : ../../Escapement_TimerEvent.c 
	$(CC) $(CFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/Escapement_CortexMx_a.o : ../../../Escapement_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ESCAPEMENT) -o $@ $< 


build/system_stm32l1xx.o : system_stm32l1xx.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<


build/TaskLEDL1.o : TaskLEDL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/UARTSimpleEchoL1.o : UARTSimpleEchoL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventL1.o : TestTimerEventL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventL1b.o : TestTimerEventL1b.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/IccMeasure.o : IccMeasure.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDL1.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDL1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDL1.o -T${LINKERFILE} -lgcc
build/UARTSimpleEchoL1.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoL1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoL1.o -T${LINKERFILE} -lgcc
build/TestTimerEventL1.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventL1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventL1.o -T${LINKERFILE} -lgcc
build/TestTimerEventL1b.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventL1b.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventL1b.o -T${LINKERFILE} -lgcc
build/IccMeasure.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/IccMeasure.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/IccMeasure.o -T${LINKERFILE} -lgcc
	

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
