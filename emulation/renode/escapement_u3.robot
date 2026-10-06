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
# can say. The platform enters Stop 2 when the port asks for it (PWR_SR.CSSF with LPMS =
# 010) and lays out the state RM0487 gives the wake-up, so that the tests tagged stop2 see
# the idle task replay the raise of the clock from there, by the same rules; they take
# the frequency of the build as MHZ and its platform as PLATFORM (escapement_u3_48mhz.repl
# for MHZ=48, and so on for 24 and 12), 96 MHz and escapement_u3.repl by default.
*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Test Timeout                  2 minutes
Resource                      ${RENODEKEYWORDS}
Library                       link_frame.py

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32U3/Examples/nucleo-u385
${RCC}                        ${0x40030C00}
${PWR}                        ${0x40030800}
${FLASH}                      ${0x40022000}
${MHZ}                        96
${PLATFORM}                   escapement_u3.repl

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

Send On LPUART1
    [Documentation]           Bytes into LPUART1 as a sender writes them, all at once.
    [Arguments]               @{bytes}
    FOR  ${byte}  IN  @{bytes}
        Execute Command       sysbus.lpuart1 WriteChar ${byte}
    END

Symbol
    [Documentation]           The address of a symbol of the image loaded, as an integer.
    [Arguments]               ${name}
    ${address}=               Execute Command  sysbus GetSymbolAddress "${name}"
    ${address}=               Convert To Integer  ${address.strip()}
    RETURN                    ${address}

The Clock Is Back At Its Frequency
    [Documentation]           After the last wake-up, the clock as OSInitializeSystemClocks left
    ...                       it for the build's MHZ: the MSIS on MSIRC0 and its divider, the
    ...                       system clock on the MSIS (SWS 00), range 1 ready at 96 MHz and
    ...                       range 2 chosen below, the booster on and ready above 24 MHz.
    ${icscr1}=                Read Word  ${RCC + 0x008}
    ${cfgr1}=                 Read Word  ${RCC + 0x01C}
    ${vosr}=                  Read Word  ${PWR + 0x3F8}
    ${div}=                   Evaluate  {96: 0, 48: 1, 24: 2, 12: 3}[int(${MHZ})]
    # R1RDY at 96 MHz, which the raise waits for; R2RDY is not, the chip waking in range 2.
    ${range}=                 Evaluate  0x10001 if int(${MHZ}) == 96 else 0x2
    ${boost}=                 Evaluate  0x1000100 if int(${MHZ}) > 24 else 0
    Log To Console            ICSCR1 ${icscr1} CFGR1 ${cfgr1} VOSR ${vosr}
    Should Be Equal As Integers  ${{ (${icscr1} >> 29) & 3 }}  ${div}
    Should Be Equal As Integers  ${{ (${icscr1} >> 31) & 1 }}  0
    Should Be Equal As Integers  ${{ (${cfgr1} >> 2) & 3 }}  0
    Should Be Equal As Integers  ${{ ${vosr} & (0x30003 if int(${MHZ}) == 96 else 0x3) }}  ${range}
    Should Be Equal As Integers  ${{ ${vosr} & 0x1000100 }}  ${boost}

Sleep In Stop 2 Within Its Times
    [Documentation]           SleepU3's results after a run: the instances on their period,
    ...                       none late, the timer events on time, and as many entries into
    ...                       Stop 2 as instances at least, which the platform counts too.
    ${results}=               Symbol  Results
    ${instances}=             Read Word  ${results + 4}
    ${ticks}=                 Read Word  ${results + 8}
    ${micros}=                Read Word  ${results + 12}
    ${jitter}=                Read Word  ${results + 16}
    ${entries}=               Read Word  ${results + 20}
    ${wake}=                  Read Word  ${results + 24}
    ${late}=                  Read Word  ${results + 28}
    ${nolse}=                 Read Word  ${results + 32}
    ${events}=                Read Word  ${results + 36}
    ${eventoff}=              Read Word  ${results + 40}
    ${missed}=                Read Word  ${results + 56}
    ${stops}=                 Read Word  ${PWR + 0x3F0}
    Log To Console            ${instances} instances, ${entries} into Stop 2 (${stops} on the platform), longest wake-up ${wake} ticks, ${missed} locks missed, gap off by ${jitter} us, ${events} events off by ${eventoff} us
    Should Be True            ${instances} >= 25
    Should Be Equal As Integers  ${nolse}  0
    Should Be Equal As Integers  ${late}  0
    Should Be True            ${jitter} <= 5
    Should Be True            ${events} >= ${instances}
    Should Be True            ${eventoff} <= 20
    Should Be True            ${entries} >= ${instances}
    Should Be True            ${stops} >= ${entries}
    Should Be True            abs(${ticks} * 1000000 - ${micros} * 32768) <= ${micros} * 32768 / 10000
    RETURN                    ${entries}  ${wake}  ${missed}

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
    [Tags]                    stop2at96
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

