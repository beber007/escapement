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
${NUCLEO}                     ${CURDIR}/../../Escapement/CORTEX-Mx/STM32U5/Examples/nucleo-u575

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware into SRAM, as tools/unoq_load.sh does on the board,
    ...                       and starts it at its reset handler with the stack pointer of its
    ...                       vector table, which is what the entry code at the head of the
    ...                       image does there (Escapement_RamEntry.S).
    [Arguments]               ${binary}  ${platform}=escapement_u5.repl  ${example}=${EXAMPLE}
    Execute Command           mach create "uno_q"
    Execute Command           path add @${CURDIR}
    Execute Command           include @${CURDIR}/Escapement_STM32_Timer.cs
    Execute Command           machine LoadPlatformDescription @${CURDIR}/${platform}
    Execute Command           sysbus LoadELF @${example}/build/${binary}.elf
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

LPTIM1 counts the crystal of 32.768 kHz
    [Documentation]           TestLPTimerU5: LPTIM1 on the LSE against TIM2, every 250 ms for
    ...                       3 s, 32768 ticks for 1,000,000 us, and the compare it sets each
    ...                       time found reached at the next.
    Load Escapement           TestLPTimerU5
    Execute Command           emulation RunFor "3"
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${instances}=             Read Word  ${results + 4}
    ${ticks}=                 Read Word  ${results + 8}
    ${micros}=                Read Word  ${results + 12}
    ${missed}=                Read Word  ${results + 16}
    ${nolse}=                 Read Word  ${results + 20}
    Log To Console            ${instances} instances, ${ticks} ticks for ${micros} us
    Should Be True            ${instances} >= 10
    Should Be Equal As Integers  ${nolse}  0
    Should Be Equal As Integers  ${missed}  0
    Should Be True            abs(${ticks} * 1000000 - ${micros} * 32768) <= ${micros} * 32768 / 1000

The idle task sleeps in Stop 2 on LPTIM1
    [Documentation]           SleepU5: the idle task arms LPTIM1 short of each 100 ms period,
    ...                       stops TIM2, sleeps, and moves TIM2 on by what LPTIM1 counted.
    ...                       The platform never reports Stop 2 entered (PWR_SR.STOPF), so the
    ...                       clock is not restarted here, which the board checks; the
    ...                       sleeps, the wake-up and the time carried over are the port's,
    ...                       TIM5 carried with TIM2 for a timer event 40 ms into each period.
    Load Escapement           SleepU5
    Execute Command           emulation RunFor "3"
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${instances}=             Read Word  ${results + 4}
    ${ticks}=                 Read Word  ${results + 8}
    ${micros}=                Read Word  ${results + 12}
    ${jitter}=                Read Word  ${results + 16}
    ${late}=                  Read Word  ${results + 28}
    ${nolse}=                 Read Word  ${results + 32}
    ${events}=                Read Word  ${results + 36}
    ${eventoff}=              Read Word  ${results + 40}
    Log To Console            ${instances} instances, ${ticks} ticks for ${micros} us, gap off by ${jitter} us at most, ${events} events off by ${eventoff} us at most
    Should Be True            ${instances} >= 25
    Should Be Equal As Integers  ${nolse}  0
    Should Be Equal As Integers  ${late}  0
    Should Be True            ${jitter} <= 5
    Should Be True            ${events} >= ${instances}
    Should Be True            ${eventoff} <= 20
    Should Be True            abs(${ticks} * 1000000 - ${micros} * 32768) <= ${micros} * 32768 / 10000

The idle task sleeps across the 2^30 wrap of the kernel clock
    [Documentation]           SleepWrapU5, SleepU5 with its times a thousand times longer, on
    ...                       TIM2, TIM5 and LPTIM1 a thousand times faster: 2.5 s cross the
    ...                       wrap at 2^30 twice while the idle task sleeps on LPTIM1 and
    ...                       moves TIM2 and TIM5 on, in ticks of a nanosecond, its margins
    ...                       scaled with them. Every start one period after the last within
    ...                       5 us, none late, and every timer event within 20 us of its
    ...                       time, as tools/unoq_sleep.py asks of the board: the core is not
    ...                       faster, and the event task's start takes as long as there.
    Load Escapement           SleepWrapU5  escapement_u5_wrap.repl
    Execute Command           emulation RunFor "2.5"
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${instances}=             Read Word  ${results + 4}
    ${ticks}=                 Read Word  ${results + 8}
    ${micros}=                Read Word  ${results + 12}
    ${jitter}=                Read Word  ${results + 16}
    ${late}=                  Read Word  ${results + 28}
    ${nolse}=                 Read Word  ${results + 32}
    ${events}=                Read Word  ${results + 36}
    ${eventoff}=              Read Word  ${results + 40}
    ${entries}=               Read Word  ${results + 20}
    Log To Console            ${instances} instances, ${entries} into Stop 2, ${late} late, gap off by ${jitter} ns at most, ${events} events off by ${eventoff} ns at most
    Should Be True            ${instances} >= 20
    Should Be Equal As Integers  ${nolse}  0
    Should Be Equal As Integers  ${late}  0
    Should Be True            ${jitter} <= 5000
    Should Be True            ${events} >= ${instances}
    Should Be True            ${eventoff} <= 20000

