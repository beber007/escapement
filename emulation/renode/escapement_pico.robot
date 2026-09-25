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
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/RP2040/Examples/pico
# How the examples were built, given by renode-test --variable: KERNEL:PA for make
# KERNEL=PA, UNDERVOLT:1 as well for make KERNEL=PA UNDERVOLT=1, and POWER:DRA for
# make KERNEL=PA POWER=DRA, likewise for the other power-management policies.
${KERNEL}                     HARD
${UNDERVOLT}                  0
${POWER}                      OTE
${CHECK}                      p = r'${CURDIR}'; import sys; p in sys.path or sys.path.insert(0, p); import rp2040_dvfs_check as c

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware linked into SRAM the way OpenOCD does on the
    ...                       board: core 0 starts at the first instruction of the image,
    ...                       0x20000000, which sets up its own stack (Escapement_RamEntry.S).
    ...                       Core 1, which the kernel never starts and which sleeps in the
    ...                       bootrom on the board, is halted: a second time domain would
    ...                       make the LED tester time some edges from the wrong core.
    ...                       The timer of the models is replaced by the fixed copy in
    ...                       Escapement_RP2040_Timer.cs, whose alarms do not interfere with
    ...                       one another. The steps of the board script of the models are
    ...                       spelt out: the copy has to be compiled once their assembly is
    ...                       loaded, and a platform description cannot redeclare the timer
    ...                       with another type, so it is unregistered first.
    [Arguments]               ${binary}
    Execute Command           $machine_name="raspberry_pico"
    Execute Command           include @${CURDIR}/rp2040/cores/initialize_peripherals.resc
    Execute Command           include @${CURDIR}/Escapement_RP2040_Timer.cs
    Execute Command           machine LoadPlatformDescription @${CURDIR}/escapement_pico.repl
    Execute Command           sysbus LoadELF @${CURDIR}/rp2040/bootroms/rp2040/b2.elf
    Execute Command           sysbus Unregister sysbus.timer
    Execute Command           machine LoadPlatformDescription @${CURDIR}/escapement_pico_timer.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/${binary}.elf
    Execute Command           sysbus.cpu0 VectorTableOffset 0x00000000
    Execute Command           sysbus.cpu1 VectorTableOffset 0x00000000
    Execute Command           sysbus.cpu0 PC 0x20000000
    Execute Command           sysbus.cpu1 IsHalted true

Check The DVFS Driver
    [Documentation]           Hooks the writes to VREG, CLK_SYS_CTRL and the post dividers of
    ...                       the PLL to rp2040_dvfs_check.py. Only where no duration is
    ...                       measured: with the hooks in place the probe fails its 2 %.
    Execute Command           sysbus AddWatchpointHook 0x40064000 DoubleWord Write "${CHECK}; c.on_vreg(self, value)"
    Execute Command           sysbus AddWatchpointHook 0x4000803C DoubleWord Write "${CHECK}; c.on_clk_sys_ctrl(self, value)"
    Execute Command           sysbus AddWatchpointHook 0x4002800C DoubleWord Write "${CHECK}; c.on_pll_prim(self, value)"

Read Counter
    [Documentation]           Reads back one of the values rp2040_dvfs_check.py keeps in the
    ...                       scratch registers of the VREG model.
    [Arguments]               ${address}
    ${value}=                 Execute Command  sysbus ReadDoubleWord ${address}
    ${value}=                 Convert To Integer  ${value.strip()}
    RETURN                    ${value}

*** Test Cases ***
The probe task runs every millisecond
    [Documentation]           TaskLEDPico toggles GPIO 4 from a task of period 1000 ticks. The
    ...                       RP2040 timer counts microseconds from the 12 MHz crystal, not
    ...                       from the core clock, so the output must be a 500 Hz square
    ...                       wave: 1 ms high, 1 ms low. On the board a frequency counter
    ...                       reads 500.02 Hz (docs/rp2040.md).
    Load Escapement           TaskLEDPico

    ${probe}=                 Create LED Tester  sysbus.gpio.probe

    # A virtual time run, not Start Emulation: the tester then starts at a set instant
    # of the pattern, not at one the host's speed decides; on the GitHub runners, the
    # first test of a suite once began mid-state and failed (2026-09-25).
    Execute Command           emulation RunFor "0.005"

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.001  tolerance=0.02  testerId=${probe}  pauseEmulation=true

