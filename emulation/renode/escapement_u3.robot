# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The STM32U3 port on a platform of our own (escapement_u3.repl), the checks of
# escapement_u5.robot that its examples allow, written before the board came. The
# platform checks the order in which the clock set-up writes its registers against the
# rules of RM0487, and that it waits for each flag it must; it does not check times or
# frequencies. These tests say that the kernel runs and schedules on a Cortex-M33 with the
# timers, the USART and the GPIO of the STM32U385, and that the set-up keeps to the
# manual's sequence, not that the chip runs at the frequency meant, which only the board
# can say.
*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Test Timeout                  2 minutes
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32U3/Examples/nucleo-u385
${RCC}                        ${0x40030C00}
${PWR}                        ${0x40030800}
${FLASH}                      ${0x40022000}

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware into SRAM, as a debugger would on the board, and
    ...                       starts it at its reset handler with the stack pointer of its
    ...                       vector table, which is what the entry code at the head of the
    ...                       image does there (Escapement_RamEntry.S).
    [Arguments]               ${binary}  ${platform}=escapement_u3.repl
    Execute Command           mach create "nucleo_u385"
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

No Rule Broken
    [Documentation]           The words of escapement_u3.repl that count the rules of RM0487 the
    ...                       clock set-up broke, in RCC, PWR and FLASH: all 0.
    # Robot's names ignore case: ${rcc} would be ${RCC}.
    ${in_rcc}=                Read Word  ${RCC + 0x3FC}
    ${in_pwr}=                Read Word  ${PWR + 0x3FC}
    ${in_flash}=              Read Word  ${FLASH + 0x3FC}
    Log To Console            rules broken: RCC ${in_rcc}, PWR ${in_pwr}, FLASH ${in_flash}
    Should Be Equal As Integers  ${in_rcc}  0
    Should Be Equal As Integers  ${in_pwr}  0
    Should Be Equal As Integers  ${in_flash}  0

Every Part Of Soak Without Error
    [Documentation]           SoakU3's eight parts each active and none in error.
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Word  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Word  ${results + 44 + 4 * ${part}}
        Log To Console        part ${part}: ${activity} done, ${errors} errors
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END

*** Test Cases ***
The clock set-up follows RM0487 to 96 MHz
    [Documentation]           TaskLEDU3, 10 ms into its run: the LSE on at the medium-high drive
    ...                       and given to the RCC's functions (LSESYSEN), the MSIS and the MSIK
    ...                       on MSIRC0, the MSIS divided by 1, the MSIK by 8, the PLL mode on the
    ...                       LSE enabled and locked, the booster fed by the MSIS and on, range 1
    ...                       ready, 2 wait states on the flash and its prefetch off, the unlock
    ...                       interrupt of the RCC enabled, the backup domain locked again; and
    ...                       no rule of the manual broken on the way (escapement_u3.repl).
    Load Escapement           TaskLEDU3
    Execute Command           emulation RunFor "0.01"
    No Rule Broken
    ${cr}=                    Read Word  ${RCC}
    ${icscr1}=                Read Word  ${RCC + 0x008}
    ${cfgr4}=                 Read Word  ${RCC + 0x028}
    ${cier}=                  Read Word  ${RCC + 0x050}
    ${bdcr}=                  Read Word  ${RCC + 0x110}
    ${vosr}=                  Read Word  ${PWR + 0x0C}
    ${dbpr}=                  Read Word  ${PWR + 0x28}
    ${acr}=                   Read Word  ${FLASH}
    ${iser0}=                 Read Word  0xE000E100
    Log To Console            CR ${cr} ICSCR1 ${icscr1} CFGR4 ${cfgr4} BDCR ${bdcr} VOSR ${vosr} ACR ${acr}
    Should Be Equal As Integers  ${{ (${cr} >> 6) & 1 }}  1
    Should Be Equal As Integers  ${{ (${cr} >> 10) & 1 }}  1
    Should Be Equal As Integers  ${{ (${icscr1} >> 23) & 1 }}  1
    Should Be Equal As Integers  ${{ (${icscr1} >> 31) & 1 }}  0
    Should Be Equal As Integers  ${{ (${icscr1} >> 29) & 3 }}  0
    Should Be Equal As Integers  ${{ (${icscr1} >> 28) & 1 }}  0
    Should Be Equal As Integers  ${{ (${icscr1} >> 26) & 3 }}  3
    Should Be Equal As Integers  ${{ (${icscr1} >> 21) & 1 }}  0
    Should Be Equal As Integers  ${{ ${cfgr4} & 3 }}  1
    Should Be Equal As Integers  ${{ (${cier} >> 8) & 1 }}  1
    Should Be Equal As Integers  ${{ ${bdcr} & 0x89B }}  0x893
    Should Be Equal As Integers  ${{ ${vosr} & 0x1030103 }}  0x1010101
    Should Be Equal As Integers  ${{ ${dbpr} & 1 }}  0
    Should Be Equal As Integers  ${{ ${acr} & 0x10F }}  2
    Should Be Equal As Integers  ${{ (${iser0} >> 9) & 1 }}  1