LPTIM1 counts the crystal of 32.768 kHz
    [Documentation]           TestLPTimerU3: LPTIM1 on the LSE against TIM2, every 250 ms for
    ...                       3 s, 32768 ticks for 1,000,000 us, and the compare it sets each
    ...                       time found reached at the next. LPTIM1 on the LSE, its clock
    ...                       enabled, reset through the RCC rather than by its ENABLE bit
    ...                       (ES0626, 2.11.1). No rule broken.
    Load Escapement           TestLPTimerU3
    Execute Command           emulation RunFor "3"
    No Rule Broken
    ${results}=               Symbol  Results
    ${instances}=             Read Word  ${results + 4}
    ${ticks}=                 Read Word  ${results + 8}
    ${micros}=                Read Word  ${results + 12}
    ${missed}=                Read Word  ${results + 16}
    ${nolse}=                 Read Word  ${results + 20}
    ${ccipr3}=                Read Word  ${RCC + 0x108}
    ${apb3enr}=               Read Word  ${RCC + 0x0A8}
    ${apb3rstr}=              Read Word  ${RCC + 0x080}
    Log To Console            ${instances} instances, ${ticks} ticks for ${micros} us
    Should Be True            ${instances} >= 10
    Should Be Equal As Integers  ${nolse}  0
    Should Be Equal As Integers  ${missed}  0
    Should Be True            abs(${ticks} * 1000000 - ${micros} * 32768) <= ${micros} * 32768 / 1000
    Should Be Equal As Integers  ${{ (${ccipr3} >> 10) & 3 }}  3
    Should Be Equal As Integers  ${{ (${apb3enr} >> 11) & 1 }}  1
    Should Be Equal As Integers  ${{ (${apb3rstr} >> 11) & 1 }}  0

The idle task sleeps in Stop 2 and the wake-up replays the raise of the clock
    [Documentation]           SleepU3: the idle task arms LPTIM1 short of each 100 ms period,
    ...                       stops TIM2, TIM4 and USART1 and enters Stop 2, from which the
    ...                       platform wakes it in range 2, the MSIS at 48 MHz, unlocked; the
    ...                       idle task waits for the lock, sleeps the margin, raises range 1
    ...                       and the clock again and moves TIM2 and TIM4 on by what LPTIM1
    ...                       counted. Every instance on its period, none late, the events on
    ...                       time, every wake-up locked within its bound and part of its
    ...                       margin slept, the clock back as at start, and no rule of the
    ...                       manual broken on the way, though each wake-up starts afresh.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    Execute Command           emulation RunFor "3"
    ${entries}  ${wake}  ${missed}=    Sleep In Stop 2 Within Its Times
    No Rule Broken
    The Clock Is Back At Its Frequency
    ${counts}=                Symbol  Counts
    ${slow}=                  Read Word  ${counts + 20}
    ${pwrcr1}=                Read Word  ${PWR}
    ${stpenr}=                Read Word  ${RCC + 0x0F8}
    ${cr}=                    Read Word  ${RCC}
    Should Be Equal As Integers  ${missed}  0
    Should Be True            ${wake} < 64
    # Counts holds the idle task's counts at every instant, Results only at each report.
    Should Be True            ${slow} >= ${entries}
    Should Be Equal As Integers  ${{ ${pwrcr1} & 7 }}  2
    Should Be Equal As Integers  ${{ (${stpenr} >> 11) & 1 }}  1
    Should Be Equal As Integers  ${{ (${stpenr} >> 6) & 1 }}  1
    # HSI16 back for LPUART1 after each wake-up, which clears it.
    Should Be Equal As Integers  ${{ (${cr} >> 11) & 1 }}  1

