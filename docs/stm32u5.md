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
| Clocks | `Escapement_Processor.c` | the 16 MHz crystal of the board, the HSE, to 160 MHz through PLL1 (the MSIS of reset, locked on the LSE, if it does not start): voltage range 1 with the EPOD booster, 4 wait states on the flash, a first step through an AHB prescaler of 2, the instruction cache on |
| Kernel timer | `Escapement_Timer.c` | TIM2, 32 bits, counting microseconds and wrapping at 2^30, its compare channel 1 on the next arrival: the 32-bit path of the STM32 port |
| Timer events | `Escapement_TimerEvent.c` | TIM5, 32 bits, free, its compare channel 1 on the next event: the logic of the RP2350 port, an event already due forced through CC1G |
| UART | `Escapement_UART.c` | USART1 on PB6 and PB7, D1 and D0 of the connector, and LPUART1 on PG7 and PG8, to the board's Linux (`/dev/ttyHS1`), 115,200 baud on USART1 and 57,600 on LPUART1, which receives through Stop 2: the driver of the RP2350 port, with no priming, the transmit interrupt of these UARTs reflecting a state; the bytes lost to an overrun are counted |
| Interrupts | `Escapement_Interrupts.c` | the 126 entries of the STM32U575/U585, every one but the reserved routed to the kernel's dispatcher, so that an application takes any interrupt with `OSSetISRDescriptor` |
| From SRAM | `Escapement_RamEntry.S`, `STM32U5_SRAM.ld` | the image in SRAM, started at its first word, which takes the stack and the reset handler from the vector table that follows, aligned on 1024 bytes; `OSInitializeSystemClocks` points VTOR at it |

The examples, in `Examples/uno-q`, are those of the Pico 2 transposed:
`TaskLEDU5`, `UARTEchoU5`, `TestTimerEventU5`, `TaskWrapU5`, `IPCU5`, `TestLPTimerU5`,
`SleepU5` and `SoakU5`, the
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
The endurance test has run for hours since, restarted at each commit the bench checks:
over the night of 2026-09-26 to 27, on 52440a3, 9.3 hours, 31 wraps of the kernel clock,
no error, no restart, the pulse at most 48 µs late and the timer events 49 µs, and 21 MB
from Linux on the link without an error.
The first load found a defect Renode could not show: the update event that loads TIM2's
prescaler raised its flag a few timer cycles after the write, past the clear that
followed at once, and the overflow it stood for reached the kernel before the timer had
started, which stopped on its overload check. With `URS` set, only a wrap of the counter
raises it (`Escapement_Timer.c`).

Under Renode, on a platform of our own (`emulation/renode/escapement_u5.repl`, see
`emulation.md`), with the pins of the UNO Q since 2026-09-26, the ten tests of
`escapement_u5.robot` pass under each of the four builds: the probe task every
millisecond, the three periodic tasks, the UART echo, the timer events, the 2^30 wrap of
the kernel clock, the tasks preempting one another inside the FIFO queue and a slot
buffer, R8-R11 kept across, and 3.5 s of the endurance test with every part active and
none in error, LPTIM1's count against TIM2's, and the idle task sleeping on LPTIM1,
though never in Stop 2 there: the platform does not report it (PWR_SR.STOPF), and the
clock is not restarted. The same across the 2^30 wrap: `SleepWrapU5`, `SleepU5` with its
times and margins a thousand times longer on TIM2, TIM5 and LPTIM1 a thousand times
faster, crosses it twice in 2.5 s, none late, every start within 0.5 µs of its period,
every timer event within 10.5 µs of its time, the start of the event task not being
faster (2026-09-27). The endurance test found the port routing to the dispatcher only
the interrupts of its own drivers, and TIM3's to the trap of an undefined one; every
interrupt of the chip now reaches it. The compiled order of the slot buffers and of the
task-level stores holds (`tools/check_order.py`), and the static analysis finds nothing.
The CI runs all of it.

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

The accuracy of the clock was measured against Linux's own, kept by NTP. At first from
the seconds `SoakU5` reports and the times `tools/soak.py` receives them: with the MSIS
running free, the kernel counted 0.48 % too many seconds, +4,800 ppm over 2,564 s on
2026-09-26, and +4,300 to +4,800 ppm over four shorter runs that day. Locked then on the
32.768 kHz crystal of the board (MSIPLLEN), as Arduino's firmware has it, it counted
1,140 s in 1,140 s the same day, which is all whole seconds over 19 minutes can tell, and
4,579 s in 4,583 over the next hour and a quarter: some 800 ppm slow.