Without the LSE the MSIS runs free and the kernel still runs
    [Documentation]           The LSE never starting (RCC 0x3F0 of escapement_u3.repl), as on a
    ...                       board whose crystal fails: the port waits its bound, leaves the
    ...                       PLL mode and the unlock interrupt off, raises the clock all the
    ...                       same and runs SoakU3, whose report says the MSI was never locked
    ...                       (bit 24 of word 93), every part without error. No rule broken.
    [Timeout]                 10 minutes
    Load Escapement           SoakU3
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x3F0} 1
    Execute Command           emulation RunFor "2.5"
    No Rule Broken
    ${cr}=                    Read Word  ${RCC}
    ${bdcr}=                  Read Word  ${RCC + 0x110}
    ${cier}=                  Read Word  ${RCC + 0x050}
    ${icscr1}=                Read Word  ${RCC + 0x008}
    Should Be Equal As Integers  ${{ (${cr} >> 6) & 1 }}  0
    Should Be Equal As Integers  ${{ (${bdcr} >> 7) & 1 }}  0
    Should Be Equal As Integers  ${{ (${cier} >> 8) & 1 }}  0
    Should Be Equal As Integers  ${{ (${icscr1} >> 29) & 3 }}  0
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${seconds}=               Read Word  ${results + 4}
    ${msi}=                   Read Word  ${results + 93 * 4}
    Should Be True            ${seconds} >= 1
    Should Be Equal As Integers  ${msi}  0
    Every Part Of Soak Without Error

An unlock of the MSI's PLL mode is set right by the RCC's interrupt
    [Documentation]           SoakU3 running, the platform unlocks the PLL mode (RCC 0x3F4:
    ...                       MSIPLLUF set, MSIPLL0RDY cleared) and the robot raises interrupt 9,
    ...                       the RCC's, as the chip would: the handler clears the flag, clears
    ...                       and sets MSIPLL0EN, which locks again, and counts it, which the
    ...                       next report of SoakU3 holds in word 93 beside bit 24, set since
    ...                       the MSI was locked at start. No rule broken on the way.
    [Timeout]                 10 minutes
    Load Escapement           SoakU3
    Execute Command           emulation RunFor "1.2"
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x3F4} 1
    Execute Command           sysbus.nvic OnGPIO 9 true
    Execute Command           sysbus.nvic OnGPIO 9 false
    Execute Command           emulation RunFor "1.1"
    No Rule Broken
    # The platform sets MSIPLL0RDY at the third read of RCC_CR after MSIPLL0EN.
    FOR  ${i}  IN RANGE  2
        Read Word             ${RCC}
    END
    ${cr}=                    Read Word  ${RCC}
    ${cifr}=                  Read Word  ${RCC + 0x054}
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${msi}=                   Read Word  ${results + 93 * 4}
    Log To Console            CR ${cr}, CIFR ${cifr}, word 93 ${msi}
    Should Be Equal As Integers  ${{ (${cr} >> 6) & 1 }}  1
    Should Be Equal As Integers  ${{ (${cr} >> 10) & 1 }}  1
    Should Be Equal As Integers  ${{ (${cifr} >> 8) & 1 }}  0
    Should Be Equal As Integers  ${msi}  0x1000001
    Every Part Of Soak Without Error

