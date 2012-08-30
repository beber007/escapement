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

# Version identifier: August 2012
# Authors: MIS-TIC 

FAMILLY = STM32L1xx
LINKERFILE = STM32L152RB_Flash.ld

FAMILLY_LC = `echo $(FAMILLY) | tr A-Z a-z`

DEF_HEADER_FILE = $(FAMILLY_LC).h
CONF_HEADER_FILE = ZottaOS_$(FAMILLY_LC).h

PATH_LIB_ST = ../../Libraries

INCLUDE_LIB_ST = -I. -I${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/inc -I${PATH_LIB_ST}/CMSIS/Device/${FAMILLY} -I${PATH_LIB_ST}/CMSIS/Include

CC = /root/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI/bin/arm-none-eabi-gcc

CPU = cortex-m3

all: build/TaskLEDL1.elf build/UARTSimpleEchoL1.elf build/TestTimerEventL1.elf build/TestTimerEventL1b.elf

clean: 
	rm -f build/*.o build/*.elf

INCLUDE_ZOTTAOS = -I. -I../../ -I../../../ -I../../../../

CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g  -fdata-sections -ffunction-sections
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g

OBJECTS_ZOTTAOS = build/ZottaOS_UART.o \
          build/ZottaOS_TimerEvent.o \
          build/ZottaOS_Interrupts.o \
          build/ZottaOS_Timer.o \
          build/ZottaOS_Atomic.o \
          build/ZottaOS_CortexMx.o \
          build/ZottaOS_CortexMx_a.o \
          build/ZottaOSHard.o \
          build/ZottaOSSoft.o \
          
OBJECTS_LIB = build/system_stm32l1xx.o \
              build/stm32l1xx_adc.o \
              build/stm32l1xx_crc.o \
              build/stm32l1xx_comp.o \
              build/stm32l1xx_dac.o \
              build/stm32l1xx_dbgmcu.o \
              build/stm32l1xx_dma.o \
              build/stm32l1xx_exti.o \
              build/stm32l1xx_flash.o \
              build/stm32l1xx_gpio.o \
              build/stm32l1xx_syscfg.o \
              build/stm32l1xx_i2c.o \
              build/stm32l1xx_iwdg.o \
              build/stm32l1xx_lcd.o \
              build/stm32l1xx_pwr.o \
              build/stm32l1xx_rcc.o \
              build/stm32l1xx_rtc.o \
              build/stm32l1xx_spi.o \
              build/stm32l1xx_tim.o \
              build/stm32l1xx_usart.o \
              build/stm32l1xx_wwdg.o \
              build/misc.o

build/ZottaOS_Atomic.o : ../../../ZottaOS_Atomic.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOS_Interrupts.o : ../../ZottaOS_Interrupts.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOS_Timer.o : ../../ZottaOS_Timer.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOS_UART.o : ../../ZottaOS_UART.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOS_CortexMx.o : ../../../ZottaOS_CortexMx.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOSHard.o : ../../../../ZottaOSHard.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOSSoft.o : ../../../../ZottaOSSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 
build/ZottaOS_TimerEvent.o : ../../ZottaOS_TimerEvent.c 
	$(CC) $(CFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $<
build/ZottaOS_CortexMx_a.o : ../../../ZottaOS_CortexMx_a.S
	$(CC) $(AFLAGS) $(INCLUDE_ZOTTAOS) -o $@ $< 


build/system_stm32l1xx.o : system_stm32l1xx.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_adc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_adc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_crc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_crc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_comp.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_comp.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_dac.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_dac.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_dbgmcu.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_dbgmcu.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_dma.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_dma.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_exti.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_exti.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_flash.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_flash.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_gpio.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_gpio.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_syscfg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_syscfg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_i2c.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_i2c.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_iwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_iwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_lcd.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_lcd.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_pwr.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_pwr.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_rcc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_rcc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_rtc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_rtc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_spi.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_spi.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_tim.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_tim.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_usart.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_usart.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/stm32l1xx_wwdg.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/stm32l1xx_wwdg.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<
build/misc.o : ${PATH_LIB_ST}/${FAMILLY}_StdPeriph_Driver/src/misc.c
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) -o $@ $<


build/TaskLEDL1.o : TaskLEDL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<
build/UARTSimpleEchoL1.o : UARTSimpleEchoL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<
build/TestTimerEventL1.o : TestTimerEventL1.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<
build/TestTimerEventL1b.o : TestTimerEventL1b.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<
build/IccMeasure.o : IccMeasure.c 
	$(CC) $(CFLAGS) $(INCLUDE_LIB_ST) $(INCLUDE_ZOTTAOS) -o $@ $<

build/TaskLEDL1.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TaskLEDL1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TaskLEDL1.o -T${LINKERFILE}
build/UARTSimpleEchoL1.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/UARTSimpleEchoL1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/UARTSimpleEchoL1.o -T${LINKERFILE}
build/TestTimerEventL1.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TestTimerEventL1.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections-mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TestTimerEventL1.o -T${LINKERFILE}
build/TestTimerEventL1b.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TestTimerEventL1b.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/TestTimerEventL1b.o -T${LINKERFILE}
build/IccMeasure.elf : ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/IccMeasure.o
	${CC} -mcpu=${CPU} -Wl,--gc-sections -mthumb -nostartfiles -o $@ ${OBJECTS_ZOTTAOS} ${OBJECTS_LIB} build/IccMeasure.o -T${LINKERFILE}
	