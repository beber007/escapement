# The STM32U5 port

`Escapement/CORTEX-Mx/STM32U5`, for the STM32U585 of the Arduino UNO Q (Cortex-M33 at up
to 160 MHz, 2 MB of flash, 768 KB of SRAM; an STM32U575 with cryptography added, which the
port does not use). Begun on 2026-09-25 under Renode, it has run on the UNO Q since
2026-09-26.

## How it is built

The port is written anew in the manner of the RP2350's rather than as one more family of
the inherited STM32 port, whose drivers carry the addresses of five families in
conditional compilation: registers are defined where they are used, the ones the port
needs and no more, from RM0456, the reference manual of the STM32U5, their addresses and
bits as in STMicroelectronics' `cmsis-device-u5` (`stm32u575xx.h`), read but not copied
into the repository.

| Part | File | What it does |
|---|---|---|
| Clocks | `Escapement_Processor.c` | the MSIS of reset, 4 MHz, to 160 MHz through PLL1: voltage range 1 with the EPOD booster, 4 wait states on the flash, a first step through an AHB prescaler of 2, the instruction cache on |
| Kernel timer | `Escapement_Timer.c` | TIM2, 32 bits, counting microseconds and wrapping at 2^30, its compare channel 1 on the next arrival: the 32-bit path of the STM32 port |
| Timer events | `Escapement_TimerEvent.c` | TIM5, 32 bits, free, its compare channel 1 on the next event: the logic of the RP2350 port, an event already due forced through CC1G |
| UART | `Escapement_UART.c` | USART1 on PB6 and PB7, D1 and D0 of the connector, 115200 baud: the driver of the RP2350 port, with no priming, the transmit interrupt of this USART reflecting a state |
| Interrupts | `Escapement_Interrupts.c` | the 126 entries of the STM32U575/U585, every one but the reserved routed to the kernel's dispatcher, so that an application takes any interrupt with `OSSetISRDescriptor` |
| From SRAM | `Escapement_RamEntry.S`, `STM32U5_SRAM.ld` | the image in SRAM, started at its first word, which takes the stack and the reset handler from the vector table that follows, aligned on 1024 bytes; `OSInitializeSystemClocks` points VTOR at it |

The examples, in `Examples/uno-q`, are those of the Pico 2 transposed:
`TaskLEDU5`, `UARTEchoU5`, `TestTimerEventU5`, `TaskWrapU5`, `IPCU5` and `SoakU5`, the
endurance test, where the part the Pico 2 runs between its cores becomes a 4-slot buffer
written by the interrupt, of TIM3 here, and read by a task it preempts, and the
independent watchdog takes the part of the RP2350's. Their outputs
(`BoardU5.h`) are the green of LED3 on PH11 and the blue of LED4 on PH15, lit when low,
and PB13 and PB14, D13 and D12 of the connector. The hard and the soft kernel build,
under EDF and deadline-monotonic scheduling; the power-aware kernel is not ported.

## On the board

The flash of the UNO Q holds Arduino's bootloader and firmware, and the pins of the MCU's
debug port are not brought out: its Linux processor drives them from GPIOs, with an
OpenOCD of Arduino's in `/opt/openocd`. The images therefore run from SRAM, and
`tools/unoq_load.sh` sends one to the board over SSH, loads it and starts it, the flash
left as it was; `--reset` returns the board to Arduino's firmware, as any reset does. The
loader also freezes TIM2, TIM3, TIM5 and the independent watchdog while the debugger
halts the core, so that a halt to read the board neither makes the tasks late nor
restarts it; while the core sleeps in the idle task, the debugger reads the peripherals
as zeros, so a reading halts it first.

## What is verified

On the UNO Q, on 2026-09-26: the clock set-up reaches 160 MHz, PLL1 locked and the
system clock on it, voltage range 1 with the booster ready, and TIM2 counts microseconds;
`TaskLEDU5` runs, the idle task asleep between the rounds; `SoakU5` ran 22 s with every
part active and none in error, its pulse at most 40 µs late and its timer events 23 µs.
The first load found a defect Renode could not show: the update event that loads TIM2's
prescaler raised its flag a few timer cycles after the write, past the clear that
followed at once, and the overflow it stood for reached the kernel before the timer had
started, which stopped on its overload check. With `URS` set, only a wrap of the counter
raises it (`Escapement_Timer.c`).

Under Renode, on a platform of our own (`emulation/renode/escapement_u5.repl`, see
`emulation.md`), with the pins of the UNO Q since 2026-09-26, the seven tests of
`escapement_u5.robot` pass under each of the four builds: the probe task every millisecond, the three periodic tasks, the UART
echo, the timer events, the 2^30 wrap of the kernel clock, the tasks preempting one
another inside the FIFO queue and a slot buffer, R8-R11 kept across, and 3.5 s of the
endurance test with every part active and none in error. The last found the port routing
to the dispatcher only the interrupts of its own drivers, and TIM3's to the trap of an
undefined one; every interrupt of the chip now reaches it. The compiled order
of the slot buffers and of the task-level stores holds (`tools/check_order.py`), and the
static analysis finds nothing. The CI runs all of it.

`tools/soak.sh`, which runs the endurance test on the Pico for weeks, does not drive the
U5 yet: it drives a probe of its own and reads the RP2040's cause of reset, where the U5
takes the board's OpenOCD over SSH, as `tools/unoq_load.sh` does, and RCC_CSR.

What that does not verify: the watchdog of the endurance test is started and reloaded,
and did not restart the emulated board in 3.5 s, but whether Renode's model would
restart it at the end of 3 s without reload was not checked. And the platform
acknowledges every clock request without
checking it, so that a wrong divider or a missing wait would pass; only the board can
say that the clock set-up is right, which it has done for the steps above, not for the
accuracy of the result. The MSIS runs free, within about 1 % of its frequency; locked on
the 32.768 kHz crystal of the board (MSIPLLEN), as Arduino's firmware has it, it would be
far closer, which timings measured on the board will want (`roadmap.md`).
