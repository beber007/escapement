# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery

*** Test Cases ***
Timer events wake the event-driven tasks
    [Documentation]           TestTimerEventF4 pairs each periodic task with an event-driven
    ...                       one. SetLed1Task raises PB13 every 5000 kernel ticks and asks
    ...                       the event handler on TIM14 to wake ClearLed1Task 1000 event
    ...                       ticks later, which lowers it; SetLed2Task and ClearLed2Task do
    ...                       the same on PB14 with 10000 and 2000 ticks. Under Renode TIM2,
    ...                       the kernel clock, counts at 10 MHz / 82 = 121.95 kHz and TIM14
    ...                       at 10 MHz / 84 = 119.05 kHz, so PB13 must stay high 8.40 ms out
    ...                       of every 41.00 ms, and PB14 16.80 ms out of every 82.00 ms. The
    ...                       period checks the kernel timer, the high time the event timer,
    ...                       and both that each event-driven task ran when it was woken.
    Execute Command           path add @${CURDIR}
    Execute Command           include @Escapement_STM32_Timer.cs
    Execute Command           mach create "escapement-f4-events"
    Execute Command           machine LoadPlatformDescription @escapement_f4.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/TestTimerEventF4.elf
    Execute Command           sysbus LoadBinary @${EXAMPLE}/build/TestTimerEventF4.bin 0x08000000

    ${flag1}=                 Create LED Tester  sysbus.gpioPortB.Flag1
    ${flag2}=                 Create LED Tester  sysbus.gpioPortB.Flag2

    Start Emulation

    Assert LED Is Blinking    testDuration=0.41  onDuration=0.0084  offDuration=0.0326  tolerance=0.02  testerId=${flag1}  pauseEmulation=true
    Assert LED Is Blinking    testDuration=0.41  onDuration=0.0168  offDuration=0.0652  tolerance=0.02  testerId=${flag2}  pauseEmulation=true
