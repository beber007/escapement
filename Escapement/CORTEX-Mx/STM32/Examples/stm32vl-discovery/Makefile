## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Version identifier: August 2012

FAMILLY = STM32F10x
LINKERFILE = STM32F103RC_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = Escapement_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/inc -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Include

# Toolchain bare-metal ARM. Surchargeable :
#   make CROSS_COMPILE=/chemin/vers/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m3

# Les règles écrivent toutes dans build/ sans jamais le créer.
$(shell mkdir -p build)

all: build/TaskLEDF1.elf build/UARTSimpleEchoF1.elf build/TestTimerEventF1.elf

clean: 
	rm -f build/*.o build/*.elf build/*.d

INCLUDE_ESCAPEMENT = -I. -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g -fdata-sections -ffunction-sections -ffreestanding -Werror=comment -MMD -MP
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
          
OBJECTS_LIB = build/system_stm32f10x.o \
          build/stm32f10x_adc.o \
          build/stm32f10x_bkp.o \
          build/stm32f10x_can.o \
          build/stm32f10x_cec.o \
          build/stm32f10x_crc.o \
          build/stm32f10x_dac.o \
          build/stm32f10x_dbgmcu.o \
          build/stm32f10x_dma.o \
          build/stm32f10x_exti.o \
          build/stm32f10x_flash.o \
          build/stm32f10x_fsmc.o \
          build/stm32f10x_gpio.o \
          build/stm32f10x_i2c.o \
          build/stm32f10x_iwdg.o \
          build/stm32f10x_pwr.o \
          build/stm32f10x_rcc.o \
          build/stm32f10x_rtc.o \
          build/stm32f10x_sdio.o \
          build/stm32f10x_spi.o \
          build/stm32f10x_tim.o \
          build/stm32f10x_usart.o \
          build/stm32f10x_wwdg.o \
          build/misc.o

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


build/system_stm32f10x.o : ${PATH_LIB_ST}/CMSIS/Device/${FAMILLY}/system_stm32f10x.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_adc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_adc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_bkp.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_bkp.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_can.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_can.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_cec.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_cec.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_crc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_crc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_dac.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_dac.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_dbgmcu.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_dbgmcu.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_dma.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_dma.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_exti.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_exti.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_flash.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_flash.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_fsmc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_fsmc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_gpio.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_gpio.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_i2c.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_i2c.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_iwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_iwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_pwr.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_pwr.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_rcc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_rcc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_rtc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_rtc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_sdio.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_sdio.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_spi.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_spi.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_tim.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_tim.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_usart.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_usart.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f10x_wwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f10x_wwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/misc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/misc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<


build/TaskLEDF1.o : TaskLEDF1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/UARTSimpleEchoF1.o : UARTSimpleEchoF1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventF1.o : TestTimerEventF1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDF1.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDF1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDF1.o -T${LINKERFILE} -lgcc
build/UARTSimpleEchoF1.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoF1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoF1.o -T${LINKERFILE} -lgcc
build/TestTimerEventF1.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF1.o -T${LINKERFILE} -lgcc
	

# Les .d produits par -MMD listent les en-tetes de chaque objet : sans eux, une
# modification de Escapement_Config.h ne declenche aucune recompilation et les essais
# portent sur un binaire perime.
-include $(wildcard build/*.d)
