# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The endurance test under Renode: SoakPico on the RP2040 models, for a long run of
# virtual time, read at every interval as tools/soak.py reads the board. Several run side
# by side, each with its own build and seed (tools/soak_emulated.sh); the seed sets the
# time the Filler works and the delay of the timer events (SoakPico.c). Core 1 cannot be
# launched under the models, so the part between the cores is left out, as in
# escapement_pico.robot.
#
#   renode-test --variable SEED:7 --variable INTERVAL:60 --variable READINGS:1440 \
#       emulation/renode/soak_emulated.robot
*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Resource                      ${RENODEKEYWORDS}
Library                       Collections

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/RP2040/Examples/pico
${SEED}                       0
${INTERVAL}                   60
${READINGS}                   60

*** Keywords ***
Read Counter
    [Arguments]               ${address}
    ${value}=                 Execute Command  sysbus ReadDoubleWord ${address}
    ${value}=                 Convert To Integer  ${value.strip()}
    RETURN                    ${value}

*** Test Cases ***
SoakPico runs without error
    [Timeout]                 NONE
    Execute Command           $machine_name="raspberry_pico"
    Execute Command           include @${CURDIR}/rp2040/cores/initialize_peripherals.resc
    Execute Command           include @${CURDIR}/Escapement_RP2040_Timer.cs
    Execute Command           machine LoadPlatformDescription @${CURDIR}/escapement_pico.repl
    Execute Command           sysbus LoadELF @${CURDIR}/rp2040/bootroms/rp2040/b2.elf
    Execute Command           sysbus Unregister sysbus.timer
    Execute Command           machine LoadPlatformDescription @${CURDIR}/escapement_pico_timer.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/SoakPico.elf
    Execute Command           sysbus.cpu0 VectorTableOffset 0x00000000
    Execute Command           sysbus.cpu1 VectorTableOffset 0x00000000
    Execute Command           sysbus.cpu0 PC 0x20000000
    Execute Command           sysbus.cpu1 IsHalted true
    ${flag}=                  Execute Command  sysbus GetSymbolAddress "SoakLaunchCore1"
    Execute Command           sysbus WriteDoubleWord ${flag.strip()} 0
    ${seed}=                  Execute Command  sysbus GetSymbolAddress "SoakSeed"
    Execute Command           sysbus WriteDoubleWord ${seed.strip()} ${SEED}
    ${results}=               Execute Command  sysbus GetSymbolAddress "Results"
    ${results}=               Convert To Integer  ${results.strip()}

    @{before}=                Create List  0  0  0  0  0  0  0  0
    ${seconds_before}=        Set Variable  0
    FOR  ${reading}  IN RANGE  1  ${READINGS} + 1
        Execute Command       emulation RunFor "${INTERVAL}"
        ${marker}=            Read Counter  ${results}
        ${seconds}=           Read Counter  ${results + 4}
        ${wraps}=             Read Counter  ${results + 8}
        ${line}=              Set Variable  reading ${reading}: ${seconds} s, ${wraps} wraps
        @{now}=               Create List
        FOR  ${part}  IN RANGE  8
            ${activity}=      Read Counter  ${results + 12 + 4 * ${part}}
            ${errors}=        Read Counter  ${results + 44 + 4 * ${part}}
            ${line}=          Set Variable  ${line}, ${activity}/${errors}
            Should Be Equal As Integers  ${errors}  0  part ${part} in error: ${line}
            Should Be True    ${activity} > ${before}[${part}]  part ${part} stopped: ${line}
            Append To List    ${now}  ${activity}
        END
        ${pulse}=             Read Counter  ${results + 76}
        ${event}=             Read Counter  ${results + 80}
        ${stack}=             Read Counter  ${results + 84}
        ${high}=              Read Counter  ${results + 92}
        Log To Console        ${line}, late max ${pulse}/${event} us, stack free ${stack}, load ${high} us
        Should Be Equal As Integers  ${marker}  0x534F414B
        Should Be True        ${seconds} - ${seconds_before} >= ${INTERVAL} - 1
        @{before}=            Copy List  ${now}
        ${seconds_before}=    Set Variable  ${seconds}
    END
