# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The STM32U5 port on a platform of our own (escapement_u5.repl), the checks of
# escapement_pico2.robot that its examples allow. The platform acknowledges every clock
# switch without checking it: these tests say that the kernel runs and schedules on a
# Cortex-M33 with the timers, the USART and the GPIO of the STM32U585, not that the clocks
# are programmed right, which only the board can say.
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
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32U5/Examples/uno-q

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware into SRAM, as tools/unoq_load.sh does on the board,
    ...                       and starts it at its reset handler with the stack pointer of its
    ...                       vector table, which is what the entry code at the head of the
    ...                       image does there (Escapement_RamEntry.S).
    [Arguments]               ${binary}  ${platform}=escapement_u5.repl
    Execute Command           mach create "uno_q"
    Execute Command           path add @${CURDIR}
    Execute Command           include @${CURDIR}/Escapement_STM32_Timer.cs
    Execute Command           machine LoadPlatformDescription @${CURDIR}/${platform}
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/${binary}.elf
    ${table}=                 Execute Command  sysbus GetSymbolAddress "CortexMxVectorTable"
    Execute Command           sysbus.cpu VectorTableOffset ${table.strip()}

Read Word
    [Documentation]           One 32-bit word of the emulated memory, as an integer.
    [Arguments]               ${address}
    ${value}=                 Execute Command  sysbus ReadDoubleWord ${address}
    ${value}=                 Convert To Integer  ${value.strip()}
    RETURN                    ${value}

*** Test Cases ***
The probe task runs every millisecond
    [Documentation]           TaskLEDU5 toggles D12, PB14, from a task of period 1000 ticks of the
    ...                       1 us timer: a 500 Hz square wave, 1 ms high, 1 ms low.
    Load Escapement           TaskLEDU5

    ${probe}=                 Create LED Tester  sysbus.gpioPortB.probe

    # A virtual time run, not Start Emulation: the tester then starts at a set instant
    # of the pattern, not at one the host's speed decides (escapement_pico2.robot).
    Execute Command           emulation RunFor "0.005"

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.001  tolerance=0.02  testerId=${probe}  pauseEmulation=true

The three periodic tasks are scheduled
    [Documentation]           The three other tasks, of periods 10, 20 and 60 ms, each raise
    ...                       their output on LED3, LED4 and D13, and put it out
    ...                       before ending.
    Load Escapement           TaskLEDU5

    ${flag1}=                 Create LED Tester  sysbus.gpioPortH.flag1  defaultTimeout=0.25
    ${flag2}=                 Create LED Tester  sysbus.gpioPortH.flag2  defaultTimeout=0.25
    ${flag3}=                 Create LED Tester  sysbus.gpioPortB.flag3  defaultTimeout=0.25

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

The UART echo answers
    [Documentation]           UARTEchoU5 sends every byte received on USART1 back from its
    ...                       interrupt handler, interrupt 61, in the second word of the
    ...                       NVIC's registers.
    Load Escapement           UARTEchoU5

    ${uart}=                  Create Terminal Tester  sysbus.usart1  defaultPauseEmulation=true

    Start Emulation

    Write Line To Uart        escapement  waitForEcho=false
    Wait For Prompt On Uart   escapement  testerId=${uart}

Timer events wake the event-driven tasks
    [Documentation]           TestTimerEventU5: LED3 on 1 ms every 5 ms and LED4 on 2 ms every
    ...                       10 ms, the kernel's TIM2 timing the periods, the event manager on
    ...                       TIM5 the high times.
    Load Escapement           TestTimerEventU5

    ${flag1}=                 Create LED Tester  sysbus.gpioPortH.flag1
    ${flag2}=                 Create LED Tester  sysbus.gpioPortH.flag2

    Execute Command           emulation RunFor "0.02"

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.004  tolerance=0.02  testerId=${flag1}  pauseEmulation=true
    Assert LED Is Blinking    testDuration=0.1  onDuration=0.002  offDuration=0.008  tolerance=0.02  testerId=${flag2}  pauseEmulation=true

