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
# The longest test takes under 30 s on the CI's runners (2026-09-26): an emulator that
# stops answering, as Renode once did there in a first test, fails in 2 minutes rather
# than holding the job until its own timeout.
Test Timeout                  2 minutes
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/RP2350/Examples/pico2
${MONITOR}                    p = r'${CURDIR}'; import sys; p in sys.path or sys.path.insert(0, p); import rp2350_exclusive_monitor as m

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware linked into SRAM as a debugger does on the
    ...                       board, and starts it at its first instruction, 0x20000000,
    ...                       which sets up its own stack (Escapement_RamEntry.S). Core 1
    ...                       waits, halted, until the firmware launches it through the SIO.
    [Arguments]               ${binary}
    Execute Command           mach create "raspberry_pico2"
    Execute Command           include @${CURDIR}/Escapement_RP2350_Timer.cs
    Execute Command           include @${CURDIR}/Escapement_RP2350_SIO.cs
    Execute Command           machine LoadPlatformDescription @${CURDIR}/escapement_pico2.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/${binary}.elf
    Execute Command           sysbus.cpu0 PC 0x20000000
    Execute Command           sysbus.cpu1 IsHalted true

Play The Exclusive Monitor Of The RP2350
    [Documentation]           Hooks the entry of the byte and word LL/SC functions the image
    ...                       holds on both cores, and the start of their exceptions, to
    ...                       rp2350_exclusive_monitor.py, which plays the global monitor of
    ...                       the chip with EXTEXCLALL set instead of Renode's.
    Execute Command           python "${MONITOR}; m.setup('global', 0)"
    FOR  ${size}  ${ll}  ${sc}  IN  1  OSUINT8_LL  OSUINT8_SC  4  OSUINT32_LL  OSUINT32_SC
        ${found}  ${lla}=     Run Keyword And Ignore Error  Execute Command  sysbus GetSymbolAddress "${ll}"
        IF  $found == "PASS"
            ${sca}=           Execute Command  sysbus GetSymbolAddress "${sc}"
            FOR  ${cpu}  IN  cpu0  cpu1
                Execute Command  sysbus.${cpu} AddHook ${lla.strip()} "${MONITOR}; m.ll(self, ${size})"
                Execute Command  sysbus.${cpu} AddHook ${sca.strip()} "${MONITOR}; m.sc(self, ${size})"
            END
        END
    END
    FOR  ${cpu}  IN  cpu0  cpu1
        Execute Command       sysbus.${cpu} AddHookAtInterruptBegin "${MONITOR}; m.exception(self)"
    END

Monitor Count
    [Documentation]           One of the counts rp2350_exclusive_monitor.py keeps.
    [Arguments]               ${key}
    ${value}=                 Execute Command  python "${MONITOR}; print(m.get('${key}', 0))"
    ${value}=                 Convert To Integer  ${value.strip()}
    RETURN                    ${value}

Read Word
    [Documentation]           One 32-bit word of the emulated memory, as an integer.
    [Arguments]               ${address}
    ${value}=                 Execute Command  sysbus ReadDoubleWord ${address}
    ${value}=                 Convert To Integer  ${value.strip()}
    RETURN                    ${value}

*** Test Cases ***
The probe task runs every millisecond
    [Documentation]           TaskLEDPico2 toggles GPIO 4 from a task of period 1000 ticks of
    ...                       the 1 us timer: a 500 Hz square wave, 1 ms high, 1 ms low.
    Load Escapement           TaskLEDPico2

    ${probe}=                 Create LED Tester  sysbus.sio.probe

    # A virtual time run, not Start Emulation: the tester then starts at a set instant
    # of the pattern, not at one the host's speed decides; on the GitHub runners, the
    # first test of a suite once began mid-state and failed (2026-09-25).
    Execute Command           emulation RunFor "0.005"

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

    # A virtual time run, not Start Emulation: the tester then starts at a set instant
    # of the pattern, not at one the host's speed decides; on the GitHub runners, the
    # first test of a suite once began mid-state and failed (2026-09-25).
    Execute Command           emulation RunFor "0.02"

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