`tools/unoq_drift.py` says it closer, timing on Linux the arrival of each report, one at
the start of each second of the kernel's: the MSIS locked, a second of the kernel's
lasted 653 ppm too long (2026-09-26, 100 s, standard error 2 ppm). The datasheet says
why: in PLL mode the MSI runs at a whole multiple of 32,768 Hz, 3.998 MHz in range 4
(DS13086 rev. 10, table 83), 576 ppm short of 4 MHz, the rest the LSE's own error; and
no whole prescaler makes a microsecond of it. PLL1 now takes the 16 MHz crystal of the
board, the HSE, which divides into one exactly: the same evening the second lasted
0.1 ppm too little over 300 s, and 22.5 ppm too long over 600 s. The two disagree by more
than their standard errors (2.4 and 0.4 ppm) say: the reference moved, Linux's clock
pulling in an offset of 35 ms from NTP meanwhile. Within some 25 ppm, then, which is as
close as that reference tells, and what a crystal gives. Over the 9.2 hours of the night
after, the seconds `SoakU5` counted were as many as Linux's to the second, and a line
through the 543 readings gives 21 ppm slow, its standard error 1.7 ppm; Linux's clock,
kept by systemd-timesyncd, was itself 61 ms off NTP with 54 ms of jitter that morning,
and within those 25 ppm the U5's and Linux's cannot be told apart. The MSIS stays
PLL1's input should the HSE not start.

LPTIM1, on the 32.768 kHz crystal (`Escapement_LPTimer.c`), is the first step of an idle
task in Stop 2 (`roadmap.md`). `TestLPTimerU5` sums, every 250 ms, the ticks it counted
and the microseconds TIM2 counted, and arms a compare some 10 ms ahead that the next
instance finds raised. On the board, on 2026-09-27, 983,025 ticks in 30,000,000 µs: the
LSE 15 ppm slow against the HSE, within what either crystal gives, and no compare missed
in 120 instances.

The idle task enters Stop 2 once the application calls `OSInitStop2`
(`Escapement_Stop2.c`): when the next event, an arrival or wrap of TIM2 or a timer event
of TIM5, is 5 ms off or more, no UART sends and USART1 does not receive, it arms LPTIM1
3 ms short of the event, stops TIM2 and TIM5 on an edge of LPTIM1 and enters Stop 2; on
waking it takes the clock back to 160 MHz and starts both again on an edge, moved on by
the ticks counted between. Until 2026-09-27 a pending timer event kept it in Sleep.
`SleepU5` runs one task every 100 ms under it. On the board, on 2026-09-27, over 43 s:
388 entries into Stop 2 for 430 instances, the others those that sent a report on
LPUART1; each instance started 100,000 µs after the one before, to the microsecond; the
longest wake-up took 29 ticks, 885 µs, against the 3 ms allowed; none woke past its
event; and TIM2 counted 43,000,000 µs where LPTIM1 counted 1,409,021 ticks, 2.1 ppm
more, some 0.2 µs a sleep. What it saves is for the PPK2 to say: built with `make
PHASES=30`, `SleepU5` alternates 30 s in Stop 2, D13 high, and 30 s in Sleep, D13 low
(`OSAllowStop2`), the load the same, for the PPK2 to record both with D13 on its digital
input; the CI builds that image at each commit, among the board's
(`tools/board_images.sh`, `ppk2_u5/SleepU5-phases30.elf`). With 5 s phases on the board,
the same day: nine entries into Stop 2 a second in the one, none in the other, every
start on its period in both; D13 itself not yet observed.

Each instance of `SleepU5` also has a timer event wake a second task 40 ms on, since
TIM5 is carried through Stop 2. On the board, on 2026-09-27, over 60 s: 1,160 entries
into Stop 2 for 610 instances, two a period, before the event and before the next
instance; each event came within 5 µs of the time it was due; the longest wake-up
19 ticks; none late. The board check runs it (`tools/unoq_sleep.py`).

An image that sleeps in Stop 2 with DBG_STOP cleared leaves the debug port unpowered
most of the time. Loading the next image then failed on an SWD parity error, OpenOCD
connecting while the core slept, and `SleepU5` ran on: the endurance test after it in
the board check of 2dbcece, on 2026-09-27, saw no report and counted restarts.
`tools/unoq_load.sh` now connects with the reset held (`connect_assert_srst`, with
`srst_nogate`), and fails on an error from OpenOCD; three loads in three after
`SleepU5` then succeeded.

LPUART1 receives through Stop 2 since 2026-09-27 (`Escapement_UART.c`). Its kernel
clock is HSI16, which it wakes itself as a byte comes (UESM, autonomous mode, RM0456
67.4.15); the first byte is sampled while HSI16 starts, up to 3.6 µs (DS13086, table 82),
3.8 % of a frame at 115,200 baud, past the 3.41 % the receiver tolerates, 1.9 % at
57,600, hence that rate on LPUART1 and in the tools that read it. Its receive FIFO holds
the bytes that come while the clock is raised again, interrupts masked. Keeping HSI16 on
in Stop 2 instead (HSIKERON) would have kept 115,200 at some 150 µA (table 82), against
20.5 µA for Stop 2 itself, every SRAM retained, at 25 °C (table 56). On the board, the
same day: `SleepU5` over 60 s received 2,761 bytes of 2,761 sent by `tools/unoq_sleep.py`
in bursts of 1 to 32, none out of the count, no overrun, with 2,444 entries into Stop 2;
`SoakU5` at 57,600 baud ran 2 min without error, its link included.

