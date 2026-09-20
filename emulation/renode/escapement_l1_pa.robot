*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32/Examples/stm32l-discovery-pa

*** Test Cases ***
La variante power-aware ordonnance ses trois taches
    [Documentation]           Memes taches que stm32l-discovery et meme charge de 90%, mais
    ...                       chaque tache declare son temps d'execution au pire cas. Le
    ...                       noyau s'en sert pour abaisser la frequence coeur. Les periodes
    ...                       valent 500, 1000 et 3000 tops sur un timer a 312,5 kHz.
    Execute Command           path add @${CURDIR}
    Execute Command           include @Escapement_STM32_Timer.cs
    Execute Command           mach create "escapement-l1-pa"
    Execute Command           machine LoadPlatformDescription @escapement_l1_pa.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/TaskLEDPA.elf
    Execute Command           sysbus LoadBinary @${EXAMPLE}/build/TaskLEDPA.bin 0x08000000

    ${flag1}=                 Create LED Tester  sysbus.gpioPortB.Flag1  defaultTimeout=0.1
    ${flag2}=                 Create LED Tester  sysbus.gpioPortB.Flag2  defaultTimeout=0.1
    ${flag3}=                 Create LED Tester  sysbus.gpioPortB.Flag3  defaultTimeout=0.1

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true