The platform catches a set-up that breaks the manual's rules
    [Documentation]           The checks of escapement_u3.repl, shown able to fail: written by the
    ...                       robot on a halted machine, the MSIS at 96 MHz in range 2 without
    ...                       the booster nor the wait states, the LSE's drive without the backup
    ...                       domain opened, the PLL mode before the LSE, range 1 asked with
    ...                       range 2 too, and the booster without its clock, each set its bit,
    ...                       and the refused writes leave their registers as they were.
    Load Escapement           TaskLEDU3
    # PWR's clock first, so that only the rules under test are broken.
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x094} 4
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x008} 0x0C800000
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x110} 0x10
    Execute Command           sysbus WriteDoubleWord ${RCC} 0x5D
    Execute Command           sysbus WriteDoubleWord ${PWR + 0x0C} 3
    Execute Command           sysbus WriteDoubleWord ${PWR + 0x0C} 0x102
    ${in_rcc}=                Read Word  ${RCC + 0x3FC}
    ${in_pwr}=                Read Word  ${PWR + 0x3FC}
    ${cr}=                    Read Word  ${RCC}
    ${bdcr}=                  Read Word  ${RCC + 0x110}
    ${vosr}=                  Read Word  ${PWR + 0x0C}
    Log To Console            rules broken: RCC ${in_rcc}, PWR ${in_pwr}
    # 96 MHz in range 2 (3), without the booster (4) nor the wait states (5); BDCR
    # without DBP (6); MSIPLL0EN before the LSE (0).
    Should Be Equal As Integers  ${in_rcc}  0x79
    # R1EN and R2EN equal (0); BOOSTEN without BOOSTSEL (2).
    Should Be Equal As Integers  ${in_pwr}  0x5
    Should Be Equal As Integers  ${{ (${cr} >> 6) & 1 }}  0
    Should Be Equal As Integers  ${bdcr}  0
    Should Be Equal As Integers  ${{ ${vosr} & 3 }}  2

The probe task runs every millisecond
    [Documentation]           TaskLEDU3 toggles D12, PA6, from a task of period 1000 ticks of the
    ...                       1 us timer: a 500 Hz square wave, 1 ms high, 1 ms low.
    Load Escapement           TaskLEDU3

    ${probe}=                 Create LED Tester  sysbus.gpioPortA.probe

    # A virtual time run, not Start Emulation: the tester then starts at a set instant
    # of the pattern, not at one the host's speed decides (escapement_pico2.robot).
    Execute Command           emulation RunFor "0.005"

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.001  tolerance=0.02  testerId=${probe}  pauseEmulation=true

The three periodic tasks are scheduled
    [Documentation]           The three other tasks, of periods 10, 20 and 60 ms, each raise
    ...                       their output on D7, D8 and D13 and lower it before ending.
    Load Escapement           TaskLEDU3

    ${flag1}=                 Create LED Tester  sysbus.gpioPortA.flag1  defaultTimeout=0.25
    ${flag2}=                 Create LED Tester  sysbus.gpioPortC.flag2  defaultTimeout=0.25
    ${flag3}=                 Create LED Tester  sysbus.gpioPortA.flag3  defaultTimeout=0.25

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

The UART echo answers
    [Documentation]           UARTEchoU3 sends every byte received on USART1 back from its
    ...                       interrupt handler, interrupt 61.
    Load Escapement           UARTEchoU3

    ${uart}=                  Create Terminal Tester  sysbus.usart1  defaultPauseEmulation=true

    Start Emulation

    Write Line To Uart        escapement  waitForEcho=false
    Wait For Prompt On Uart   escapement  testerId=${uart}

Timer events wake the event-driven tasks
    [Documentation]           TestTimerEventU3: D7 high 1 ms every 5 ms and D8 high 2 ms every
    ...                       10 ms, the kernel's TIM2 timing the periods, the event manager on
    ...                       TIM4 the high times.
    Load Escapement           TestTimerEventU3

    ${flag1}=                 Create LED Tester  sysbus.gpioPortA.flag1
    ${flag2}=                 Create LED Tester  sysbus.gpioPortC.flag2

    Execute Command           emulation RunFor "0.02"

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.004  tolerance=0.02  testerId=${flag1}  pauseEmulation=true
    Assert LED Is Blinking    testDuration=0.1  onDuration=0.002  offDuration=0.008  tolerance=0.02  testerId=${flag2}  pauseEmulation=true

