# The STM32U5 port

`Escapement/CORTEX-Mx/STM32U5`, for the NUCLEO-U575ZI-Q (STM32U575ZI, Cortex-M33 at up to
160 MHz, 2 MB of flash, 768 KB of SRAM). Begun on 2026-09-25, it runs under Renode only:
no board has run it yet.

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
| UART | `Escapement_UART.c` | USART1 on PA9 and PA10, the virtual serial port of the board's ST-LINK, 115200 baud: the driver of the RP2350 port, with no priming, the transmit interrupt of this USART reflecting a state |
| Interrupts | `Escapement_Interrupts.c` | the 126 entries of the STM32U575, those of TIM2, TIM5 and USART1 routed to the kernel's dispatcher |

The examples, in `Examples/nucleo-u575zi-q`, are those of the Pico 2 transposed:
`TaskLEDU5`, `UARTEchoU5`, `TestTimerEventU5`, `TaskWrapU5` and `IPCU5`. Their outputs
(`BoardU5.h`) are the green and blue LEDs of the board, LD1 on PC7 and LD2 on PB7, and
PA5 and PA6, D13 and D12 of the Arduino connector. The hard and the soft kernel build,
under EDF and deadline-monotonic scheduling; the power-aware kernel is not ported.

## What is verified

Under Renode, on a platform of our own (`emulation/renode/escapement_u5.repl`, see
`emulation.md`), on 2026-09-25, the six tests of `escapement_u5.robot` pass under each
of the four builds: the probe task every millisecond, the three periodic tasks, the UART
echo, the timer events, the 2^30 wrap of the kernel clock, and the tasks preempting one
another inside the FIFO queue and a slot buffer, R8-R11 kept across. The compiled order
of the slot buffers and of the task-level stores holds (`tools/check_order.py`), and the
static analysis finds nothing. The CI runs all of it.

What that does not verify: the platform acknowledges every clock request without
checking it, so that a wrong divider or a missing wait would pass; only the board can
say that the clock set-up is right. The MSIS also runs free, within about 1 % of its
frequency; locked on the 32.768 kHz crystal of the board (MSIPLLEN), it would be far
closer, which timings measured on the board will want (`roadmap.md`).