The idle task sleeps up to the wrap when an arrival lies beyond it
    [Documentation]           Stop2EventWrapU5: an event-driven task of 300 ms, signalled 10 ms
    ...                       after each start, waits for the rest of its period in the arrival
    ...                       queue, its time beyond the 2^30 wrap for the period that crosses
    ...                       it, which the kernel arms TIM2's compare with. The idle task, in
    ...                       Stop 2 on LPTIM1, must sleep no further than the wrap: TIM2, the
    ...                       kernel's time, and TIM5, both moved on by what LPTIM1 counted,
    ...                       must drift apart by as much over the period across the wrap as
    ...                       over the others, within 5 us, and every start come one period
    ...                       after the last. Scaled as SleepWrapU5, in ns. Slept past the wrap,
    ...                       TIM2 lost 1.5 ms of that period (2026-09-29).
    Load Escapement           Stop2EventWrapU5  escapement_u5_wrap.repl
    Execute Command           emulation RunFor "2.5"
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${marker}=                Read Word  ${results}
    ${instances}=             Read Word  ${results + 4}
    ${gapoff}=                Read Word  ${results + 8}
    ${skewmin}=               Read Word  ${results + 12}
    ${skewmax}=               Read Word  ${results + 16}
    ${spread}=                Evaluate  ((${skewmax} - ${skewmin}) & 0xFFFFFFFF)
    ${entries}=               Read Word  ${results + 20}
    ${late}=                  Read Word  ${results + 24}
    ${nolse}=                 Read Word  ${results + 28}
    Log To Console            ${instances} instances, ${late} late, gap off by ${gapoff} ns at most, TIM5 less TIM2 over a period spread over ${spread} ns
    Should Be Equal As Integers  ${marker}  0x53574556
    Should Be True            ${instances} >= 6
    Should Be Equal As Integers  ${nolse}  0
    Should Be True            ${gapoff} <= 5000
    Should Be True            ${spread} <= 5000

Scheduling survives the 2^30 wrap of the kernel clock
    [Documentation]           The kernel counts time modulo 2^30 and shifts every temporal
    ...                       variable back when its counter wraps, which TIM2 does at 2^30.
    ...                       As in escapement_pico2.robot, the timer runs 1000 times faster
    ...                       (escapement_u5_wrap.repl) and TaskWrapU5 scales its periods to
    ...                       match, which puts the boundary at 1.07 s of emulated time for
    ...                       an unchanged load.
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
    Execute Command           python "import System; System.AppDomain.CurrentDomain.SetData('helped', 0)"
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
    ...                       PG6, the CTS of the Linux side's UART, must be an output held
    ...                       low (Escapement_UART.c).
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
    # GPIOG: MODER, then ODR.
    ${moder}=                 Read Word  0x42021800
    ${odr}=                   Read Word  0x42021814
    Should Be Equal As Integers  ${{ (${moder} >> 12) & 3 }}  1
    Should Be Equal As Integers  ${{ (${odr} >> 6) & 1 }}  0

The endurance test counts the bursts of the link
    [Documentation]           SoakU5 handed three bytes of the link at once, the emulation
    ...                       paused: read one after the other, they are a burst of three, which
    ...                       word 94 of the results keeps in its top byte, and the bytes come
    ...                       in order. Each report ends with that word: 28 numbers after SOAK,
    ...                       which tools/soak.py reads.
    [Timeout]                 10 minutes
    Load Escapement           SoakU5

    ${lpuart1}=               Create Terminal Tester  sysbus.lpuart1  defaultPauseEmulation=true
    Wait For Line On Uart     SOAK  timeout=1.5  testerId=${lpuart1}
    Execute Command           sysbus.lpuart1 WriteChar 0
    Execute Command           sysbus.lpuart1 WriteChar 1
    Execute Command           sysbus.lpuart1 WriteChar 2
    ${line}=                  Wait For Line On Uart  SOAK  timeout=1.5  testerId=${lpuart1}

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${bytes}=                 Read Word  ${results + 88 * 4}
    ${errors}=                Read Word  ${results + 89 * 4}
    ${burst}=                 Read Word  ${results + 94 * 4}
    Should Be Equal As Integers  ${bytes}  3
    Should Be Equal As Integers  ${errors}  0
    Should Be Equal As Integers  ${{ ${burst} >> 24 }}  3
    Should Be Equal As Integers  ${{ len($line.Line.split()) }}  29

