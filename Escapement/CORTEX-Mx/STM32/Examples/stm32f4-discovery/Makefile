## Copyright (c) 2026 Bertrand Hurst. All rights reserved.
## Escapement - Lightweight Power-Aware Real-Time OS.
## Derived from prior work; see LICENSE and NOTICE at the root of this repository.

# Version identifier: August 2012

FAMILLY = STM32F4xx
LINKERFILE = STM32F407VG_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = Escapement_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/inc -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Include

# Bare-metal ARM toolchain. Overridable:
#   make CROSS_COMPILE=/path/to/arm-none-eabi-
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc

CPU = cortex-m4

# Every rule writes into build/ without ever creating it.
$(shell mkdir -p build)

all: build/TaskLEDF4.elf build/UARTSimpleEchoF4.elf build/TestTimerEventF4.elf

clean: 
	rm -f build/*.o build/*.elf build/*.bin build/*.d

# Raw images, for the Renode emulation (see emulation/renode/).
OBJCOPY = $(CROSS_COMPILE)objcopy

bin: all build/TaskLEDF4.bin build/UARTSimpleEchoF4.bin build/TestTimerEventF4.bin

build/%.bin : build/%.elf
	$(OBJCOPY) -O binary $< $@

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
          
OBJECTS_LIB = build/system_stm32f4xx.o \
          build/stm32f4xx_adc.o \
          build/stm32f4xx_can.o \
          build/stm32f4xx_crc.o \
          build/stm32f4xx_cryp.o \
          build/stm32f4xx_dac.o \
          build/stm32f4xx_dbgmcu.o \
          build/stm32f4xx_dcmi.o \
          build/stm32f4xx_dma.o \
          build/stm32f4xx_exti.o \
          build/stm32f4xx_flash.o \
          build/stm32f4xx_fsmc.o \
          build/stm32f4xx_hash.o \
          build/stm32f4xx_gpio.o \
          build/stm32f4xx_i2c.o \
          build/stm32f4xx_iwdg.o \
          build/stm32f4xx_pwr.o \
          build/stm32f4xx_rcc.o \
          build/stm32f4xx_rng.o \
          build/stm32f4xx_rtc.o \
          build/stm32f4xx_sdio.o \
          build/stm32f4xx_spi.o \
          build/stm32f4xx_syscfg.o \
          build/stm32f4xx_tim.o \
          build/stm32f4xx_usart.o \
          build/stm32f4xx_wwdg.o \
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


build/system_stm32f4xx.o : system_stm32f4xx.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_adc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_adc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_can.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_can.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_crc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_crc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_cryp.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_cryp.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_dac.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_dac.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_dbgmcu.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_dbgmcu.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_dcmi.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_dcmi.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_dma.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_dma.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_exti.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_exti.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_flash.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_flash.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_fsmc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_fsmc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_hash.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_hash.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_gpio.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_gpio.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_i2c.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_i2c.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_iwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_iwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_pwr.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_pwr.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_rcc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_rcc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_rng.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_rng.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_rtc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_rtc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_sdio.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_sdio.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_spi.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_spi.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_syscfg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_syscfg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_tim.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_tim.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_usart.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_usart.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32f4xx_wwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32f4xx_wwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/misc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/misc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<


build/TaskLEDF4.o : TaskLEDF4.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/UARTSimpleEchoF4.o : UARTSimpleEchoF4.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<
build/TestTimerEventF4.o : TestTimerEventF4.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ESCAPEMENT) -o $@ $<

build/TaskLEDF4.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDF4.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TaskLEDF4.o -T${LINKERFILE} -lgcc
build/UARTSimpleEchoF4.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoF4.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/UARTSimpleEchoF4.o -T${LINKERFILE} -lgcc
build/TestTimerEventF4.elf : ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF4.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostdlib -o $@ ${OBJECTS_ESCAPEMENT} ${OBJECTS_LIB} build/TestTimerEventF4.o -T${LINKERFILE} -lgcc
	

# The .d files produced by -MMD list the headers of each object: without them, a change
# to Escapement_Config.h triggers no rebuild at all and the next test runs against a
# stale binary.
-include $(wildcard build/*.d)