The three periodic tasks are scheduled
    [Documentation]           The three other tasks, of periods 10, 20 and 60 ms, each raise
    ...                       their output on GPIO 25, 2 and 3 and lower it before ending.
    Load Escapement           TaskLEDPico

    ${flag1}=                 Create LED Tester  sysbus.gpio.led  defaultTimeout=0.25
    ${flag2}=                 Create LED Tester  sysbus.gpio.flag2  defaultTimeout=0.25
    ${flag3}=                 Create LED Tester  sysbus.gpio.flag3  defaultTimeout=0.25

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

The UART echo answers
    [Documentation]           UARTEchoPico sends every byte received on UART0 back from its
    ...                       interrupt handler.
    Load Escapement           UARTEchoPico

    ${uart}=                  Create Terminal Tester  sysbus.uart0  defaultPauseEmulation=true

    Start Emulation

    Write Line To Uart        escapement  waitForEcho=false
    Wait For Prompt On Uart   escapement  testerId=${uart}

Timer events wake the event-driven tasks
    [Documentation]           TestTimerEventPico pairs each periodic task with an event-driven
    ...                       one, as TestTimerEventF4 does. SetLed1Task raises GPIO 2 every
    ...                       5 ms and asks the event manager on alarm 2 to wake ClearLed1Task
    ...                       1 ms later, which lowers it; SetLed2Task and ClearLed2Task do the
    ...                       same on GPIO 3 every 10 ms, for 2 ms. The period checks the
    ...                       kernel's alarms, the high time the event manager's, and both
    ...                       that each event-driven task ran when it was woken.
    Load Escapement           TestTimerEventPico

    ${flag1}=                 Create LED Tester  sysbus.gpio.flag2
    ${flag2}=                 Create LED Tester  sysbus.gpio.flag3

    # A virtual time run, not Start Emulation: the tester then starts at a set instant
    # of the pattern, not at one the host's speed decides; on the GitHub runners, the
    # first test of a suite once began mid-state and failed (2026-09-25).
    Execute Command           emulation RunFor "0.02"

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.004  tolerance=0.02  testerId=${flag1}  pauseEmulation=true
    Assert LED Is Blinking    testDuration=0.1  onDuration=0.002  offDuration=0.008  tolerance=0.02  testerId=${flag2}  pauseEmulation=true

Scheduling survives the 2^30 wrap of the kernel clock
    [Documentation]           The kernel counts time modulo 2^30 and shifts every temporal
    ...                       variable back when its counter wraps; the port rebuilds that
    ...                       wrap with ALARM1, since the RP2040 counter never wraps. At the
    ...                       1 us tick of the chip it comes after eighteen minutes, so no
    ...                       test had reached it. Here the timer runs at 1 GHz and
    ...                       TaskWrapPico scales its periods to match, which puts the
    ...                       boundary at 1.07 s of emulated time for an unchanged load.
    ...                       Every task must still run afterwards, and the probe, toggled
    ...                       every 50 ms, must keep its period within 2 %.
    Load Escapement           TaskWrapPico
    Execute Command           sysbus.timer Frequency 1000000000

    ${flag1}=                 Create LED Tester  sysbus.gpio.led  defaultTimeout=1
    ${flag2}=                 Create LED Tester  sysbus.gpio.flag2  defaultTimeout=1
    ${flag3}=                 Create LED Tester  sysbus.gpio.flag3  defaultTimeout=1
    ${probe}=                 Create LED Tester  sysbus.gpio.probe

    Start Emulation

    # Before the boundary: the three tasks are running normally.
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true

    # Cross it, and check that the counter did.
    Execute Command           emulation RunFor "1.2"
    ${raw}=                   Execute Command  sysbus ReadDoubleWord 0x40054028
    Should Be True            ${raw.strip()} > 0x40000000

    # After the boundary: every task must still be scheduled, the probe on time.
    Assert LED Is Blinking    testDuration=0.5  onDuration=0.05  offDuration=0.05  tolerance=0.02  testerId=${probe}  pauseEmulation=true
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