The endurance test of the NUCLEO-U575ZI-Q reports on USART1
    [Documentation]           SoakU5 of Examples/nucleo-u575 (escapement_u5_nucleo.repl), whose
    ...                       reports go on USART1, the ST-LINK's virtual COM port, which
    ...                       tools/soak.py nucleo reads, rather than on LPUART1: a line
    ...                       starting SOAK each second, none on LPUART1, and every part of the
    ...                       test active and without error, as for the UNO Q.
    [Timeout]                 10 minutes
    Load Escapement           SoakU5  escapement_u5_nucleo.repl  ${NUCLEO}

    ${usart1}=                Create Terminal Tester  sysbus.usart1  defaultPauseEmulation=true
    ${lpuart1}=               Create Terminal Tester  sysbus.lpuart1  defaultPauseEmulation=true
    Wait For Line On Uart     SOAK  timeout=2.5  testerId=${usart1}
    Wait For Line On Uart     SOAK  timeout=1.5  testerId=${usart1}
    Should Not Be On Uart     SOAK  timeout=0.1  testerId=${lpuart1}

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${seconds}=               Read Word  ${results + 4}
    Should Be True            ${seconds} >= 2
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Word  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Word  ${results + 44 + 4 * ${part}}
        Log To Console        part ${part}: ${activity} done, ${errors} errors
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END

Without the HSE, PLL1 takes the MSIS within its input ranges
    [Documentation]           SoakU5 of Examples/nucleo-u575 on a platform whose HSE never starts
    ...                       (escapement_u5_nohse.repl), as the NUCLEO-U575ZI-Q ships: PLL1
    ...                       takes the MSIS in range 2, 16.0017 MHz locked on the LSE, its
    ...                       booster through a prescaler of 2 and its VCO through M = 3, both
    ...                       within their ranges, x 60 / 2; the wait states of the flash and
    ...                       of the SRAM raised before the MSIS, the SRAM's taken off again
    ...                       once in range 1; the HSE left off; and the test runs as with it. Until
    ...                       2026-09-30 PLL1 took the MSIS of range 4, 3.998 MHz, under the
    ...                       4 MHz of both. The platform does not check the clocks: this says
    ...                       what the port writes, the board what the chip does.
    [Timeout]                 10 minutes
    Load Escapement           SoakU5  escapement_u5_nohse.repl  ${NUCLEO}

    ${usart1}=                Create Terminal Tester  sysbus.usart1  defaultPauseEmulation=true
    Wait For Line On Uart     SOAK  timeout=2.5  testerId=${usart1}

    # RCC: CR, ICSCR1, PLL1CFGR, PLL1DIVR, and the wait states the platform saw as the
    # MSIS's range was set.
    ${cr}=                    Read Word  0x46020C00
    ${icscr1}=                Read Word  0x46020C08
    ${cfgr}=                  Read Word  0x46020C28
    ${divr}=                  Read Word  0x46020C34
    ${latency}=               Read Word  0x46020FF0
    ${sram_then}=             Read Word  0x46020FF4
    ${sram_now}=              Read Word  0x40026000
    Should Be Equal As Integers  ${{ (${cr} >> 16) & 1 }}  0
    Should Be Equal As Integers  ${{ (${icscr1} >> 28) & 0xF }}  2
    Should Be Equal As Integers  ${{ (${icscr1} >> 23) & 1 }}  1
    Should Be Equal As Integers  ${{ ${cfgr} & 3 }}  1
    Should Be Equal As Integers  ${{ (${cfgr} >> 2) & 3 }}  0
    Should Be Equal As Integers  ${{ (${cfgr} >> 8) & 0xF }}  2
    Should Be Equal As Integers  ${{ (${cfgr} >> 12) & 0xF }}  1
    Should Be Equal As Integers  ${{ (${divr} & 0x1FF) + 1 }}  60
    Should Be Equal As Integers  ${{ ((${divr} >> 24) & 0x7F) + 1 }}  2
    Should Be Equal As Integers  ${latency}  4
    Should Be Equal As Integers  ${sram_then}  1
    Should Be Equal As Integers  ${{ (${sram_now} >> 16) & 7 }}  0

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Word  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Word  ${results + 44 + 4 * ${part}}
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END
