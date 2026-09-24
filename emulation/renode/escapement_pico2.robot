# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The RP2350 port on a platform of our own (escapement_pico2.repl), the checks of
# escapement_pico.robot that its examples allow. The platform acknowledges every clock
# switch and every reset without checking them: these tests say that the kernel runs and
# schedules on a Cortex-M33 with the timer, the UART and the GPIO of the RP2350, not that
# the clocks are programmed right, which only the board can say.
*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/RP2350/Examples/pico2

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware linked into SRAM as a debugger does on the
    ...                       board, and starts it at its first instruction, 0x20000000,
    ...                       which sets up its own stack (Escapement_RamEntry.S).
    [Arguments]               ${binary}
    Execute Command           mach create "raspberry_pico2"
    Execute Command           include @${CURDIR}/Escapement_RP2350_Timer.cs
    Execute Command           include @${CURDIR}/Escapement_RP2350_SIO.cs
    Execute Command           machine LoadPlatformDescription @${CURDIR}/escapement_pico2.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/${binary}.elf
    Execute Command           sysbus.cpu0 PC 0x20000000

*** Test Cases ***
The probe task runs every millisecond
    [Documentation]           TaskLEDPico2 toggles GPIO 4 from a task of period 1000 ticks of
    ...                       the 1 us timer: a 500 Hz square wave, 1 ms high, 1 ms low.
    Load Escapement           TaskLEDPico2

    ${probe}=                 Create LED Tester  sysbus.sio.probe

    Start Emulation

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.001  tolerance=0.02  testerId=${probe}  pauseEmulation=true

The three periodic tasks are scheduled
    [Documentation]           The three other tasks, of periods 10, 20 and 60 ms, each raise
    ...                       their output on GPIO 25, 2 and 3 and lower it before ending.
    Load Escapement           TaskLEDPico2

    ${flag1}=                 Create LED Tester  sysbus.sio.led  defaultTimeout=0.25
    ${flag2}=                 Create LED Tester  sysbus.sio.flag2  defaultTimeout=0.25
    ${flag3}=                 Create LED Tester  sysbus.sio.flag3  defaultTimeout=0.25

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

The UART echo answers
    [Documentation]           UARTEchoPico2 sends every byte received on UART0 back from its
    ...                       interrupt handler, interrupt 33, in the second word of the
    ...                       NVIC's registers.
    Load Escapement           UARTEchoPico2

    ${uart}=                  Create Terminal Tester  sysbus.uart0  defaultPauseEmulation=true

    Start Emulation

    Write Line To Uart        escapement  waitForEcho=false
    Wait For Prompt On Uart   escapement  testerId=${uart}

Timer events wake the event-driven tasks
    [Documentation]           TestTimerEventPico2: GPIO 2 high 1 ms every 5 ms and GPIO 3 high
    ...                       2 ms every 10 ms, the kernel's alarms timing the periods, the
    ...                       event manager on alarm 2 the high times.
    Load Escapement           TestTimerEventPico2

    ${flag1}=                 Create LED Tester  sysbus.sio.flag2
    ${flag2}=                 Create LED Tester  sysbus.sio.flag3

    Start Emulation

    Assert LED Is Blinking    testDuration=0.1  onDuration=0.001  offDuration=0.004  tolerance=0.02  testerId=${flag1}  pauseEmulation=true
    Assert LED Is Blinking    testDuration=0.1  onDuration=0.002  offDuration=0.008  tolerance=0.02  testerId=${flag2}  pauseEmulation=true

Scheduling survives the 2^30 wrap of the kernel clock
    [Documentation]           The kernel counts time modulo 2^30 and shifts every temporal
    ...                       variable back when its counter wraps; the port rebuilds that
    ...                       wrap with ALARM1 of TIMER0, whose 64-bit counter never wraps.
    ...                       As in escapement_pico.robot, the timer runs at 1 GHz and
    ...                       TaskWrapPico2 scales its periods to match, which puts the
    ...                       boundary at 1.07 s of emulated time for an unchanged load.
    ...                       Every task must still run afterwards, and the probe, toggled
    ...                       every 50 ms, must keep its period within 2 %.
    Load Escapement           TaskWrapPico2
    Execute Command           sysbus.timer0 Frequency 1000000000

    ${flag1}=                 Create LED Tester  sysbus.sio.led  defaultTimeout=1
    ${flag2}=                 Create LED Tester  sysbus.sio.flag2  defaultTimeout=1
    ${flag3}=                 Create LED Tester  sysbus.sio.flag3  defaultTimeout=1
    ${probe}=                 Create LED Tester  sysbus.sio.probe

    Start Emulation

    # Before the boundary: the three tasks are running normally.
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true

    # Cross it, and check that the counter did (TIMERAWL of TIMER0).
    Execute Command           emulation RunFor "1.2"
    ${raw}=                   Execute Command  sysbus ReadDoubleWord 0x400B0028
    Should Be True            ${raw.strip()} > 0x40000000

    # After the boundary: every task must still be scheduled, the probe on time.
    Assert LED Is Blinking    testDuration=0.5  onDuration=0.05  offDuration=0.05  tolerance=0.02  testerId=${probe}  pauseEmulation=true
    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true