The 4-slot buffer crosses between the two cores
    [Documentation]           FourSlotCoresPico2 launches core 1 through the handshake of the
    ...                       bootrom, which the SIO model plays: bare code on core 1 writes
    ...                       records of eight equal words, rising, into a 4-slot buffer and a
    ...                       plain array, and a task on core 0 reads both 32 times a
    ...                       millisecond. Renode runs the two cores by turns, finely enough
    ...                       for the plain array to tear: in 200 ms of emulated time no read
    ...                       of the buffer may mix two records or go backwards, and the plain
    ...                       array, the negative control, must tear at least once.
    Load Escapement           FourSlotCoresPico2

    Execute Command           emulation RunFor "0.2"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${written}=               Read Word  ${results}
    ${reads}=                 Read Word  ${results + 4}
    ${torn}=                  Read Word  ${results + 8}
    ${backwards}=             Read Word  ${results + 12}
    ${plain_torn}=            Read Word  ${results + 24}
    Should Be True            ${written} > 1000
    Should Be True            ${reads} > 5000
    Should Be Equal As Integers  ${torn}  0
    Should Be Equal As Integers  ${backwards}  0
    Should Be True            ${plain_torn} > 0

The 3-slot buffer crosses between the two cores
    [Documentation]           ThreeSlotCoresPico2 is FourSlotCoresPico2 with the 3-slot buffer,
    ...                       whose writer and reader hand a slot over with an LL/SC pair.
    ...                       Renode's own exclusives compare values and stall when both cores
    ...                       touch a reserved location, so the test plays the monitor of the
    ...                       RP2350 instead. No read may mix two records or go backwards, the
    ...                       plain array must tear, and the reader's SC must have failed
    ...                       through the writer's stores, which only that monitor does. A
    ...                       writer that may take the slot being read fails the test, and a
    ...                       reader that tries its SC once stalls it; the races that need the
    ...                       other core between an LL and its SC are the model's
    ...                       (test/model/threeslot.py): Renode seldom switches cores there.
    [Timeout]                 3 minutes
    Load Escapement           ThreeSlotCoresPico2
    Play The Exclusive Monitor Of The RP2350

    Execute Command           emulation RunFor "0.2"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    ${written}=               Read Word  ${results}
    ${reads}=                 Read Word  ${results + 4}
    ${torn}=                  Read Word  ${results + 8}
    ${backwards}=             Read Word  ${results + 12}
    ${plain_torn}=            Read Word  ${results + 24}
    ${reader_failed}=         Monitor Count  sc0.failed
    ${cleared}=               Monitor Count  cleared.write0
    Should Be True            ${written} > 1000
    Should Be True            ${reads} > 5000
    Should Be Equal As Integers  ${torn}  0
    Should Be Equal As Integers  ${backwards}  0
    Should Be True            ${plain_torn} > 0
    Should Be True            ${reader_failed} > 0 and ${cleared} > 0

