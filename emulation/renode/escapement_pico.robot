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

The program of the README blinks the LED
    [Documentation]           ReadmeExample, the program the README shows, which the Makefile
    ...                       builds from the page itself: GPIO 25 toggled every 0.5 s, the LED
    ...                       blinking at 1 Hz. It is written for the hard kernel, and the
    ...                       Makefile builds it for that one only.
    ${built}=                 Evaluate  os.path.exists(r"${EXAMPLE}/build/ReadmeExample.elf")  modules=os
    Skip If                   not ${built}  built for the hard kernel only
    Load Escapement           ReadmeExample

    ${led}=                   Create LED Tester  sysbus.gpio.led

    Execute Command           emulation RunFor "0.1"

    Assert LED Is Blinking    testDuration=3  onDuration=0.5  offDuration=0.5  tolerance=0.02  testerId=${led}  pauseEmulation=true

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

Two tasks send on the UART at once
    [Documentation]           UARTSendersPico, as UARTSendersPico2 in escapement_pico2.robot: a
    ...                       task of 10 ms sends lines of 15 "a" for 6 ms of each period, a
    ...                       task of 500 us a line of 7 "b", preempting it. Over 0.3 s every
    ...                       line on UART0 must be whole, and every line queued come out but
    ...                       those still queued at the end (2026-09-29).
    Load Escapement           UARTSendersPico
    ${out}=                   Evaluate  __import__('tempfile').mktemp(suffix='.txt')
    Execute Command           sysbus.uart0 CreateFileBackend @${out} true
    Execute Command           emulation RunFor "0.3"
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${a}=                     Execute Command  sysbus ReadDoubleWord ${results + 4}
    ${b}=                     Execute Command  sysbus ReadDoubleWord ${results + 8}
    ${a}=                     Convert To Integer  ${a.strip()}
    ${b}=                     Convert To Integer  ${b.strip()}
    ${lines}=                 Evaluate  open(r'${out}', 'rb').read().split(b'\\n')[:-1]
    ${bad}=                   Evaluate  [l for l in $lines if l not in (b'a' * 15, b'b' * 7)]
    ${na}=                    Evaluate  $lines.count(b'a' * 15)
    ${nb}=                    Evaluate  $lines.count(b'b' * 7)
    Log To Console            ${a} lines of a and ${b} of b queued, ${na} and ${nb} whole on the port, ${bad.__len__()} not: ${bad[:3]}
    Should Be True            ${a} > 100 and ${b} > 100
    Should Be Empty           ${bad}
    Should Be True            0 <= ${a} + ${b} - ${na} - ${nb} <= 4

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
    ...                       must come out once and in order, the buffer never give a
    ...                       record twice, and no task find R8-R11 changed by a task that
    ...                       preempted it and ended. An operation preempted in the queue is
    ...                       completed by the task that preempts it: the hooks count the
    ...                       helpers of the queue entered for a descriptor far from the stack
    ...                       pointer, that is, on the stack of the preempted task, and some
    ...                       must be.
    Load Escapement           IPCPico
    Execute Command           python "import System; System.AppDomain.CurrentDomain.SetData('helped', 0)"
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
    ${registers}=             Read Counter  ${results + 36}
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
    [Documentation]           SoakPico, the firmware of the endurance test (tools/soak.py),
    ...                       for 2.5 s: its marker set, its heartbeat past two seconds, every
    ...                       part active and none in error — the pulse on time, the queue
    ...                       in order, the buffers never torn nor repeated, the timer events
    ...                       on time, the heartbeat seeing every part move each second, the
    ...                       interrupt of alarm 3 putting records, and the stacks and the
    ...                       guard words intact.
    [Timeout]                 10 minutes
    Load Escapement           SoakPico
    # Core 1 stays halted under the models, and cannot be launched: the firmware is told
    # to skip the part between the cores, which the RP2350 suite and the board run.
    ${flag}=                  Execute Command  sysbus GetSymbolAddress "SoakLaunchCore1"
    Execute Command           sysbus WriteDoubleWord ${flag.strip()} 0

    Execute Command           emulation RunFor "2.5"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${marker}=                Read Counter  ${results}
    ${seconds}=               Read Counter  ${results + 4}
    ${pulses}=                Read Counter  ${results + 12}
    Should Be Equal As Integers  ${marker}  0x534F414B
    # The kernel starts once core 1 has answered its launch, which under Renode takes a
    # part of the run that varies with the host (from 0 to 1.3 s seen): the pulse is
    # held to the seconds the firmware counted, one per heartbeat from its start.
    Should Be True            ${seconds} >= 2 and ${pulses} >= (${seconds} - 1) * 1000
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Counter  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Counter  ${results + 44 + 4 * ${part}}
        Log To Console        part ${part}: ${activity} done, ${errors} errors
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END