The wake-up keeps to the manual when R1EN reads 1 after Stop 2
    [Documentation]           RM0487 does not say what PWR_VOSR reads after a Stop 2 entered in
    ...                       range 1. Here the platform keeps R1EN, its ready flag coming back
    ...                       4 reads later (PWR 0x3F4): the idle task waits for R1RDY rather
    ...                       than write R1EN again, and runs as with R1EN cleared, no rule
    ...                       broken.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    Execute Command           sysbus WriteDoubleWord ${PWR + 0x3F4} 1
    Execute Command           emulation RunFor "3"
    ${entries}  ${wake}  ${missed}=    Sleep In Stop 2 Within Its Times
    No Rule Broken
    The Clock Is Back At Its Frequency
    Should Be Equal As Integers  ${missed}  0

A lock that does not come is waited for no longer than its bound
    [Documentation]           The MSI's PLL mode never locking again after Stop 2 (RCC 0x3EC):
    ...                       the idle task waits OS_STOP2_LOCK_TICKS of LPTIM1, 1.95 ms, counts
    ...                       the miss, and raises the clock on the MSI unlocked, which the
    ...                       platform flags as the one rule broken (RCC bit 9); every wake-up
    ...                       still in time, the margin of 3 ms covering the wait. Without the
    ...                       bound the idle task hung there.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x3EC} 0x7FFFFFFF
    Execute Command           emulation RunFor "3"
    ${entries}  ${wake}  ${missed}=    Sleep In Stop 2 Within Its Times
    ${in_rcc}=                Read Word  ${RCC + 0x3FC}
    ${in_pwr}=                Read Word  ${PWR + 0x3FC}
    Should Be Equal As Integers  ${missed}  ${entries}
    Should Be True            64 <= ${wake} < 98
    ${expected}=              Evaluate  0x200 if int(${MHZ}) > 48 else 0
    Should Be Equal As Integers  ${in_rcc}  ${expected}
    Should Be Equal As Integers  ${in_pwr}  0

With MSIPLL0FAST the chip wakes locked
    [Documentation]           SleepFastU3, SleepU3 built with FAST=1: MSIPLL0FAST set once the
    ...                       PLL mode has locked, which keeps it through Stop 2 (RM0487, p. 415,
    ...                       419). The platform never locking again otherwise (RCC 0x3EC), no
    ...                       wake-up misses its lock, and no rule is broken.
    [Tags]                    stop2
    Load Escapement           SleepFastU3  ${PLATFORM}
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x3EC} 0x7FFFFFFF
    Execute Command           emulation RunFor "3"
    ${entries}  ${wake}  ${missed}=    Sleep In Stop 2 Within Its Times
    No Rule Broken
    ${cr}=                    Read Word  ${RCC}
    Should Be Equal As Integers  ${missed}  0
    Should Be Equal As Integers  ${{ (${cr} >> 8) & 1 }}  1

SleepU3 reports on USART1 across Stop 2
    [Documentation]           SleepU3's reports go out on USART1, the ST-LINK's virtual COM port,
    ...                       which the idle task disables before each Stop 2 and enables again
    ...                       after it: two lines of SLEEP and its 16 numbers, a second apart,
    ...                       the instances between them counted.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    ${usart1}=                Create Terminal Tester  sysbus.usart1  defaultPauseEmulation=true
    ${first}=                 Wait For Line On Uart  SLEEP  timeout=2.5  testerId=${usart1}
    ${second}=                Wait For Line On Uart  SLEEP  timeout=2.5  testerId=${usart1}
    Log To Console            ${first.Line} / ${second.Line}
    Should Be Equal As Integers  ${{ len($second.Line.split()) }}  17
    Should Be Equal As Integers  ${{ int($second.Line.split()[1], 16) - int($first.Line.split()[1], 16) }}  10
    No Rule Broken

