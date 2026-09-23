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
Scheduling survives the 2^30 wrap of the kernel clock
    [Documentation]           The kernel counts time modulo 2^30 and shifts every temporal
    ...                       variable back when its counter wraps. At the usual tick rate
    ...                       that happens once every eighteen minutes, so no test had ever
    ...                       reached it. Here TIM2 is clocked 8200 times faster and
    ...                       TaskWrapF4 scales its periods to match, which puts the
    ...                       boundary at 107 ms of emulated time for an unchanged load.
    ...                       Each task must still raise and lower its output afterwards.
    Execute Command           path add @${CURDIR}
    Execute Command           include @Escapement_STM32_Timer.cs
    Execute Command           mach create "escapement-f4-wrap"
    Execute Command           machine LoadPlatformDescription @escapement_f4_wrap.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/TaskWrapF4.elf
    Execute Command           sysbus LoadBinary @${EXAMPLE}/build/TaskWrapF4.bin 0x08000000

    ${flag1}=                 Create LED Tester  sysbus.gpioPortB.Flag1  defaultTimeout=0.25
    ${flag2}=                 Create LED Tester  sysbus.gpioPortB.Flag2  defaultTimeout=0.25
    ${flag3}=                 Create LED Tester  sysbus.gpioPortB.Flag3  defaultTimeout=0.25

    Start Emulation

    # Before the boundary: the three tasks are running normally.
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true

    # Cross it. The counter reaches 2^30 after 107 ms, so this leaves it well past.
    Execute Command           emulation RunFor "0.15"

    # After the boundary: every task must still be scheduled.
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true
