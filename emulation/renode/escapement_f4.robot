# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
# The longest test takes under 30 s on the CI's runners (2026-09-26): an emulator that
# stops answering, as Renode once did there in a first test, fails in 2 minutes rather
# than holding the job until its own timeout.
Test Timeout                  2 minutes
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery

*** Keywords ***
Load Escapement
    [Arguments]               ${binary}
    Execute Command           path add @${CURDIR}
    Execute Command           include @Escapement_STM32_Timer.cs
    Execute Command           mach create "escapement-f4"
    Execute Command           machine LoadPlatformDescription @escapement_f4.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/${binary}.elf
    Execute Command           sysbus LoadBinary @${EXAMPLE}/build/${binary}.bin 0x08000000

*** Test Cases ***
The three periodic tasks are scheduled
    [Documentation]           TaskLEDF4 creates three tasks of periods 100, 200 and 600 ticks
    ...                       toggling PB13, PB14 and PB15. The timer runs at 121.95 kHz, so
    ...                       the periods are 820 us, 1.64 ms and 4.92 ms. Each task must
    ...                       therefore raise and then lower its output within its window.
    Load Escapement           TaskLEDF4

    # A task can raise and lower its output inside the same emulated
    # microsecond — at -O2 most instances of the longest task do — so the
    # tester has to be given several periods to catch a pulse it can see.
    ${flag1}=                 Create LED Tester  sysbus.gpioPortB.Flag1  defaultTimeout=0.25
    ${flag2}=                 Create LED Tester  sysbus.gpioPortB.Flag2  defaultTimeout=0.25
    ${flag3}=                 Create LED Tester  sysbus.gpioPortB.Flag3  defaultTimeout=0.25

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

The UART echo answers
    [Documentation]           UARTSimpleEchoF4 sends every character received on USART2 back
    ...                       from its receive interrupt, through the transmit queue of
    ...                       Escapement_UART.c. No task runs: this proves the dispatch of
    ...                       peripheral interrupts (_OSIOHandler) and the UART driver.
    Load Escapement           UARTSimpleEchoF4

    ${uart}=                  Create Terminal Tester  sysbus.usart2  defaultPauseEmulation=true

    Start Emulation

    Write Line To Uart        escapement  waitForEcho=false
    # The example echoes the characters as they are, without adding a line ending.
    Wait For Prompt On Uart   escapement  testerId=${uart}