A byte on LPUART1 keeps the idle task out of Stop 2 for the window
    [Documentation]           SleepU3: a wake-up byte sent 50 ms into a period, the chip in
    ...                       Stop 2 since the event at 40 ms, wakes it, and the idle task stays
    ...                       in Sleep for the window after it, 20 ms, TIM2 running, for the
    ...                       bytes of a message to come on a running clock; LPTIM1 ends the
    ...                       window and the idle task enters Stop 2 again before the next
    ...                       period, TIM2 standing still. No rule broken.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    ${counts}=                Symbol  Counts
    Execute Command           emulation RunFor "1.05"
    ${entries}=               Read Word  ${counts}
    Execute Command           sysbus.lpuart1 WriteChar 0
    # Within the window: TIM2 runs.
    Execute Command           emulation RunFor "0.014"
    ${a}=                     Read Word  0x40000024
    Execute Command           emulation RunFor "0.001"
    ${b}=                     Read Word  0x40000024
    Should Be True            ${b} - ${a} > 900
    # Past it: Stop 2 again, TIM2 standing still, the sleep held once.
    Execute Command           emulation RunFor "0.010"
    ${a}=                     Read Word  0x40000024
    Execute Command           emulation RunFor "0.001"
    ${b}=                     Read Word  0x40000024
    Should Be Equal As Integers  ${a}  ${b}
    ${held}=                  Read Word  ${counts + 16}
    Should Be True            ${held} >= 1
    # The entry into Stop 2 is counted on waking, before the next period.
    Execute Command           emulation RunFor "0.030"
    ${after}=                 Read Word  ${counts}
    ${late}=                  Read Word  ${counts + 8}
    Should Be Equal As Integers  ${after}  ${entries + 2}
    Should Be Equal As Integers  ${late}  0
    No Rule Broken

Bytes on LPUART1 every 10 ms keep the idle task out of Stop 2
    [Documentation]           SleepU3 sent a wake-up byte then a frame of 47 bytes, a byte every
    ...                       10 ms, half a second, less than the window apart: the idle task
    ...                       never enters Stop 2 meanwhile and every byte is received in
    ...                       order, none dropped; once they stop, it enters Stop 2 again, and
    ...                       no period was late.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    ${counts}=                Symbol  Counts
    ${results}=               Symbol  Results
    ${frame}=                 Link Frame  0  47
    Execute Command           emulation RunFor "1.003"
    Execute Command           sysbus.lpuart1 WriteChar 0
    Execute Command           emulation RunFor "0.001"
    ${entries}=               Read Word  ${counts}
    FOR  ${byte}  IN  @{frame}
        Execute Command       emulation RunFor "0.010"
        Execute Command       sysbus.lpuart1 WriteChar ${byte}
    END
    Execute Command           emulation RunFor "0.001"
    ${during}=                Read Word  ${counts}
    ${held}=                  Read Word  ${counts + 16}
    ${received}=              Read Word  ${results + 44}
    ${errors}=                Read Word  ${results + 48}
    ${dropped}=               Read Word  ${results + 64}
    Log To Console            ${during} - ${entries} entries into Stop 2 during the bytes, ${held} sleeps held, ${received} bytes received
    Should Be Equal As Integers  ${during}  ${entries}
    Should Be True            ${held} >= 10
    Should Be Equal As Integers  ${received}  47
    Should Be Equal As Integers  ${errors}  0
    Should Be Equal As Integers  ${dropped}  0
    Execute Command           emulation RunFor "0.5"
    ${after}=                 Read Word  ${counts}
    ${late}=                  Read Word  ${counts + 8}
    Should Be True            ${after} >= ${entries} + 8
    Should Be Equal As Integers  ${late}  0