Scheduling survives the 2^30 wrap of the kernel clock
    [Documentation]           The kernel counts time modulo 2^30 and shifts every temporal
    ...                       variable back when its counter wraps, which TIM2 does at 2^30.
    ...                       As in escapement_pico2.robot, the timer runs 1000 times faster
    ...                       (escapement_u5_wrap.repl) and TaskWrapU5 scales its periods to
    ...                       match, which puts the
    ...                       boundary at 1.07 s of emulated time for an unchanged load.
    ...                       Every task must still run afterwards, and the probe, toggled
    ...                       every 50 ms, must keep its period within 2 %.
    Load Escapement           TaskWrapU5  escapement_u5_wrap.repl

    ${flag1}=                 Create LED Tester  sysbus.gpioPortH.flag1  defaultTimeout=1
    ${flag2}=                 Create LED Tester  sysbus.gpioPortH.flag2  defaultTimeout=1
    ${flag3}=                 Create LED Tester  sysbus.gpioPortB.flag3  defaultTimeout=1
    ${probe}=                 Create LED Tester  sysbus.gpioPortB.probe

    Start Emulation

    # Before the boundary: the three tasks are running normally.
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true

    # Cross it: 1.2 s at the 1 ns tick is past 2^30, so a counter below 2^30 now has
    # wrapped, which one ignoring its limit would not have (TIM2_CNT).
    Execute Command           emulation RunFor "1.2"
    ${count}=                 Read Word  0x40000024
    Should Be True            ${count} < 0x40000000

    # After the boundary: every task must still be scheduled, the probe on time.
    Assert LED Is Blinking    testDuration=0.5  onDuration=0.05  offDuration=0.05  tolerance=0.02  testerId=${probe}  pauseEmulation=true
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

Tasks preempt one another inside the FIFO queue and a slot buffer
    [Documentation]           IPCU5: a long task fills a FIFO queue and writes a 3-slot
    ...                       buffer, a short one of higher priority preempts it wherever it
    ...                       stands to put its own records and read the buffer once each
    ...                       time, and an event-driven task empties the queue. Every record
    ...                       must come out once and in order, the buffer never give a
    ...                       record twice, and no task find R8-R11 changed by a task that
    ...                       preempted it and ended. An operation preempted in the queue is
    ...                       completed by the task that preempts it: the hooks count the
    ...                       helpers of the queue entered for a descriptor far from the stack
    ...                       pointer, that is, on the stack of the preempted task, and some
    ...                       must be.
    Load Escapement           IPCU5
    FOR  ${helper}  ${reg}  IN  FIFOEnqueueHelper  1  FIFODequeueHelper  2
        ${address}=           Execute Command  sysbus GetSymbolAddress "${helper}"
        Execute Command       sysbus.cpu AddHook ${address.strip()} "import System; d = System.AppDomain.CurrentDomain; des = int(str(self.GetRegisterUnsafe(${reg})), 0); sp = int(str(self.GetRegisterUnsafe(13)), 0); d.SetData('helped', (d.GetData('helped') or 0) + (1 if des - sp > 64 else 0))"
    END

    Execute Command           emulation RunFor "0.05"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${put0}=                  Read Word  ${results}
    ${put1}=                  Read Word  ${results + 4}
    ${taken0}=                Read Word  ${results + 8}
    ${taken1}=                Read Word  ${results + 12}
    ${out_of_order}=          Read Word  ${results + 16}
    ${slot_reads}=            Read Word  ${results + 24}
    ${slot_repeats}=          Read Word  ${results + 28}
    ${slot_torn}=             Read Word  ${results + 32}
    ${registers}=             Read Word  ${results + 36}
    ${helped}=                Execute Command  python "import System; print(System.AppDomain.CurrentDomain.GetData('helped') or 0)"
    ${helped}=                Convert To Integer  ${helped.strip()}
    Log To Console            put ${put0}+${put1}, taken ${taken0}+${taken1}, slot reads ${slot_reads}, helped ${helped}
    Should Be True            ${taken0} > 200 and ${taken1} > 10
    Should Be True            ${put0} - ${taken0} <= 8 and ${put1} - ${taken1} <= 8
    Should Be Equal As Integers  ${out_of_order}  0
    Should Be True            ${slot_reads} > 10
    Should Be Equal As Integers  ${slot_repeats}  0
    Should Be Equal As Integers  ${slot_torn}  0
    Should Be Equal As Integers  ${registers}  0
    Should Be True            ${helped} > 0

Every part of the endurance test runs without error
    [Documentation]           SoakU5, the firmware of the endurance test, for 3.5 s: its marker
    ...                       set, its heartbeat past three seconds, every part active and none
    ...                       in error — the pulse on time, the queue in order, the buffers
    ...                       never torn nor repeated, the one the interrupt of TIM3 writes
    ...                       among them, the timer events on time, the heartbeat seeing every
    ...                       part move each second, and the stack and the guard words intact.
    ...                       The watchdog, started by the first heartbeat and reloaded by the
    ...                       others, must not have restarted the board: the counts, cleared
    ...                       at each start, would then be those of less than 3 s.
    [Timeout]                 10 minutes
    Load Escapement           SoakU5

    Execute Command           emulation RunFor "3.5"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${marker}=                Read Word  ${results}
    ${seconds}=               Read Word  ${results + 4}
    ${pulses}=                Read Word  ${results + 12}
    Should Be Equal As Integers  ${marker}  0x534F414B
    Should Be True            ${seconds} >= 3 and ${pulses} >= 3400
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Word  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Word  ${results + 44 + 4 * ${part}}
        Log To Console        part ${part}: ${activity} done, ${errors} errors
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END
