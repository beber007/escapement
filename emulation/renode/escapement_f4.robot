*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${EXAMPLE}                    ${CURDIR}/../../Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery

*** Keywords ***
Charger Escapement
    [Arguments]               ${binaire}
    Execute Command           path add @${CURDIR}
    Execute Command           include @Escapement_STM32_Timer.cs
    Execute Command           mach create "escapement-f4"
    Execute Command           machine LoadPlatformDescription @escapement_f4.repl
    Execute Command           sysbus LoadELF @${EXAMPLE}/build/${binaire}.elf
    Execute Command           sysbus LoadBinary @${EXAMPLE}/build/${binaire}.bin 0x08000000

*** Test Cases ***
Les trois taches periodiques sont ordonnancees
    [Documentation]           TaskLEDF4 cree trois taches de periodes 100, 200 et 600 tops qui
    ...                       basculent PB13, PB14 et PB15. Le timer bat a 121,95 kHz : les
    ...                       periodes valent 820 us, 1,64 ms et 4,92 ms. Chaque tache doit
    ...                       donc allumer puis eteindre sa sortie dans sa fenetre.
    Charger Escapement        TaskLEDF4

    ${flag1}=                 Create LED Tester  sysbus.gpioPortB.Flag1  defaultTimeout=0.05
    ${flag2}=                 Create LED Tester  sysbus.gpioPortB.Flag2  defaultTimeout=0.05
    ${flag3}=                 Create LED Tester  sysbus.gpioPortB.Flag3  defaultTimeout=0.05

    Start Emulation

    Assert LED State          true   testerId=${flag1}  pauseEmulation=true
    Assert LED State          false  testerId=${flag1}  pauseEmulation=true
    Assert LED State          true   testerId=${flag2}  pauseEmulation=true
    Assert LED State          false  testerId=${flag2}  pauseEmulation=true
    Assert LED State          true   testerId=${flag3}  pauseEmulation=true
    Assert LED State          false  testerId=${flag3}  pauseEmulation=true

L'echo UART repond
    [Documentation]           UARTSimpleEchoF4 renvoie sur USART2 chaque caractere recu, via
    ...                       une tache declenchee par interruption. Prouve que le noyau
    ...                       ordonnance aussi le traitement evenementiel.
    Charger Escapement        UARTSimpleEchoF4

    ${uart}=                  Create Terminal Tester  sysbus.usart2  defaultPauseEmulation=true

    Start Emulation

    Write Line To Uart        escapement  waitForEcho=false
    # L'exemple renvoie les caracteres tels quels, sans ajouter de fin de ligne.
    Wait For Prompt On Uart   escapement  testerId=${uart}