A wake-up byte that comes out wrong is dropped with the frames it spoils
    [Documentation]           SleepU3's link at 115,200 baud: the byte that wakes the chip from
    ...                       Stop 2 is sampled while HSI16 starts and may come out as anything.
    ...                       Sent as 0x5A instead of 0x00, 50 ms into a period, it ends as a
    ...                       frame of its own at the 0x00 that opens the next, too short, and is
    ...                       dropped; the frame after it 5 ms later is received whole, one with
    ...                       its CRC wrong is dropped, and the count goes on in the frame after,
    ...                       none out of it.
    [Tags]                    stop2
    Load Escapement           SleepU3  ${PLATFORM}
    ${results}=               Symbol  Results
    ${first}=                 Link Frame  0  10
    ${bad}=                   Link Frame  10  10  bad_crc=True
    ${second}=                Link Frame  10  20
    Execute Command           emulation RunFor "1.05"
    Execute Command           sysbus.lpuart1 WriteChar 0x5A
    Execute Command           emulation RunFor "0.005"
    Send On LPUART1           @{first}
    Execute Command           emulation RunFor "0.005"
    Send On LPUART1           @{bad}
    Execute Command           emulation RunFor "0.005"
    Send On LPUART1           @{second}
    Execute Command           emulation RunFor "0.005"
    ${received}=              Read Word  ${results + 44}
    ${errors}=                Read Word  ${results + 48}
    ${overruns}=              Read Word  ${results + 52}
    ${dropped}=               Read Word  ${results + 64}
    Log To Console            ${received} bytes received, ${errors} out of the count, ${dropped} frames dropped
    Should Be Equal As Integers  ${received}  30
    Should Be Equal As Integers  ${errors}  0
    Should Be Equal As Integers  ${overruns}  0
    Should Be Equal As Integers  ${dropped}  2

The idle task sleeps in Stop 2 across the 2^30 wrap of the kernel clock
    [Documentation]           SleepWrapU3, SleepU3 with its times a thousand times longer, on
    ...                       TIM2, TIM4 and LPTIM1 a thousand times faster
    ...                       (escapement_u3_wrap.repl): 2.5 s cross the wrap at 2^30 twice
    ...                       while the idle task sleeps in Stop 2 and moves TIM2 and TIM4 on, in
    ...                       ticks of a nanosecond, its margins scaled with them. Every start
    ...                       one period after the last within 5 us, none late, and every timer
    ...                       event within 20 us of its time, as on the U5. No rule broken.
    [Tags]                    stop2at96
    Load Escapement           SleepWrapU3  escapement_u3_wrap.repl
    Execute Command           emulation RunFor "2.5"
    ${results}=               Symbol  Results
    ${instances}=             Read Word  ${results + 4}
    ${jitter}=                Read Word  ${results + 16}
    ${entries}=               Read Word  ${results + 20}
    ${late}=                  Read Word  ${results + 28}
    ${nolse}=                 Read Word  ${results + 32}
    ${events}=                Read Word  ${results + 36}
    ${eventoff}=              Read Word  ${results + 40}
    Log To Console            ${instances} instances, ${entries} into Stop 2, ${late} late, gap off by ${jitter} ns at most, ${events} events off by ${eventoff} ns at most
    Should Be True            ${instances} >= 20
    Should Be Equal As Integers  ${nolse}  0
    Should Be Equal As Integers  ${late}  0
    Should Be True            ${entries} >= ${instances}
    Should Be True            ${jitter} <= 5000
    Should Be True            ${events} >= ${instances}
    Should Be True            ${eventoff} <= 20000
    No Rule Broken

The idle task sleeps up to the wrap when an arrival lies beyond it
    [Documentation]           Stop2EventWrapU3: an event-driven task of 300 ms, signalled 10 ms
    ...                       after each start, waits for the rest of its period in the arrival
    ...                       queue, its time beyond the 2^30 wrap for the period that crosses
    ...                       it, which the kernel arms TIM2's compare with. The idle task, in
    ...                       Stop 2 on LPTIM1, must sleep no further than the wrap: TIM2 and
    ...                       TIM4, both moved on by what LPTIM1 counted, must drift apart by as
    ...                       much over the period across the wrap as over the others, within
    ...                       5 us, and every start come one period after the last. Scaled as
    ...                       SleepWrapU3, in ns.
    [Tags]                    stop2at96
    Load Escapement           Stop2EventWrapU3  escapement_u3_wrap.repl
    Execute Command           emulation RunFor "2.5"
    ${results}=               Symbol  Results
    ${marker}=                Read Word  ${results}
    ${instances}=             Read Word  ${results + 4}
    ${gapoff}=                Read Word  ${results + 8}
    ${skewmin}=               Read Word  ${results + 12}
    ${skewmax}=               Read Word  ${results + 16}
    ${spread}=                Evaluate  ((${skewmax} - ${skewmin}) & 0xFFFFFFFF)
    ${entries}=               Read Word  ${results + 20}
    ${nolse}=                 Read Word  ${results + 28}
    Log To Console            ${instances} instances, ${entries} into Stop 2, gap off by ${gapoff} ns at most, TIM4 less TIM2 over a period spread over ${spread} ns
    Should Be Equal As Integers  ${marker}  0x53574556
    Should Be True            ${instances} >= 6
    Should Be True            ${entries} >= ${instances}
    Should Be Equal As Integers  ${nolse}  0
    Should Be True            ${gapoff} <= 5000
    Should Be True            ${spread} <= 5000
    No Rule Broken