Tasks preempt one another inside the FIFO queue and a slot buffer
    [Documentation]           IPCPico2: a long task fills a FIFO queue and writes a 3-slot
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
    Load Escapement           IPCPico2
    FOR  ${helper}  ${reg}  IN  FIFOEnqueueHelper  1  FIFODequeueHelper  2
        ${address}=           Execute Command  sysbus GetSymbolAddress "${helper}"
        Execute Command       sysbus.cpu0 AddHook ${address.strip()} "import System; d = System.AppDomain.CurrentDomain; des = int(str(self.GetRegisterUnsafe(${reg})), 0); sp = int(str(self.GetRegisterUnsafe(13)), 0); d.SetData('helped', (d.GetData('helped') or 0) + (1 if des - sp > 64 else 0))"
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
    [Documentation]           SoakPico2, the firmware of the endurance test (tools/soak.py),
    ...                       until its heartbeat is past two seconds: its marker set, every
    ...                       part active and none in error — the pulse on time, the queue
    ...                       in order, the buffers never torn nor repeated, the timer events
    ...                       on time, the heartbeat seeing every part move each second, the
    ...                       interrupt of alarm 3 putting records, and the stacks and the
    ...                       guard words intact.
    [Timeout]                 10 minutes
    Load Escapement           SoakPico2

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    # The kernel starts once core 1 has answered its launch, which under Renode takes a
    # part of the run that varies with the host (from 0 to over 1.5 s seen): the run goes
    # on by half seconds until the firmware has counted two, and the pulse is held to the
    # seconds it counted, one per heartbeat from its start.
    FOR  ${step}  IN RANGE  12
        Execute Command       emulation RunFor "0.5"
        ${seconds}=           Read Word  ${results + 4}
        IF  ${seconds} >= 2  BREAK
    END
    ${marker}=                Read Word  ${results}
    ${pulses}=                Read Word  ${results + 12}
    Should Be Equal As Integers  ${marker}  0x534F414B
    Should Be True            ${seconds} >= 2 and ${pulses} >= (${seconds} - 1) * 1000
    FOR  ${part}  IN RANGE  8
        ${activity}=          Read Word  ${results + 12 + 4 * ${part}}
        ${errors}=            Read Word  ${results + 44 + 4 * ${part}}
        Log To Console        part ${part}: ${activity} done, ${errors} errors
        Should Be True        ${activity} > 0
        Should Be Equal As Integers  ${errors}  0
    END

The queue of Evéquoz crosses between the two cores
    [Documentation]           FIFOCoresPico2 runs the queue between the cores
    ...                       (Escapement_CoreQueue.c) with each core a producer and a
    ...                       consumer of the same two queues, as its model has them
    ...                       (test/model/fifo_mp.py), with the exclusive monitor of the
    ...                       RP2350 played. Each producer writes 500 records; once they are
    ...                       all taken, the two consumers must have taken each record of
    ...                       each producer once, whole, and in its producer's order, and
    ...                       each consumer records of both producers. Renode runs each core
    ...                       for a slice of 1 us here, and some SCs of each core must have
    ...                       failed through the other's stores: at its usual slice the two
    ...                       cores never overlap in the queue.
    [Timeout]                 3 minutes
    Load Escapement           FIFOCoresPico2
    Play The Exclusive Monitor Of The RP2350
    Execute Command           emulation SetGlobalQuantum "0.000001"

    Execute Command           emulation RunFor "0.05"

    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}
    FOR  ${producer}  IN RANGE  2
        ${address}=           Evaluate  ${results} + 4 * ${producer}
        ${written}=           Read Word  ${address}
        Should Be Equal As Integers  ${written}  500
        ${taken}=             Set Variable  ${0}
        ${sum}=               Set Variable  ${0}
        ${squares}=           Set Variable  ${0}
        FOR  ${consumer}  IN RANGE  2
            ${base}=          Evaluate  ${results} + 8 + 32 * ${consumer}
            ${at}=            Evaluate  ${base} + 4 * ${producer}
            ${t}=             Read Word  ${at}
            ${s}=             Read Word  ${at + 8}
            ${q}=             Read Word  ${at + 16}
            Should Be True    ${t} > 0
            ${taken}=         Evaluate  ${taken} + ${t}
            ${sum}=           Evaluate  ${sum} + ${s}
            ${squares}=       Evaluate  ${squares} + ${q}
        END
        Log To Console        producer ${producer}: ${written} written, ${taken} taken
        Should Be Equal As Integers  ${taken}  500
        Should Be Equal As Integers  ${sum}  125250
        Should Be Equal As Integers  ${squares}  41791750
    END
    FOR  ${consumer}  IN RANGE  2
        ${base}=              Evaluate  ${results} + 8 + 32 * ${consumer}
        ${torn}=              Read Word  ${base + 24}
        ${out_of_order}=      Read Word  ${base + 28}
        Should Be Equal As Integers  ${torn}  0
        Should Be Equal As Integers  ${out_of_order}  0
    END
    ${cleared0}=              Monitor Count  cleared.write0
    ${cleared1}=              Monitor Count  cleared.write1
    Should Be True            ${cleared0} > 0 and ${cleared1} > 0