Tasks preempt one another inside the FIFO queue and a slot buffer
    [Documentation]           IPCPico: a long task fills a FIFO queue and writes a 3-slot
    ...                       buffer, a short one of higher priority preempts it wherever it
    ...                       stands to put its own records and read the buffer once each
    ...                       time, and an event-driven task empties the queue. Every record
    ...                       must come out once and in order, and the buffer never give a
    ...                       record twice. An operation preempted in the queue is completed
    ...                       by the task that preempts it: the hooks count the helpers of
    ...                       the queue entered for a descriptor far from the stack pointer,
    ...                       that is, on the stack of the preempted task, and some must be.
    Load Escapement           IPCPico
    FOR  ${helper}  ${reg}  IN  FIFOEnqueueHelper  1  FIFODequeueHelper  2
        ${address}=           Execute Command  sysbus GetSymbolAddress "${helper}"
        Execute Command       sysbus.cpu0 AddHook ${address.strip()} "import System; d = System.AppDomain.CurrentDomain; des = int(str(self.GetRegisterUnsafe(${reg})), 0); sp = int(str(self.GetRegisterUnsafe(13)), 0); d.SetData('helped', (d.GetData('helped') or 0) + (1 if des - sp > 64 else 0))"
    END

    Execute Command           emulation RunFor "0.05"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${put0}=                  Read Counter  ${results}
    ${put1}=                  Read Counter  ${results + 4}
    ${taken0}=                Read Counter  ${results + 8}
    ${taken1}=                Read Counter  ${results + 12}
    ${out_of_order}=          Read Counter  ${results + 16}
    ${slot_reads}=            Read Counter  ${results + 24}
    ${slot_repeats}=          Read Counter  ${results + 28}
    ${slot_torn}=             Read Counter  ${results + 32}
    ${helped}=                Execute Command  python "import System; print(System.AppDomain.CurrentDomain.GetData('helped') or 0)"
    ${helped}=                Convert To Integer  ${helped.strip()}
    Log To Console            put ${put0}+${put1}, taken ${taken0}+${taken1}, slot reads ${slot_reads}, helped ${helped}
    Should Be True            ${taken0} > 200 and ${taken1} > 10
    Should Be True            ${put0} - ${taken0} <= 8 and ${put1} - ${taken1} <= 8
    Should Be Equal As Integers  ${out_of_order}  0
    Should Be True            ${slot_reads} > 10
    Should Be Equal As Integers  ${slot_repeats}  0
    Should Be Equal As Integers  ${slot_torn}  0
    Should Be True            ${helped} > 0

The power-aware kernel scales the frequency and the voltage
    [Documentation]           Under the power-aware kernel the tasks of TaskLEDPico, which
    ...                       declare their execution times, leave the core idle most of the
    ...                       time, and the kernel lowers the frequency and the voltage whenever
    ...                       the deadlines allow. Write hooks check that clk_sys never runs
    ...                       faster than the core voltage allows (rp2040_dvfs_check.py):
    ...                       the driver raises the voltage before the frequency and lowers it
    ...                       after, which a driver doing either always first fails when
    ...                       undervolted. Over 100 ms the voltage changed 196 times under
    ...                       OTE (2026-09-22); the test asks for 100. The
    ...                       emulated core does not slow down with clk_sys, so this
    ...                       proves the registers are driven in the right order, not that
    ...                       any energy is saved.
    Skip If                   '${KERNEL}' != 'PA'  built without the power-aware kernel
    Load Escapement           TaskLEDPico
    Check The DVFS Driver
    Create Log Tester         0

    Execute Command           emulation RunFor "0.1"

    # SRAM ends at 0x20042000. The context switch of the Cortex-M0 once cleared every
    # flag of a starting task but the running one; the kernel then took the idle task
    # for a periodic one and updated a field past its control block, beyond the RAM.
    Should Not Be In Log      non existing peripheral at 0x2004

    ${violations}=            Read Counter  0x40064108
    Should Be Equal As Integers  ${violations}  0
    ${changes}=               Read Counter  0x40064100
    # DRA gives a task only the time left by instances of earlier deadline that have
    # ended, and does not stretch the last one to the next arrival as OTE does: in
    # TaskLEDPico nothing is left to it, and it keeps the fastest speed, as it does in
    # test_scheduler wrap on the host. Measured under Renode on 2026-09-24: 0 changes.
    IF  '${POWER}' == 'DRA'
        Should Be Equal As Integers  ${changes}  0
        Pass Execution        DRA had nothing to reclaim and kept the fastest speed
    END
    Should Be True            ${changes} >= 100
    ${lowest}=                Read Counter  0x40064104
    ${expected}=              Set Variable If  '${UNDERVOLT}' == '1'  ${7}  ${10}
    Should Be Equal As Integers  ${lowest}  ${expected}
    # Both operating points on the PLL, 50 and 125 MHz, are taken.
    ${speeds}=                Read Counter  0x4006410C
    Should Be Equal As Integers  ${speeds}  6