The endurance test drops optional instances, never a mandatory one
    [Documentation]           SoakFirmPico, SoakPico with a (2,5)-firm task (SoakPico.c), for
    ...                       2.5 s: every part active and none in error, as SoakPico's; the
    ...                       firm task's mandatory instances, two of each five, all run, some
    ...                       optional ones run and others are dropped, and no instance ends
    ...                       past its deadline nor bears a number other than the kernel's.
    ...                       The soft kernel only builds it.
    [Timeout]                 10 minutes
    ${built}=                 Evaluate  os.path.exists(r"${EXAMPLE}/build/SoakFirmPico.elf")  modules=os
    Skip If                   not ${built}  built for the soft kernel only
    Load Escapement           SoakFirmPico
    ${flag}=                  Execute Command  sysbus GetSymbolAddress "SoakLaunchCore1"
    Execute Command           sysbus WriteDoubleWord ${flag.strip()} 0

    Execute Command           emulation RunFor "2.5"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${seconds}=               Read Counter  ${results + 4}
    Should Be True            ${seconds} >= 2
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Counter  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Counter  ${results + 44 + 4 * ${part}}
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END
    ${firm}=                  Execute Command  sysbus GetSymbolAddress "Firm"
    ${firm}=                  Convert To Integer  ${firm.strip()}
    ${mandatory}=             Read Counter  ${firm}
    ${optional}=              Read Counter  ${firm + 4}
    ${dropped}=               Read Counter  ${firm + 8}
    ${errors}=                Read Counter  ${firm + 12}
    ${late}=                  Read Counter  ${firm + 16}
    Log To Console            firm: ${mandatory} mandatory, ${optional} optional run, ${dropped} dropped, ${errors} errors, late max ${late} us
    # Two instances of each five mandatory, among all those seen, within the last few.
    ${total}=                 Evaluate  ${mandatory} + ${optional} + ${dropped}
    Should Be True            ${mandatory} > 0 and abs(5 * ${mandatory} - 2 * ${total}) <= 10
    Should Be True            ${optional} > 0 and ${dropped} > 0
    Should Be Equal As Integers  ${errors}  0
    Should Be True            ${late} <= 5000

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

The MPU stops a stack run past its end
    [Documentation]           StackGuardPico overflows the one stack on purpose, 512 bytes a
    ...                       level. The region of the MPU over the 1 KB between the globals
    ...                       and the stack must stop it before it writes any global: Sentinel,
    ...                       the last of them, keeps its value, and the core end in a fault
    ...                       handler: HardFault on the Pico (2026-10-05), MemManage under
    ...                       Renode, which ARMv6-M does not have.
    Load Escapement           StackGuardPico

    Execute Command           emulation RunFor "0.3"

    ${sentinel}=              Execute Command  sysbus GetSymbolAddress "Sentinel"
    ${value}=                 Execute Command  sysbus ReadDoubleWord ${sentinel}
    Should Be Equal As Integers  ${value.strip()}  0xA5A5A5A5
    # Some 260 KB of stack, at under 600 bytes a level: the task went most of the way
    # down, it did not stop early on some other fault. The Pico reached 507 (2026-10-05).
    ${depth}=                 Execute Command  sysbus GetSymbolAddress "Depth"
    ${value}=                 Execute Command  sysbus ReadDoubleWord ${depth}
    Should Be True            ${value.strip()} >= 430
    ${pc}=                    Execute Command  sysbus.cpu0 PC
    ${hardfault}=             Execute Command  sysbus GetSymbolAddress "HardFaultException"
    ${memmanage}=             Execute Command  sysbus GetSymbolAddress "MemManageException"
    Should Be True            ${pc.strip()} in (${hardfault.strip()}, ${memmanage.strip()})
