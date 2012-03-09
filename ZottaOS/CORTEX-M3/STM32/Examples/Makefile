# Copyright (c) 2006-2012 MIS Institute of the HEIG affiliated to the University of
# Applied Sciences of Western Switzerland. All rights reserved.
# IN NO EVENT SHALL THE MIS INSTITUTE NOR THE HEIG NOR THE UNIVERSITY OF APPLIED
# SCIENCES OF WESTERN SWITZERLAND BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
# INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
# DOCUMENTATION, EVEN IF THE MIS INSTITUTE OR THE HEIG OR THE UNIVERSITY OF APPLIED
# SCIENCES OF WESTERN SWITZERLAND HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# THE MIS INSTITUTE, THE HEIG AND THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWIT-
# ZERLAND SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFT-
# WARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE MIS INSTITUTE NOR THE HEIG
# AND NOR THE UNIVERSITY OF APPLIED SCIENCES OF WESTERN SWITZERLAND HAVE NO OBLIGATION
# TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

# Version identifier: January 2012
# Compiler and linker: 
# Authors: MIS-TIC 

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump

LINKERFILE = STM32F103RC_Flash.ld

OBJECTS = build/ZottaOS_UART.o \
          build/ZottaOS_TimerEvent.o \
          build/ZottaOS_Interrupts.o \
          build/ZottaOS_Timer.o \
          build/ZottaOS_CortexM3.o \
          build/ZottaOS_CortexM3_a.o \
          build/ZottaOSHard.o \
          build/ZottaOSSoft.o \
          build/system_stm32f10x.o \
          build/stm32f10x_flash.o \
          build/stm32f10x_rcc.o \
          build/stm32f10x_gpio.o \
          build/stm32f10x_usart.o \
          build/stm32f10x_dbgmcu.o \
          build/stm32f10x_tim.o \
          build/misc.o

INCLUDE_PATH_LIB_STM32 = -I../Libraries/STM32F10x_StdPeriph_Driver/inc -I../Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x -I../Libraries/CMSIS/CM3/CoreSupport -include stm32f10x_conf.h
INCLUDE_PATH_ZOTTAOS = -I../ -I../../ -I../../../

CFLAGS = -mcpu=${CPU} -mthumb -Wall -c -g
AFLAGS = -mcpu=$(CPU) -mthumb -Wall -c -g

CPU = cortex-m3

all: build/TaskLED.elf build/UARTSimpleEcho.elf build/TestTimerEvent.elf

clean: 
	rm -f build/*.o build/*.elf

build/ZottaOS_Interrupts.o : ../ZottaOS_Interrupts.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $<
build/ZottaOS_Timer.o : ../ZottaOS_Timer.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $< 
build/ZottaOS_UART.o : ../ZottaOS_UART.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $< 
build/ZottaOS_CortexM3.o : ../../ZottaOS_CortexM3.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $<
build/ZottaOSHard.o : ../../../ZottaOSHard.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $< 
build/ZottaOSSoft.o : ../../../ZottaOSSoft.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $< 
build/ZottaOS_TimerEvent.o : ../ZottaOS_TimerEvent.c 
	$(CC) $(CFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $<

build/ZottaOS_CortexM3_a.o : ../../ZottaOS_CortexM3_a.S
	$(CC) $(AFLAGS) $(INCLUDE_PATH_ZOTTAOS) -o $@ $< 

build/StateChart.o : StateChart.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) $(INCLUDE_PATH_ZOTTAOS) -o $@ $< 

build/system_stm32f10x.o : system_stm32f10x.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/misc.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/misc.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/stm32f10x_dbgmcu.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_dbgmcu.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/stm32f10x_flash.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_flash.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/stm32f10x_gpio.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/stm32f10x_rcc.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/stm32f10x_usart.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $<
build/stm32f10x_tim.o : ../Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_tim.c
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) -o $@ $< 

build/TaskLED.o : TaskLED.c 
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) $(INCLUDE_PATH_ZOTTAOS) -o $@ $<
build/UARTSimpleEcho.o : UARTSimpleEcho.c 
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) $(INCLUDE_PATH_ZOTTAOS) -o $@ $<
build/TestTimerEvent.o : TestTimerEvent.c 
	$(CC) $(CFLAGS) $(INCLUDE_PATH_LIB_STM32) $(INCLUDE_PATH_ZOTTAOS) -o $@ $<

build/TaskLED.elf : ${OBJECTS} build/TaskLED.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ ${OBJECTS} build/TaskLED.o -T${LINKERFILE}
build/UARTSimpleEcho.elf : ${OBJECTS} build/UARTSimpleEcho.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ ${OBJECTS} build/UARTSimpleEcho.o -T${LINKERFILE}
build/COLD_LED1.elf : ${OBJECTS} build/COLD_LED1.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ build/COLD_LED1.o ${OBJECTS} -T${LINKERFILE}
build/TestTimerEvent.elf : ${OBJECTS} build/TestTimerEvent.o
	${CC} -mcpu=${CPU} -mthumb -nostartfiles -o $@ build/TestTimerEvent.o ${OBJECTS} -T${LINKERFILE}
	