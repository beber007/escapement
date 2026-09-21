*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/RP2040/Examples/pico

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
