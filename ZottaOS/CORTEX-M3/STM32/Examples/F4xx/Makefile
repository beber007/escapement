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

# Version identifier: March 2012
# Authors: MIS-TIC 

FAMILLY = STM32F4xx
LINKERFILE = STM32F407VG_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = ZottaOS_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/inc -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Include

CC = /root/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI/bin/arm-none-eabi-gcc

CPU = cortex-m4

all: build/TaskLEDF4.elf build/UARTSimpleEchoF4.elf build/TestTimerEventF4.elf

clean: 
	rm -f build/*.o build/*.elf

INCLUDE_ZOTTAOS = -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g

OBJECTS_ZOTTAOS = build/ZottaOS_UART.o \
          build/ZottaOS_TimerEvent.o \
          build/ZottaOS_Interrupts.o \
          build/ZottaOS_Timer.o \
          build/ZottaOS_Atomic.o \
          build/ZottaOS_CortexM3.o \
          build/ZottaOS_CortexM3_a.o \
          build/ZottaOSHard.o \
          build/ZottaOSSoft.o \
          
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

build/ZottaOS_Atomic.o : ../../../ZottaOS_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOS_Interrupts.o : ../../ZottaOS_Interrupts.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOS_Timer.o : ../../ZottaOS_Timer.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOS_UART.o : ../../ZottaOS_UART.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOS_CortexM3.o : ../../../ZottaOS_CortexM3.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOSHard.o : ../../../../ZottaOSHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOSSoft.o : ../../../../ZottaOSSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOS_TimerEvent.o : ../../ZottaOS_TimerEvent.c 
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOS_CortexM3_a.o : ../../../ZottaOS_CortexM3_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 


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
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<
build/UARTSimpleEchoF4.o : UARTSimpleEchoF4.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<
build/TestTimerEventF4.o : TestTimerEventF4.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<

build/TaskLEDF4.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TaskLEDF4.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TaskLEDF4.o -T${LINKERFILE}
build/UARTSimpleEchoF4.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/UARTSimpleEchoF4.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/UARTSimpleEchoF4.o -T${LINKERFILE}
build/TestTimerEventF4.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TestTimerEventF4.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TestTimerEventF4.o -T${LINKERFILE}
	