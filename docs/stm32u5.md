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
| Clocks | `Escapement_Processor.c` | the MSIS of reset, 4 MHz, locked on the LSE, to 160 MHz through PLL1: voltage range 1 with the EPOD booster, 4 wait states on the flash, a first step through an AHB prescaler of 2, the instruction cache on |
| Kernel timer | `Escapement_Timer.c` | TIM2, 32 bits, counting microseconds and wrapping at 2^30, its compare channel 1 on the next arrival: the 32-bit path of the STM32 port |
| Timer events | `Escapement_TimerEvent.c` | TIM5, 32 bits, free, its compare channel 1 on the next event: the logic of the RP2350 port, an event already due forced through CC1G |
| UART | `Escapement_UART.c` | USART1 on PB6 and PB7, D1 and D0 of the connector, and LPUART1 on PG7 and PG8, to the board's Linux (`/dev/ttyHS1`), 115200 baud: the driver of the RP2350 port, with no priming, the transmit interrupt of these UARTs reflecting a state; the bytes lost to an overrun are counted |
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
loader also freezes TIM2, TIM3 and TIM5 while the debugger halts the core, so that a
halt to read the board does not make the tasks late; the independent watchdog's bit in
that register reads back 0, and the watchdog runs on, so a halt must stay short. While
the core sleeps in the idle task the debugger reads zeros, in SRAM as in the
peripherals: a reading halts it first.

The endurance test does without the debugger once it runs: `SoakU5` sends its counts to
Linux on LPUART1 every second, and `tools/soak.py uno-q`, run on the board's Linux as a
service, reads them there and logs them at every interval. The other way, it sends the
MCU a count of bytes in bursts of random length at random times, interrupts that Linux
adds at moments of its own, each byte checked against the one before; a byte lost or
wrong is an error. Arduino's Bridge, which holds `/dev/ttyHS1`, is stopped meanwhile.
If the reports stop, the board has restarted, and the script loads the image again.
On 2026-09-26 the link carried every byte sent, 5,570 in 12 s, none lost, while every
part of the test ran without error.

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

`tools/soak.py` runs the endurance test of the Pico and of the UNO Q alike: the same
checks, the counts read over SWD on the one and from the reports of LPUART1 on the other.
`SoakU5` reports the causes of reset its run found in RCC_CSR, whose flags stay set
across resets until cleared, and clears them: each run finds the resets since the one
before it. On 2026-09-26 a halt of 4.5 s, past the watchdog's 3 s, restarted the board;
the image loaded again reported `pin+IWDG`, the watchdog and the reset of the load, and
the link went on without an error, the script now waiting for the new image's first
report before it sends again.

What that does not verify: the watchdog of the endurance test is started and reloaded,
and did not restart the emulated board in 3.5 s, but whether Renode's model would
restart it at the end of 3 s without reload was not checked. And the platform
acknowledges every clock request without
checking it, so that a wrong divider or a missing wait would pass; only the board can
say that the clock set-up is right, which it has done for the steps above.

The accuracy of the clock was measured against Linux's own, kept by NTP, from the seconds
`SoakU5` reports and the times `tools/soak.py` receives them. With the MSIS running free,
the kernel counted 0.48 % too many seconds: +4,800 ppm over 2,564 s on 2026-09-26, and
+4,300 to +4,800 ppm over four shorter runs that day. Locked since on the 32.768 kHz
crystal of the board (MSIPLLEN), as Arduino's firmware has it, it counted 1,140 s in
1,140 s the same day, every part of the test without error: within 1 s, about 900 ppm,
which is all whole seconds over 19 minutes can tell. The long endurance run will say
closer.

## Errata

The errata sheet of the chip, ES0499 (rev. 12, June 2026), was read against the port on
2026-09-26. The UNO Q's STM32U585 is revision U (DBGMCU_IDCODE 0x30076482). One erratum
touches the port, and only the accuracy of its clock:

- **2.2.27, spurious MSI PLL unlock**: the MSI may leave its PLL mode on a failure of the
  LSE it detects wrongly, more likely cold and at a low core voltage; the MSIS then runs
  free again, 0.48 % fast on this board. ST's workaround turns the PLL mode off and on
  again from the interrupt that reports it; neither the CMSIS headers of the U575/585
  nor Zephyr's name that interrupt, and it waits for RM0456 to say which it is. Until
  then the endurance test would show an unlock, its seconds drifting off Linux's.

The port already stands clear of the others that come near it:

| Erratum | Why the port is clear |
|---|---|
| 2.2.3, 2.2.16: LSE unusable at the low and medium-low drives | it sets medium-high, as Zephyr |
| 2.2.26: hang on entering Stop or Standby with the flash prefetching at 4 wait states | the idle task only sleeps (WFI, SLEEPDEEP never set), and the images run from SRAM |
| 2.2.1: PC13 toggling disturbs the LSE | neither the port nor Arduino's device tree uses PC13 |
| 2.22.3: LPUART transmitter jitter with a kernel clock 3 to 4 times the baud rate | 160 MHz for 115,200 baud |
| 2.2.2, 2.2.5, 2.2.11, 2.2.19, 2.2.22: exits from and entries to Stop and Standby | the port uses neither |
| TIM break and ocref, IWDG in Stop, USART DMA and smartcard, MPU faults | not used |

The core is a Cortex-M33 r0p4 (CPUID 0x410FD214). Arm's own errata notice for it
(SDEN-756493, v9.0) leaves only 1080541 open in that revision, the same as ES0499's 2.1.1,
on the MPU (`architecture.md`).
