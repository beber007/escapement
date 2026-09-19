## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Version identifier: August 2012

FAMILLY = STM32F0xx
LINKERFILE = STM32F051R8_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = Escapement_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/inc -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Include

# Toolchain bare-metal ARM. Surchargeable :
#   make CROSS_COMPILE=/chemin/vers/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m0

# Les règles écrivent toutes dans build/ sans jamais le créer.
$(shell mkdir -p build)

all: build/TaskLEDF0.elf build/UARTSimpleEchoF0.elf build/TestTimerEventF0.elf build/TestTimerEventF0b.elf

clean: 
	rm -f build/*.o build/*.elf

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

# -ffreestanding : pas de bibliotheque C hote, GCC fournit ses propres
# stdint.h et stddef.h.
CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g -fdata-sections -ffunction-sections -ffreestanding
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g

OBJECTS_ESCAPEMENT = build/Escapement_UART.o \
          build/Escapement_TimerEvent.o \
          build/Escapement_Interrupts.o \
          build/Escapement_Timer.o \
          build/Escapement_Atomic.o \
          build/Escapement_CortexMx.o \
          build/Escapement_CortexMx_a.o \
          build/EscapementHard.o \
          build/EscapementSoft.o \
          
OBJECTS_LIB = build/system_stm32f0xx.o \
              build/stm32f0xx_adc.o \
              build/stm32f0xx_cec.o \
              build/stm32f0xx_crc.o \
              build/stm32f0xx_comp.o \
              build/stm32f0xx_dac.o \
              build/stm32f0xx_dbgmcu.o \
              build/stm32f0xx_dma.o \
              build/stm32f0xx_exti.o \
              build/stm32f0xx_flash.o \
              build/stm32f0xx_gpio.o \
              build/stm32f0xx_syscfg.o \
              build/stm32f0xx_i2c.o \
              build/stm32f0xx_iwdg.o \
              build/stm32f0xx_pwr.o \
              build/stm32f0xx_rcc.o \
              build/stm32f0xx_rtc.o \
              build/stm32f0xx_spi.o \
              build/stm32f0xx_tim.o \
              build/stm32f0xx_usart.o \
              build/stm32f0xx_wwdg.o \
              build/stm32f0xx_misc.o

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


build/system_stm32f0xx.o : system_stm32f0xx.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_adc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_adc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_cec.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_cec.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_crc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_crc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_comp.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_comp.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_dac.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_dac.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_dbgmcu.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_dbgmcu.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_dma.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_dma.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_exti.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_exti.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_flash.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_flash.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_gpio.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_gpio.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_syscfg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_syscfg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_i2c.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_i2c.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_iwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_iwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_pwr.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_pwr.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_rcc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_rcc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_rtc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_rtc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_spi.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_spi.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_tim.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_tim.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_usart.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_usart.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_wwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_wwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f0xx_misc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f0xx_misc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/misc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/misc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<


build/TaskLEDF0.o : TaskLEDF0.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/UARTSimpleEchoF0.o : UARTSimpleEchoF0.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventF0.o : TestTimerEventF0.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventF0b.o : TestTimerEventF0b.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDF0.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDF0.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDF0.o -T${LINKERFILE} -lgcc
build/UARTSimpleEchoF0.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoF0.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoF0.o -T${LINKERFILE} -lgcc
build/TestTimerEventF0.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF0.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF0.o -T${LINKERFILE} -lgcc
build/TestTimerEventF0b.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF0b.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF0b.o -T${LINKERFILE} -lgcc
	