Three hours of it on the board, on 2026-09-27 from 11:18 to 14:18 UTC, in six runs of
30 min at 2673f02, each across a wrap of TIM2 at 2^30: 108,000 instances, 454,487
entries into Stop 2, every start on its period to the microsecond, the longest wake-up
18 ticks, none late, every timer event within 7 µs of its time, 547,645 bytes of 547,645
received from Linux, none out of the count, no overrun, TIM2 7.3 to 7.6 ppm behind
LPTIM1.

The UNO Q cannot show the U585's current alone. Read from its schematics (ABX00162,
dated 2025-10-01, pages 19, 21 and 22) and datasheet on 2026-09-27: the MCU's VDD,
VDDA and VDDUSB sit on PWR_3P3V, made by two buck converters in series from 5 V
(TPS62A02, U2801 and U2802), with no jumper, 0-ohm resistor or test point in between;
the same rail feeds the ANX7625, the 3.3 V side of the Wi-Fi, the level shifters, the
3.3 V pins of the connectors and the power LED, through 330 ohms, some 3 to 4 mA of its
own; VDDIO2, and with it port G and LPUART1, comes from the Qualcomm side's PMIC
(PM4125, VREG_L15A_1P8V). The U585 has no SMPS there (STM32U585AII6, VCAP but no VDD11),
confirming the port's use of the LDO. At the board's 5 V input a PPK2 measures the whole
board, some 0.6 to 0.7 W running and 28 mA with Linux powered off (a user's figures on
the Arduino forum, 2026-05-28); the 20 µA of Stop 2 are lost in it, and only the
difference between Sleep and Stop 2, some mA at the MCU, may show through `make
PHASES=30`'s alternation. The absolute currents are for a NUCLEO-U575ZI-Q, whose board
has a jumper for the MCU's current, JP5 (UM2861, 6.4.5): `Examples/nucleo-u575` builds
`SleepU5` for it from the same sources, its outputs on pins that are no LED, since the
LEDs are on the rail that jumper measures. That board has no HSE fitted as shipped
(UM2861, 6.7), and the port, which falls back to the MSIS locked on the LSE, now gives the
HSE up after one failed start rather than waiting for it at every wake-up from Stop 2.
Its STM32U575ZIT6Q has the SMPS the U585 of the UNO Q lacks, 8.2 against 20.5 µA in
Stop 2 with every SRAM retained at 25 °C by the datasheet (DS13737 rev. 4, tables 54 and
56); the port keeps the LDO, as reset leaves it. Neither the board nor the image has been
tried yet.

## Errata

The errata sheet of the chip, ES0499 (rev. 12, June 2026), was read against the port on
2026-09-26. The UNO Q's STM32U585 is revision U (DBGMCU_IDCODE 0x30076482). One erratum
touches the port, and only the accuracy of its clock, and that only should the HSE not
start:

- **2.2.27, spurious MSI PLL unlock**: the MSI may leave its PLL mode on a failure of the
  LSE it detects wrongly, more likely cold and at a low core voltage; the MSIS then runs
  free again, 0.48 % fast on this board, which the kernel's clock follows only when PLL1
  had to take the MSIS. ST's workaround is taken: the unlock raises
  line 23 of the EXTI and interrupt 125 (RM0456 rev. 7; neither the CMSIS headers of the
  U575/585 nor Zephyr name them), whose handler turns the PLL mode off and on again and
  counts it, `SoakU5` reporting the count. On 2026-09-26 line 23 raised by software on
  the board, the core halted, was handled: counted once, the pending flag cleared, the
  PLL mode on again. The halt itself cost the link a byte, as a halt alone did after.

The port already stands clear of the others that come near it:

| Erratum | Why the port is clear |
|---|---|
| 2.2.3, 2.2.16: LSE unusable at the low and medium-low drives | it sets medium-high, as Zephyr |
| 2.2.26: hang on entering Stop or Standby with the flash prefetching at 4 wait states | the images, the entry into Stop 2 with them, run from SRAM |
| 2.2.1: PC13 toggling disturbs the LSE | neither the port nor Arduino's device tree uses PC13 |
| 2.22.3: LPUART transmitter jitter with a kernel clock 3 to 4 times the baud rate | HSI16 for 57,600 baud, 278 times |
| 2.2.2: MSI slow on leaving Standby or Stop 3 | the port enters Stop 2 alone |
| 2.2.5: hang entering Stop 2 with PLL2, PLL3, HSI48 or SHSI on | the port starts none of them |
| 2.2.11: first read of a cache line after Stop 2 corrupted | the images run from SRAM through the S-bus, which ICACHE does not cache; DCACHE1 is off |
| 2.2.19: HardFault on a wake-up by an SRD peripheral with DBG_STOP set | `OSInitStop2` clears DBG_STOP, which the debugger may set |
| 2.2.22: device locked by a reset in Stop 2 with an SRAM powered down | every SRAM stays powered |
| TIM break and ocref, IWDG in Stop, USART DMA and smartcard, MPU faults | not used |

The core is a Cortex-M33 r0p4 (CPUID 0x410FD214). Arm's own errata notice for it
(SDEN-756493, v9.0) leaves only 1080541 open in that revision, the same as ES0499's 2.1.1,
on the MPU (`architecture.md`).
