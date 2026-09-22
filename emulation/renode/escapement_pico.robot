*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/RP2040/Examples/pico
# How the examples were built, given by renode-test --variable: KERNEL:PA for make
# KERNEL=PA, and UNDERVOLT:1 as well for make KERNEL=PA UNDERVOLT=1.
${KERNEL}                     HARD
${UNDERVOLT}                  0
${CHECK}                      p = r'${CURDIR}'; import sys; p in sys.path or sys.path.insert(0, p); import rp2040_dvfs_check as c

*** Keywords ***
Load Escapement
    [Documentation]           Loads a firmware linked into SRAM the way OpenOCD does on the
    ...                       board: core 0 starts at the first instruction of the image,
    ...                       0x20000000, which sets up its own stack (Escapement_RamEntry.S).
    ...                       Core 1, which the kernel never starts and which sleeps in the
    ...                       bootrom on the board, is halted: a second time domain would
    ...                       make the LED tester time some edges from the wrong core.
    [Arguments]               ${binary}
    Execute Command           $platform_file=@${CURDIR}/escapement_pico.repl
    Execute Command           include @${CURDIR}/rp2040/boards/initialize_custom_board.resc
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

    Start Emulation

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

The power-aware kernel scales the frequency and the voltage
    [Documentation]           Under the power-aware kernel the tasks of TaskLEDPico, which
    ...                       declare their execution times, leave the core idle most of the
    ...                       time, and the kernel lowers the frequency and the voltage whenever
    ...                       the deadlines allow. Write hooks check that clk_sys never runs
    ...                       faster than the core voltage allows (rp2040_dvfs_check.py):
    ...                       the driver raises the voltage before the frequency and lowers it
    ...                       after, which a driver doing either always first fails when
    ...                       undervolted. Over 100 ms the voltage changes 196 times. The
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
    Should Be True            ${changes} >= 100
    ${lowest}=                Read Counter  0x40064104
    ${expected}=              Set Variable If  '${UNDERVOLT}' == '1'  ${7}  ${10}
    Should Be Equal As Integers  ${lowest}  ${expected}
    # Both operating points on the PLL, 50 and 125 MHz, are taken.
    ${speeds}=                Read Counter  0x4006410C
    Should Be Equal As Integers  ${speeds}  6