The endurance test runs with the idle task of Stop 2 installed
    [Documentation]           SoakStop2U3, SoakU3 calling OSInitStop2: its pulse every
    ...                       millisecond and USART1's reception leave the idle task no Stop 2,
    ...                       which it declines each time, and every part runs without error
    ...                       for 3.5 s as without it.
    [Timeout]                 10 minutes
    [Tags]                    stop2at96
    Load Escapement           SoakStop2U3
    Execute Command           emulation RunFor "3.5"
    ${results}=               Symbol  Results
    ${counts}=                Symbol  Counts
    ${seconds}=               Read Word  ${results + 4}
    ${entries}=               Read Word  ${counts}
    ${stops}=                 Read Word  ${PWR + 0x3F0}
    Should Be True            ${seconds} >= 3
    Should Be Equal As Integers  ${entries}  0
    Should Be Equal As Integers  ${stops}  0
    Every Part Of Soak Without Error
    No Rule Broken

The platform catches a wake-up from Stop 2 that breaks the manual's rules
    [Documentation]           The checks of escapement_u3.repl on Stop 2, shown able to fail:
    ...                       SleepU3 running at 96 MHz, the robot clears the booster, then
    ...                       enters Stop 2 itself with TIM2 counting and USART1 enabled, and
    ...                       raises the MSIS back to 96 MHz at once, before range 1, the
    ...                       booster, the wait states, the MSIS's ready flag and the lock;
    ...                       then enters a Stop mode with LPMS cleared. Each sets its bit.
    [Tags]                    stop2at96
    Load Escapement           SleepU3
    Execute Command           emulation RunFor "0.12"
    No Rule Broken
    ${vosr}=                  Read Word  ${PWR + 0x0C}
    Execute Command           sysbus WriteDoubleWord ${PWR + 0x0C} ${{ ${vosr} & 0x3 }}
    Execute Command           sysbus WriteDoubleWord 0x40000000 0x5
    Execute Command           sysbus WriteDoubleWord 0x40013800 0x2000000D
    Execute Command           sysbus WriteDoubleWord ${PWR + 0x38} 1
    ${icscr1}=                Read Word  ${RCC + 0x008}
    Should Be Equal As Integers  ${{ (${icscr1} >> 29) & 3 }}  1
    Execute Command           sysbus WriteDoubleWord ${RCC + 0x008} ${{ ${icscr1} & ~(3 << 29) }}
    Execute Command           sysbus WriteDoubleWord ${PWR} 0
    Execute Command           sysbus WriteDoubleWord ${PWR + 0x38} 1
    ${in_rcc}=                Read Word  ${RCC + 0x3FC}
    ${in_pwr}=                Read Word  ${PWR + 0x3FC}
    Log To Console            rules broken: RCC ${in_rcc}, PWR ${in_pwr}
    # Above 48 MHz before R1RDY (3), above 24 before BOOSTRDY (4), before the wait states
    # (5), while the MSIS is not ready (8), before the lock (9).
    Should Be Equal As Integers  ${in_rcc}  0x338
    # Stop 2 from 96 MHz without the booster (5), another Stop mode (6), TIM2 counting (7),
    # USART1 enabled (8).
    Should Be Equal As Integers  ${in_pwr}  0x1E0

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