Scheduling survives the 2^30 wrap of the kernel clock
    [Documentation]           The kernel counts time modulo 2^30 and shifts every temporal
    ...                       variable back when its counter wraps, which TIM2 does at 2^30.
    ...                       As in escapement_u5.robot, the timer runs 1000 times faster
    ...                       (escapement_u3_wrap.repl) and TaskWrapU3 scales its periods to
    ...                       match, which puts the boundary at 1.07 s of emulated time for
    ...                       an unchanged load. Every task must still run afterwards, and the
    ...                       probe, toggled every 50 ms, must keep its period within 2 %.
    Load Escapement           TaskWrapU3  escapement_u3_wrap.repl

    ${flag1}=                 Create LED Tester  sysbus.gpioPortA.flag1  defaultTimeout=1
    ${flag2}=                 Create LED Tester  sysbus.gpioPortC.flag2  defaultTimeout=1
    ${flag3}=                 Create LED Tester  sysbus.gpioPortA.flag3  defaultTimeout=1
    ${probe}=                 Create LED Tester  sysbus.gpioPortA.probe

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true

    # Cross it: 1.2 s at the 1 ns tick is past 2^30, so a counter below 2^30 now has
    # wrapped, which one ignoring its limit would not have (TIM2_CNT).
    Execute Command           emulation RunFor "1.2"
    ${count}=                 Read Word  0x40000024
    Should Be True            ${count} < 0x40000000

    Assert LED Is Blinking    testDuration=0.5  onDuration=0.05  offDuration=0.05  tolerance=0.02  testerId=${probe}  pauseEmulation=true
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

Tasks preempt one another inside the FIFO queue and a slot buffer
    [Documentation]           IPCU3: a long task fills a FIFO queue and writes a 3-slot
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
    Load Escapement           IPCU3
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
    [Documentation]           SoakU3, the firmware of the endurance test, for 3.5 s: its marker
    ...                       set, its heartbeat past three seconds, every part active and none
    ...                       in error — the pulse on time, the queue in order, the buffers
    ...                       never torn nor repeated, the one the interrupt of TIM3 writes
    ...                       among them, the timer events on TIM4 on time, the heartbeat seeing
    ...                       every part move each second, and the stack and the guard words
    ...                       intact. The watchdog, started by the first heartbeat and reloaded
    ...                       by the others, must not have restarted the board: the counts,
    ...                       cleared at each start, would then be those of less than 3 s.
    [Timeout]                 10 minutes
    Load Escapement           SoakU3

    Execute Command           emulation RunFor "3.5"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${marker}=                Read Word  ${results}
    ${seconds}=               Read Word  ${results + 4}
    ${pulses}=                Read Word  ${results + 12}
    Should Be Equal As Integers  ${marker}  0x534F414B
    Should Be True            ${seconds} >= 3 and ${pulses} >= 3400
    Every Part Of Soak Without Error
    No Rule Broken

The endurance test reports on USART1 and counts the bursts of the link
    [Documentation]           SoakU3 reports each second on USART1, the ST-LINK's virtual COM
    ...                       port. Handed three bytes of the link at once, the emulation
    ...                       paused, it reads them one after the other: a burst of three, which
    ...                       word 94 of the results keeps in its top byte, the bytes in order.
    ...                       Each report ends with that word: 28 numbers after SOAK.
    [Timeout]                 10 minutes
    Load Escapement           SoakU3

    ${usart1}=                Create Terminal Tester  sysbus.usart1  defaultPauseEmulation=true
    Wait For Line On Uart     SOAK  timeout=1.5  testerId=${usart1}
    Execute Command           sysbus.usart1 WriteChar 0
    Execute Command           sysbus.usart1 WriteChar 1
    Execute Command           sysbus.usart1 WriteChar 2
    ${line}=                  Wait For Line On Uart  SOAK  timeout=1.5  testerId=${usart1}

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${bytes}=                 Read Word  ${results + 88 * 4}
    ${errors}=                Read Word  ${results + 89 * 4}
    ${burst}=                 Read Word  ${results + 94 * 4}
    Should Be Equal As Integers  ${bytes}  3
    Should Be Equal As Integers  ${errors}  0
    Should Be Equal As Integers  ${{ ${burst} >> 24 }}  3
    Should Be Equal As Integers  ${{ len($line.Line.split()) }}  29
