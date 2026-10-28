# The STM32U5 port

`Escapement/CORTEX-Mx/STM32U5` runs the kernel on the STM32U585 of the Arduino UNO Q.
The chip is a Cortex-M33 at up to 160 MHz with 2 MB of flash and 768 KB of SRAM. The
U585 is an STM32U575 with cryptography added, and the port does not use the
cryptography. Work began under Renode on 2026-09-25. The port has run on the UNO Q since
2026-09-26. The same sources also build `SleepU5` for a NUCLEO-U575ZI-Q, a board on
which the MCU's current can be measured.

This page covers how the port is built and loaded, and what was verified on the board
and under Renode. It also covers the accuracy of the kernel clock, the idle task in Stop
2, the current measurement, and the errata of the chip.

```sh
U5=Escapement/CORTEX-Mx/STM32U5/Examples/uno-q
make -C $U5                                    # hard kernel, EDF
make -C $U5 KERNEL=SOFT                        # soft kernel
make -C $U5 SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
tools/unoq_load.sh $U5/build/SoakU5.elf        # into SRAM over SSH, and start it
tools/unoq_load.sh --reset                     # back to Arduino's firmware
renode-test emulation/renode/escapement_u5.robot
```

## How it is built

The port was written anew, on the model of the RP2350 port. It is not one more family of
the inherited STM32 port. The drivers of that port carry the addresses of five families
in conditional compilation. Each file of this port defines the registers it uses, and
only those the port needs. They come from RM0456, the reference manual of the STM32U5.
Their addresses and bit names are as in STMicroelectronics' `cmsis-device-u5`
(`stm32u575xx.h`). That header was read, not copied into the repository.

| Part | File | What it does |
|---|---|---|
| Clocks | `Escapement_Processor.c` | the board's 16 MHz crystal (HSE) to 160 MHz through PLL1 (80, 40 or 16 with `make MHZ=`), or the MSIS locked on the LSE, raised to its 16 MHz range, if the HSE does not start; voltage range 1 with the EPOD booster, 4 flash wait states, a first step through an AHB prescaler of 2, the instruction cache on |
| Kernel timer | `Escapement_Timer.c` | TIM2, 32 bits, counting microseconds and wrapping at 2^30, compare channel 1 on the next arrival; the 32-bit path of the STM32 port |
| Timer events | `Escapement_TimerEvent.c` | TIM5, 32 bits, free-running, compare channel 1 on the next event; the logic of the RP2350 port, an event already due forced through CC1G |
| UART | `Escapement_UART.c` | USART1 on PB6 and PB7 (D1 and D0 of the connector) at 115,200 baud, with its receive FIFO; LPUART1 on PG7 and PG8 to the board's Linux (`/dev/ttyHS1`) at 57,600 baud by default, 115,200 for `SleepU5` and `SleepWrapU5` behind a wake-up byte and for `SoakU5`, which never enters Stop 2, receiving through Stop 2, and PG6, the CTS of the Linux side, held low: left floating, it read high on 2026-09-28. The RP2350 driver without its priming, since the transmit interrupt of these UARTs reflects a state. Bytes lost to an overrun are counted |
| Interrupts | `Escapement_Interrupts.c` | the 126 entries of the STM32U575/U585, all but the reserved ones routed to the kernel's dispatcher, so that an application can take any interrupt with `OSSetISRDescriptor` |
| Low-power timer | `Escapement_LPTimer.c` | LPTIM1 on the 32.768 kHz crystal (LSE), which counts through Stop 2 |
| Stop 2 | `Escapement_Stop2.c` | the idle task in Stop 2, woken by LPTIM1 (below) |
| From SRAM | `Escapement_RamEntry.S`, `STM32U5_SRAM.ld` | the image starts at its first word, which takes the stack and the reset handler from the vector table that follows, aligned on 1024 bytes; `OSInitializeSystemClocks` points VTOR at that table |

The examples in `Examples/uno-q` are those of the Pico 2, transposed:

- `TaskLEDU5`, `UARTEchoU5`, `TestTimerEventU5`, `TaskWrapU5`, `IPCU5` and `SoakU5`, the
  endurance test;
- `TestLPTimerU5`, `SleepU5` and `Stop2EventWrapU5`, for Stop 2;
- `StackGuardU5`, a stack that overflows on purpose (roadmap, "A stack that faults");
- `SoakFirmU5`, built for the soft kernel only, which is `SoakU5` with an (m,k)-firm
  task.

In `SoakU5`, the part that the Pico 2 runs between its two cores becomes a 4-slot
buffer. The interrupt of TIM3 writes the buffer, and a task that this interrupt preempts
reads it. The independent watchdog stands in for the RP2350's watchdog. The examples
drive four outputs (`BoardU5.h`):

- the green of LED3 on PH11 and the blue of LED4 on PH15, both lit when low;
- PB13 and PB14, which are D13 and D12 of the connector.

The hard and the soft kernel build under EDF and deadline-monotonic scheduling. The
power-aware kernel is not ported.

## On the board

### Loading into SRAM

The flash of the UNO Q holds Arduino's bootloader and firmware. The debug pins of the
MCU are not brought out: the board's Linux processor drives them from GPIOs, with an
OpenOCD of Arduino's in `/opt/openocd`. The images therefore run from SRAM.
`tools/unoq_load.sh` sends an image to the board over SSH, loads it and starts it. It
leaves the flash as it was. With `--reset`, it returns the board to Arduino's firmware,
as any reset does.

The loader also freezes TIM2, TIM3 and TIM5 while the debugger halts the core
(DBGMCU_APB1FZR1), so that halting the board to read it does not make the tasks late.
The independent watchdog's bit in that register reads back 0. The watchdog therefore
keeps running, and a halt must stay short. While the core sleeps in the idle task, the
debugger reads zeros from SRAM and peripherals alike, so a reading halts the core first.

### The endurance test

Once `SoakU5` runs, the debugger is no longer needed. The image sends its counts to
Linux on LPUART1 every second. `tools/soak.py uno-q` runs as a service on the board's
Linux. It reads the counts and logs them at every interval. In the other direction, it
sends the MCU a count, one byte after the other, in bursts of random length at random
times, so that the interrupts come at moments Linux chooses. The MCU checks each byte
against the one before. A byte lost or wrong is an error. Arduino's Bridge, which holds
`/dev/ttyHS1`, is stopped meanwhile. If the reports stop, the board has restarted, and
the script loads the image again. On 2026-09-26 the link carried every byte sent, 5,570
in 12 s with none lost, while every part of the test ran without error.

The same script runs the Pico's endurance test with the same checks. On the Pico it
reads the counts over SWD. `SoakU5` also reports the causes of reset it found in
RCC_CSR, then clears them. These flags survive resets until cleared, so each run sees
the resets since the run before it. On 2026-09-26 a halt of 4.5 s, longer than the
watchdog's 3 s, restarted the board. The image, loaded again, reported `pin+IWDG` (the
watchdog, and the reset of the load). The link went on without an error. The script now
waits for the new image's first report before it sends again.

An image that the long run reuses has already counted the bytes sent to it before. Each
board check runs the endurance test for 2 min. Then the long run continues with the same
image. Until 2026-10-01, the log of the long run read some 78,000 bytes more received
than sent, the same offset all along. The script now starts its count of bytes sent from
the image's count, each time it continues an image.

On a NUCLEO-U575ZI-Q, the link goes over USART1 to the ST-LINK at 115,200 baud. At
00adc77, USART1 ran without its FIFO, so each byte had to be read within one frame, 87
µs. On 2026-09-29, after 62,768 s (17 h 26 min), the run lost one byte to an overrun. It
counted the next byte as out of the count. The result was 1 overrun and 1 error of the
link, while every count of the kernel stayed at 0. What delayed the interrupt that once
cannot be told afterwards. LPUART1 on the UNO Q, with its FIFO of 8 bytes at 57,600
baud, had lost no byte in 69,000 s. USART1 now runs with its FIFO too, which allows some
700 µs: at 73a6d83, on 2026-09-30, it had carried 54 MB in 84,218 s (23 h 23 min) with
no overrun and no error.

LPUART1 lost its first byte on 2026-10-01, at 1b67f30, after 2 h 20 min. The counts read
1 overrun, 1 error of the link, and every count of the kernel at 0. Before that, the UNO
Q had run some 300,000 s of endurance without one loss. The FIFO of LPUART1 allows 1.4
ms, yet the pulse, due every millisecond, was never more than 47 µs late: no interrupt
was held off that long. Nothing halted the core either: there was no OpenOCD and no
login on the board at that minute. The counts do not rule out one case: the core and its
timers stopped together, as under a debugger's halt, while the LPUART, on HSI16, went on
receiving. Since then `SoakU5` keeps the longest burst of the link, the bytes read
within 5 µs of each other. With the burst, it keeps the time that the kernel's clock
counted since the byte before them. A burst of n bytes waiting came over at least n − 1
byte times, 174 µs each at 57,600 baud. If the clock counted less than that,
`tools/soak.py` says how much the clock lost.

The clock lost nothing: the losses came from the driver. `OSEnqueueUART`, called by a
task, masked the interrupt of its USART in the NVIC around the write of `TXEIE` in CR1.
The interrupt writes CR1 too. A task preempted between the mask and the unmask left the
receiver unread for as long as the tasks that preempted it ran, the timer's interrupt
and the pulse on time. In the endurance test, the heartbeat sends the report twice a
second. The heartbeat has the furthest deadline, so the long task, up to 6 ms, may run
in its place. On 2026-10-09 the soft kernel's run on the NUCLEO lost a byte after 23 h
46 min, in a burst of 10 bytes read together. The hard kernel's run on the UNO Q lost
one after 16 h 54 min, in a burst of 9. The pulse was at most 56 and 48 µs late. A
window widened to some 75 µs of work between the mask and the unmask lost 2 bytes in the
first minute, with the pulse at most 41 µs late. `OSEnqueueUART` now sets the interrupt
pending, and the interrupt alone writes CR1, as the RP2350 port has done since
2026-09-29. The STM32U3 port, written from this one, had the same fault.

## What is verified

### On the board

On the UNO Q on 2026-09-26, the clock set-up reached 160 MHz. PLL1 locked, with the
system clock on it. Voltage range 1 was set with the booster ready, and TIM2 counted
microseconds. `TaskLEDU5` ran, with the idle task asleep between rounds. `SoakU5` ran
for 22 s with every part active and none in error. Its pulse was at most 40 µs late, and
its timer events at most 23 µs.

The endurance test has run for hours since. It is restarted at each commit that the
bench checks. Over the night of 2026-09-26 to 27, on 52440a3, it ran 9.3 hours across 31
wraps of the kernel clock with no error and no restart. The pulse was at most 48 µs late
and the timer events at most 49 µs. Over the link, 21 MB came from Linux without an
error.

The first load found a defect that Renode could not show. The update event that loads
TIM2's prescaler raised its flag a few timer cycles after the write. By then, the clear
that followed the write had already run. The kernel took that flag for an overflow
before the timer had started, and stopped on its overload check. With `URS` set, only a
wrap of the counter raises the flag (`Escapement_Timer.c`).

On 2026-10-04 `StackGuardU5` overflowed the one stack on purpose. MSPLIM was set to the
end of the globals, as the port has done since that day. The core faulted on a stack
overflow (CFSR 0x00100000, STKOF), with the stack pointer above the globals. Built
without MSPLIM, the stack went into the globals, and the core faulted on an undefined
instruction (roadmap, "A stack that faults"). Renode does not model MSPLIM.

### Under Renode

Renode runs the port on a platform of the project's own
(`emulation/renode/escapement_u5.repl`, see [`emulation.md`](emulation.md)). The
platform has had the pins of the UNO Q since 2026-09-26. The tests of
`escapement_u5.robot` pass under each of the four builds: hard and soft kernel, under
EDF and deadline-monotonic. They cover:

- the probe task every millisecond, the three periodic tasks, the UART echo and the
  timer events;
- the 2^30 wrap of the kernel clock;
- tasks preempting one another inside the FIFO queue and a slot buffer, with R8-R11 kept
  across;
- 3.5 s of the endurance test with every part active and none in error;
- LPTIM1's count against TIM2's, and the idle task sleeping on LPTIM1, up to the wrap
  when the next arrival lies beyond it;
- the wake-up from Stop 2 taking PLL1 back to the HSE, a wake-up that the HSE misses
  going on the MSIS, and the path without the HSE;
- LPUART1 through Stop 2: the window after a byte, and a wake-up byte that comes out
  wrong, dropped with the frames it spoils;
- `SleepU5` built for the NUCLEO, from its SRAM and from its flash;
- the endurance test built for the NUCLEO-U575ZI-Q reporting on USART1, the ST-LINK's
  virtual COM port, and not on LPUART1 (`escapement_u5_nucleo.repl`, which adds the
  ports A and F of that board's pins).

The idle task never reaches Stop 2 there. The platform does not report it
(PWR_SR.STOPF), and the clock is not restarted. `SleepWrapU5` takes the idle task across
the 2^30 wrap. It is `SleepU5` with its times and margins a thousand times longer, and
with TIM2, TIM5 and LPTIM1 clocked a thousand times faster. It crosses the wrap twice in
2.5 s with no instance late (2026-09-27). Every start came within 0.5 µs of its period.
Every timer event came within 10.5 µs of its time: the start of the event task is not a
thousand times faster.

An audit of the port found on 2026-09-29 that the idle task could sleep past the wrap.
An event-driven task signalled before its period is over waits for the period's end in
the arrival queue. Its time is kept whole, and that time may lie beyond the wrap. The
kernel arms TIM2's compare with it, above the counter's range, where it never matches.
The idle task took that compare for the next event and slept on past the wrap in steps
of LPTIM1. It then set TIM2 back to just before the wrap: the kernel's clock lost the
overshoot, up to a whole sleep, 2 s on the board, and counted nothing late.
`Stop2EventWrapU5` shows it under Renode. There TIM2 and TIM5, both moved on by LPTIM1
after each sleep, drift apart by some 46 µs a period, the same within 1.5 µs every
period. Over the period across the wrap, they drift apart by 1.55 ms more. The idle task
now sleeps no further than the wrap when the compare lies beyond it
(`Escapement_Stop2.c`). That period now falls in with the others.

The endurance test found that the port routed to the dispatcher only the interrupts of
its own drivers. The port sent TIM3's interrupt to the trap for an undefined one. Every
interrupt of the chip now reaches the dispatcher. The compiled order of the slot buffers
and of the task-level stores holds (`tools/check_order.py`), and the static analysis
finds nothing. The CI runs all of it.

### What this does not verify

The endurance test starts and reloads the watchdog, and the emulated board did not
restart in 3.5 s. It was not checked whether Renode's model would restart the board
after 3 s without a reload. The platform also acknowledges every clock request without
checking it, so a wrong divider or a missing wait would pass. Only the board can show
that the clock set-up is right. The board has shown it for the steps above.

## Accuracy of the kernel clock

The clock was measured against Linux's own clock, which NTP keeps. The first
measurements compared the seconds that `SoakU5` reports with the times at which
`tools/soak.py` receives them. With the MSIS running free, the kernel counted 0.48 % too
many seconds: +4,800 ppm over 2,564 s on 2026-09-26, and +4,300 to +4,800 ppm over four
shorter runs that day. Locked on the board's 32.768 kHz crystal (MSIPLLEN), as Arduino's
firmware has it, the kernel counted 1,140 s in 1,140 s the same day. That is all that
whole seconds over 19 minutes can tell. Over the next hour and a quarter it counted
4,579 s in 4,583, some 800 ppm slow.

`tools/unoq_drift.py` measures more closely. It times on Linux the arrival of each
report, sent at the start of each of the kernel's seconds. With the MSIS locked, a
second of the kernel's lasted 653 ppm too long (2026-09-26, 100 s, standard error 2
ppm). The datasheet explains why. In PLL mode the MSI runs at a whole multiple of 32,768
Hz. In range 4 that is 3.998 MHz (DS13086 rev. 10, table 83), which is 576 ppm short of
4 MHz. The LSE's own error makes up the rest. No whole prescaler makes a microsecond of
that frequency.

PLL1 now takes the board's 16 MHz crystal, the HSE, which divides into a microsecond
exactly. The same evening, the second lasted 0.1 ppm too little over 300 s, and 22.5 ppm
too long over 600 s. The two results differ by more than their standard errors (2.4 and
0.4 ppm) allow, because the reference moved: Linux's clock took in an offset of 35 ms
from NTP meanwhile. So the rate is known within some 25 ppm. That is as close as that
reference tells, and it is what a crystal gives. Over the 9.2 hours of the following
night, the seconds that `SoakU5` counted matched Linux's to the second. A line through
the 543 readings gives 21 ppm slow, with a standard error of 1.7 ppm. That morning
Linux's clock, kept by systemd-timesyncd, was itself 61 ms off NTP with 54 ms of jitter.
Within those 25 ppm, the U5's clock and Linux's cannot be told apart.

### Without the HSE

Should the HSE not start, the MSIS remains PLL1's input. Until 2026-09-30 that input was
the MSIS's range 4, 3.998 MHz. That is under the 4 MHz that RM0456 gives as the floor of
both the VCO's input and the booster's clock (RCC_PLL1CFGR). The port now raises the
MSIS to range 2. Its MSIRC0 runs at 1,465 periods of the LSE, 48.00512 MHz, divided by
3: 16.0017 MHz. Through its prescaler of 2, the booster's clock is 8.0009 MHz. Through M
= 3, the VCO's input is 5.3339 MHz. After x 60 / 2, the clock is 160.017 MHz. By the
datasheet's figures that is 107 ppm fast, where range 4 gave 576 ppm slow. After each
wake-up, the MSI takes up to 0.8 ms to come within 1 % of its frequency again. Meanwhile
it runs as in MSI mode: within some 1.4 % at 30 °C and 3 V, and a further −4 to +2 %
over temperature (DS13086 rev. 10, table 83). At 5.4 % slow, both inputs stay within
their ranges, at 7.6 and 5.05 MHz. Range 3 divided by 3, 4.0004 MHz, would not. The SRAM
that the images run from reads at 0 wait states up to 16 MHz only in voltage range 4
(RM0456, table 47). The chip starts and wakes from Stop 2 in range 4. One wait state is
set before the MSIS goes to 16 MHz and before each Stop 2, and it is taken off in
range 1. The first version of this change read the 1 % as a bound during the lock, and
it missed the SRAM. An independent review found both faults the same day. The system
clock itself may run a few % above its 160 MHz until the MSI is locked, as it could with
range 4. `escapement_u5.robot` checks the registers on a platform without the HSE
(`escapement_u5_nohse.repl`).

The NUCLEO-U575ZI-Q was taken for a board without the HSE. Its endurance runs since
2026-09-30 (7d069ea) were taken for a check of this path on the board. On 2026-10-02 its
RCC read the HSE ready and PLL1 on it, with M = 4: its crystal X3 is fitted (UM2861,
6.7, leaves it to the variant). Those runs checked the HSE. The MSIS of range 2 had run
under Renode only, on no board. `SleepNoHSEU5` is `SleepU5` built with `OS_NO_HSE`
(`make NOHSE=1` for any image). It starts as a board without the HSE would: PLL1 takes
the MSIS of range 2 at start and at every wake-up from Stop 2.

A Renode test checks it on the platform that enters Stop 2, and the test fails when the
option does nothing. The board check of each commit runs it on the UNO Q for a minute
after `SleepU5`. Its first run on the board, in the check of d9a32eb on 2026-10-02, was
the first of that path on a chip. It counted 600 instances and 1,225 entries into Stop
2, none late. Every byte of the link was received. The longest wake-up took 4 ticks of
LPTIM1, against 19 when waiting for the HSE. The check still failed, on TIM2 against
LPTIM1. It expected TIM2 106.7 ppm ahead, since PLL1 on the MSIS is that fast, and it
read -4.1. TIM2 is set from LPTIM1 at every wake-up from Stop 2, where the image spends
most of its time. TIM2 runs on PLL1 only while awake. It is now checked as for
`SleepU5`, within 20 ppm.

Since 2026-09-28 the board check times the U5 against `CLOCK_MONOTONIC_RAW`
(`tools/unoq_drift.py`). That clock is the crystal of the board's Qualcomm processor as
it is, which NTP does not pull. That crystal was measured against NTP on 2026-09-29,
from 16:50 to 17:35 UTC. NTP had then been synchronized for hours with a stratum 1
server:

- straight against the server: 266 SNTP exchanges were timed on the raw clock, and the
  shortest of each minute was kept. The raw clock read 0.2 ppm fast, with a standard
  error of 6.3 ppm, too wide to tell a rate that small. The Wi-Fi limits it, with round
  trips of 8 to 470 ms and a median of 210 ms;
- through the correction that NTP applies, which is the rate of `CLOCK_MONOTONIC`
  against the raw clock and needs no network: +0.73 ppm over those 45 minutes. That
  figure includes NTP taking in an offset of a few milliseconds. The frequency that
  systemd-timesyncd had settled on gave +1.19 to +1.42 ppm (`adjtimex`).

Over a longer span the correction tells the crystal more closely, as the offsets that
NTP takes in weigh less. From 17:37 to 20:57 UTC the same day, over 11,989 s of the raw
clock, the clock that NTP disciplines gained 21.09 ms on it. That is 1.76 ppm, within
some 0.5 ppm for the few milliseconds that NTP may be off at either end. So the raw
clock runs slow by some 1.8 ppm. That is a hundred and fifty times less than the check's
bound of 300 ppm.

## The idle task in Stop 2

### The plan, as read on 2026-09-26

This is the plan for Stop 2, as read from RM0456 rev. 7 and ES0499 on 2026-09-26, before
any of it was written. LPTIM1 cannot be the kernel's clock. It has 16 bits and wraps
every 2 s on the LSE, in steps of 30.5 µs. Its counter must be read twice to be trusted.
A new compare must be waited for (CMPOK), after a latency that RM0456 does not quantify
for it (§58.4). So TIM2 stays the kernel's clock while the core runs. When the idle task
finds the next event far enough off, it arms LPTIM1 on the LSE to wake it early and
enters Stop 2. LPTIM1 wakes the chip from Stop 2 (table 599). Stop 2 turns the HSE off
and leaves the chip in range 4. Waking takes the HSE's 2 ms, up to 47 µs for range 1, 50
µs for the booster and 25 to 50 µs for PLL1. So Stop 2 pays only for waits of some
milliseconds. TIM2 is then moved on by the time that LPTIM1 counted, and the idle task
finishes the wait in Sleep. The errata kept in view were these:

- never clear LPTIM1's ENABLE, and reset it through the RCC instead (2.17.1);
- writing DIER clears the flag it enables (2.17.3);
- a HardFault may follow a wake-up by LPTIM1, of the SRD domain, when debugging with
  DBG_STOP (2.2.19).

Renode's STM32L0_LpTimer has the same first registers and was expected to serve.

Two parts of that plan changed on 2026-09-27. The plan kept Stop 2 away while a timer
event was pending, since TIM5 stops with it. That rule came from the definition of Stop,
not from a reading for TIM5 itself. TIM5 is now carried through Stop 2 instead
(06e2e60). The plan also kept Stop 2 away while a UART had to receive, since LPUART1
runs on PCLK3. `SoakU5`, fed by Linux, would then hardly ever have entered Stop 2.
LPUART1 then received through Stop 2 on HSI16, at 57,600 baud, the rate that its start
allows for a first byte (2673f02, "LPUART1 through Stop 2" below). Since 2026-10-02 it
has received at 115,200 baud behind a wake-up byte ("The wake-up byte").

### LPTIM1

LPTIM1, on the 32.768 kHz crystal (`Escapement_LPTimer.c`), was the first step toward an
idle task in Stop 2 ([`roadmap.md`](roadmap.md)). Every 250 ms, `TestLPTimerU5` sums the
ticks that LPTIM1 counted and the microseconds that TIM2 counted. It also arms a compare
some 10 ms ahead, which the next instance must find raised. On the board on 2026-09-27
it read 983,025 ticks in 30,000,000 µs. The LSE thus ran 15 ppm slow against the HSE,
within what either crystal gives. No compare was missed in 120 instances.

### Entering and leaving Stop 2

The idle task enters Stop 2 once the application has called `OSInitStop2`
(`Escapement_Stop2.c`), provided three conditions hold:

- the next event is 5 ms off or more;
- no UART is sending;
- USART1 is not receiving.

That event is an arrival or the wrap of TIM2, or a timer event of TIM5. The idle task
arms LPTIM1 3 ms short of the event. It stops TIM2 and TIM5 on an edge of LPTIM1 and
enters Stop 2. On waking, it takes the clock back to 160 MHz and starts both timers
again on an edge. The timers are moved on by the ticks counted in between. Since
2026-10-03 the idle task first sleeps on the MSIS until 500 µs before the event ("The
margin slept on the MSIS", below). Until 2026-09-27 a pending timer event kept the idle
task in Sleep.

`SleepU5` runs one task every 100 ms over this idle task. On the board on 2026-09-27,
over 43 s:

- there were 388 entries into Stop 2 for 430 instances, and the others were the
  instances that sent a report on LPUART1;
- each instance started 100,000 µs after the one before, to the microsecond;
- the longest wake-up took 29 ticks, 885 µs, of the 3 ms allowed, and none woke past its
  event;
- TIM2 counted 43,000,000 µs where LPTIM1 counted 1,409,021 ticks, which is 2.1 ppm
  more, some 0.2 µs a sleep.

Each instance of `SleepU5` also has a timer event wake a second task 40 ms later, since
TIM5 is carried through Stop 2. On the board on 2026-09-27, over 60 s, there were 1,160
entries into Stop 2 for 610 instances. That is two a period: one before the event and
one before the next instance. Each event came within 5 µs of the time it was due. The
longest wake-up took 19 ticks, and none was late. The board check runs this image
(`tools/unoq_sleep.py`).

### A bound on the HSE's start

Until 2026-10-02 the wake-up waited for the HSE as reset does, some 20 ms with
interrupts masked. It then gave the HSE up for good, with PLL1 on the MSIS from then on.
DS13086 gives the HSE's start as 2 ms typical, and gives no maximum (table 80). On the
board the whole wake-up took at most 29 ticks, 885 µs. The wake-up now waits 64 ticks of
LPTIM1, 1.95 ms (`Escapement_Processor.c`). Past that, PLL1 takes the MSIS of range 2
for that wake-up only, 160.017 MHz, some 100 ppm fast. The next wake-up tries the HSE
again. `SleepU5` counts these wake-ups in word 14 of its results, and
`tools/unoq_sleep.py` shows them. The SRAM's wait state before Stop 2 follows the MSIS's
range rather than the HSE's absence, since a miss leaves the MSIS in range 2.

Under Renode, on a platform that enters Stop 2 (`escapement_u5_stop2.repl`), every
wake-up of `SleepU5` took PLL1 back to the HSE when the HSE started within the bound.
When the HSE never started, every wake-up went on the MSIS in 70 ticks at most, none
late of the 98 allowed. The first wake-up after the HSE started again took PLL1 back to
the HSE. The old wait fails that test. On the board, in the check of 3faaf23 on
2026-10-02, `SleepU5` entered Stop 2 2,239 times in 60 s. The longest wake-up took 19
ticks, and the HSE missed none. All 3,085 bytes sent from Linux were received.

### Loading an image while the core sleeps

An image that sleeps in Stop 2 with DBG_STOP cleared leaves the debug port unpowered
most of the time. Loading the next image then failed on an SWD parity error, because
OpenOCD had connected while the core slept, and `SleepU5` ran on. In the board check of
2dbcece on 2026-09-27, the endurance test that came next saw no report and counted
restarts. `tools/unoq_load.sh` now connects with the reset held (`connect_assert_srst`,
with `srst_nogate`). It fails on any error from OpenOCD. After that change, three loads
out of three that followed `SleepU5` succeeded.

### LPUART1 through Stop 2

LPUART1 has received through Stop 2 since 2026-09-27 (`Escapement_UART.c`). Its kernel
clock is HSI16, which the LPUART wakes by itself when a byte comes (UESM, autonomous
mode, RM0456 67.4.15). The first byte is sampled while HSI16 starts, which takes up to
3.6 µs (DS13086, table 82). At 115,200 baud that is 3.8 % of a frame, past the 3.41 %
that the receiver tolerates. At 57,600 baud it is 1.9 %. LPUART1 and the tools that read
it therefore ran at 57,600 baud, until the wake-up byte below. The receive FIFO holds
the bytes that come while the clock is raised again, with interrupts masked. Keeping
HSI16 on in Stop 2 instead (HSIKERON) would have kept 115,200 baud at some 150 µA (table
82), against 20.5 µA for Stop 2 itself with every SRAM retained, at 25 °C (table 56).

On the board the same day, `SleepU5` received over 60 s all 2,761 bytes that
`tools/unoq_sleep.py` sent in bursts of 1 to 32. None was out of the count, and there
was no overrun, with 2,444 entries into Stop 2. `SoakU5` at 57,600 baud ran 2 min
without error, its link included.

### The window after a byte

Since 2026-10-02, a byte received on LPUART1, or a wake-up from Stop 2 that LPTIM1 did
not cause, keeps the idle task in Sleep for `OS_STOP2_LINK_WINDOW_US` after it. The
default is 20 ms (`Escapement_Stop2.c`). It is the second step of the wake-up byte
([`roadmap.md`](roadmap.md), Done, "LPUART1 at 115,200 baud through Stop 2"): the bytes
that follow the first come while HSI16 runs, so that they could be sampled at 115,200
baud. The window is counted on LPTIM1. LPTIM1's compare ends the window if nothing else
wakes the chip, so that Stop 2 resumes then rather than at the next event. The compare
is written once a window: its write waits some two ticks with interrupts masked, and the
bytes of a message come every 87 µs at 115,200 baud. `SleepU5` counts the sleeps that
the window held in word 15 of its results.

Under Renode, on the platform that enters Stop 2, a byte sent 50 ms into a period woke
the chip from Stop 2. TIM2 ran for the 20 ms after it, then stood still again in Stop 2
before the next period, with none late. A byte every 10 ms for half a second kept the
idle task out of Stop 2 throughout, and all its bytes were received in order. Without
the window, both tests fail. Without the compare at its end, the first test fails.

On the board, in the check of 29348d8 on 2026-10-02, still at 57,600 baud and without a
wake-up byte, `SleepU5` entered Stop 2 1,239 times in 60 s, against 2,239 at 3faaf23.
The window held 2,759 sleeps in Sleep after the bursts from Linux. All 2,785 bytes were
received, none late, and the longest wake-up took 19 ticks as before.

### The wake-up byte

`SleepU5`'s link has run at 115,200 baud since 2026-10-02 (`OS_LPUART1_BAUD_RATE`, set
by the UNO Q's Makefile). So has the link of `SoakU5`, which never enters Stop 2, and so
have its tools. Before each message, `tools/unoq_sleep.py` sends a wake-up byte, 0x00.
It waits 5 ms, more than the 3 ms a wake-up is allowed (`OS_STOP2_WAKE_US`). Then it
sends the message as a frame: 0x00, the bytes and their CRC-16 (CCITT, from 0xFFFF)
encoded with COBS, 0x00. Sampled while HSI16 starts, the wake-up byte may come out as
anything. If it comes out as 0x00, it is an empty frame, and it is ignored. If it comes
out as any other value, it ends at the 0x00 that opens the message, as a frame too
short. That frame is dropped and counted in word 16 of `SleepU5`'s results. A frame of
the message that was dropped would show as bytes missing from the count.

Under Renode, a wake-up byte sent as 0x5A into Stop 2 was dropped. The frame 5 ms after
it was received whole. A frame with its CRC off by one was dropped, and the count went
on in the next frame, with none out of it. Accepting any CRC fails that test.

On the board, in the check of 9ff2e2f on 2026-10-02, `SleepU5` at 115,200 baud received
all 2,655 bytes that `tools/unoq_sleep.py` sent over 60 s, in frames behind their
wake-up bytes. None was out of the count, there was no overrun, and no frame was
dropped. So no wake-up byte came out as a value other than 0x00, or none came out at
all: the counts do not tell these apart. `SleepU5` entered Stop 2 1,230 times, and the
window held 3,875 sleeps. The longest wake-up took 19 ticks, the HSE missed none, and
none was late. One minute is no bound on how often a wake-up byte comes out wrong.

### Faster, awake

Awake, the limit of LPUART1 is its FIFO of 8 frames against the latency of its
interrupt, which the kernel's timer outranks. On 2026-10-02 `SoakU5` ran 10 min at each
of 230,400, 460,800 and 921,600 baud on the board. The images and `tools/soak.py` were
built for each rate by hand from 9a6b1da (`OS_LPUART1_BAUD_RATE`). Each rate carried
some 385,000 bytes of the link, in bursts of up to 64, with none out of the count, no
overrun and no restart. The pulse was at most 45 µs late at every rate. At 921,600 the
FIFO holds 87 µs. The limit lies beyond 921,600, the highest rate tried. And
`tools/soak.py` sends some 640 bytes a second on average, so a stream at full rate is
not tried either.

### Three hours

`SleepU5` ran three hours on the board on 2026-09-27, from 11:18 to 14:18 UTC. It ran in
six runs of 30 min at 2673f02, each across a wrap of TIM2 at 2^30. Over 108,000
instances and 454,487 entries into Stop 2, every start was on its period to the
microsecond. The longest wake-up took 18 ticks, and none was late. Every timer event
came within 7 µs of its time. All 547,645 bytes sent from Linux were received, none out
of the count, with no overrun. TIM2 ran 7.3 to 7.6 ppm behind LPTIM1.

## Measuring the current

The UNO Q cannot show the U585's current alone. The following was read from its
schematics (ABX00162, dated 2025-10-01, pages 19, 21 and 22) and datasheet on
2026-09-27. The MCU's VDD, VDDA and VDDUSB sit on PWR_3P3V. Two buck converters in
series make that rail from 5 V (TPS62A02, U2801 and U2802). There is no jumper, 0-ohm
resistor or test point in between. The same rail feeds:

- the ANX7625;
- the 3.3 V side of the Wi-Fi;
- the level shifters;
- the 3.3 V pins of the connectors;
- the power LED, through 330 ohms, some 3 to 4 mA on its own.

VDDIO2, and with it port G and LPUART1, comes from the PMIC of the Qualcomm side
(PM4125, VREG_L15A_1P8V). The U585 has no SMPS there (STM32U585AII6, VCAP but no VDD11),
which confirms the port's use of the LDO.

A PPK2 at the board's 5 V input measures the whole board. It reads some 0.6 to 0.7 W
running, and 28 mA with Linux powered off (a user's figures on the Arduino forum,
2026-05-28). The 20 µA of Stop 2 are lost in that. Only the difference between Sleep and
Stop 2, some mA at the MCU, may show, and `make PHASES=30` is built for it. `SleepU5`
then alternates 30 s in Stop 2 with D13 high and 30 s in Sleep with D13 low
(`OSAllowStop2`), under the same load. The PPK2 records both, with D13 on its digital
input. The CI builds that image at each commit among the board's images
(`tools/board_images.sh`, `ppk2_u5/SleepU5-phases30.elf`). With 5 s phases on the board
the same day, there were nine entries into Stop 2 a second in one phase and none in the
other, and every start was on its period in both. D13 itself has not yet been observed.

The absolute currents are for a NUCLEO-U575ZI-Q. That board has a jumper for the MCU's
current, JP5 (UM2861, 6.4.5). `Examples/nucleo-u575` builds `SleepU5` for it from the
same sources. Its outputs are on pins that drive no LED, since the LEDs sit on the rail
that the jumper measures. This board has its HSE crystal fitted ("Without the HSE"). The
board's STM32U575ZIT6Q has the SMPS that the UNO Q's U585 lacks. By the datasheet, it
draws 8.2 against 20.5 µA in Stop 2 with every SRAM retained at 25 °C (DS13737 rev. 4,
tables 54 and 56). The port keeps the LDO, as reset leaves it, unless built with `make
SMPS=1`. That option selects the SMPS before the voltage range is raised
(PWR_CR3.REGSEL), so that both can be measured on the same board. The NUCLEO on the
bench had its VDD at 1.8 V, JP4 on [2-3], not at the 3.3 V it ships with (UM2861,
6.4.4.3). OpenOCD read a target voltage of 1.80 V on 2026-10-02, the first time it was
read, and the jumper was found there. It was set back to [1-2], 3.3 V, the same evening,
during the search for the hang below. A current measured at 1.8 V is to be weighed
against the datasheet at the VDD of each table. The CI builds both images at each commit
(`ppk2_u5/SleepU5-nucleo-phases30.elf` and `SleepU5-nucleo-smps-phases30.elf`). The
board has run the endurance test on the bench since 2026-09-28. The PPK2 measured
`SleepU5` from the flash instead (`SleepU5Flash`, below).

### SleepU5 on the NUCLEO-U575ZI-Q

Built for the NUCLEO, `SleepU5` sends its reports on USART1, the virtual COM port of the
ST-LINK, and receives nothing there: USART1 receiving would keep the idle task out of
Stop 2 (`_OSUARTIdle`). `tools/unoq_sleep.py 60 nucleo` reads the reports on the UNO Q.
A Renode test checks that the reports go there and that the idle task still arms LPTIM1.

On the board on 2026-10-02, the image first never sent a report. Two causes were found
in turn:

- JP2, which carries the ST-LINK's reset to the MCU (UM2861), was off. OpenOCD's "reset
  halt" then reset nothing. After it, the core was found in its HardFault handler. The
  image was loaded and started there, at that priority, and locked up at the kernel's
  first exception. `tools/nucleo_load.sh` still reported the load done.
- With JP2 back, the image entered Stop 2 and never woke. OpenOCD reached the debug port
  but could not halt the core. The reset held did not get the core back, and only a
  power-off did. A build that set DBG_STOP ran. The image as it was failed in the same
  way with the SMPS (`make SMPS=1`) and at 3.3 V. The NUCLEO's chip is revision X
  (DBGMCU_IDCODE 0x20016482), the first revision. Its errata were then read too (ES0499
  rev. 12), and none accounts for the hang. The cause was the loader.
  `tools/nucleo_load.sh` runs Debian's OpenOCD 0.12.0, whose `stm32x5x_common.cfg` sets
  DBG_STOP and DBG_STANDBY at each connection. The UNO Q's own OpenOCD clears both.
  `OSInitStop2` cleared DBG_STOP alone, and DBGMCU_CR read 0x6 on the NUCLEO: the chip
  entered Stop 2 with DBG_STANDBY set. RM0456 says that DBG_STANDBY holds off the reset
  (75.2.4) and that a system reset leaves it (75.12.4). With DBG_STOP set, the clocks
  never stop, which is why that build ran.

`OSInitStop2` now clears both. `SleepU5` then ran 60 s on the NUCLEO at 3.3 V, with 600
instances and 1,141 entries into Stop 2. Every start was on its period to the
microsecond, and the events came within 5 µs. TIM2 stayed within 5.6 ppm of LPTIM1, and
none was late. The reset held also got the core back while it slept, with its SRAM
intact. The longest wake-up took 58 ticks of LPTIM1, and 61 to 66 with DBG_STOP set,
against 19 on the UNO Q. The wake-up waits for the HSE, which this board has (above).
This board's HSE starts in some 1.8 ms, where the UNO Q's takes under 0.6. That is under
the 64 ticks, 1.95 ms, that the wake-up allows it before PLL1 goes on the MSIS
(`Escapement_Processor.c`), with little to spare. None missed in these runs, and the
count of `SleepU5` would show a wake-up that did. The chip is revision X. On revision X,
the MSI PLL's unlock line, line 23 of the EXTI, and interrupt 125, which the port
enables for erratum 2.2.27, are reserved (RM0456, tables 118 and 186, notes 2), though
the erratum touches revision X too. The port reads DBGMCU_IDCODE and leaves the line and
the interrupt alone on revision X. Revision X therefore goes without the workaround. A
Renode test on a platform whose IDCODE reads revision X
(`escapement_u5_nucleo_revx.repl`) checks it. The test fails when the line and the
interrupt are enabled regardless.

Both loaders, `tools/nucleo_load.sh` and `tools/unoq_load.sh`, now refuse a load when
the core is in an exception after their "reset halt" (ICSR.VECTACTIVE not 0): the reset
did not reach the core, as with JP2 off, and the image would start inside a handler.

### SleepU5 from the flash, for the PPK2

A current of a few µA in Stop 2 is to be measured after a power cycle, with no debugger
connected since: ST's examples for the NUCLEO-U575ZI-Q ask for a power reset after a
load. A probe once connected leaves the debug domain powered until then, and the DBGMCU
registers survive anything less (RM0456, 75.12.4). An image in SRAM does not survive
that power cycle. `SleepU5Flash.elf` (`Examples/nucleo-u575`) is `SleepU5` linked into
the flash, `STM32U5_FLASH.ld`:

- its vector table is at 0x08000000, where the NUCLEO boots (FLASH_OPTR and NSBOOTADD0R
  read 0x1FEFF8AA and 0x0800007F on 2026-10-03);
- its code and constants are in the flash;
- `_OSResetHandler` copies its data into SRAM.

The UNO Q keeps its images in SRAM, since its flash holds Arduino's firmware. The
NUCLEO's flash as it was on 2026-10-03 is kept outside the repository, to be written
back.

Code run from the flash goes through ICACHE, which the port enables. On revision X, the
NUCLEO's revision, the first fetch from the cache line last used before Stop 2 may read
wrong after Stop 2 (ES0499, 2.2.11): `Stop2Idle` disables ICACHE before its WFI and
enables it again after, which is ST's workaround. An image in SRAM, fetched through the
S-bus, is not cached, and it did not need the workaround.

Under Renode, on the NUCLEO's platform, `SleepU5Flash` starts from the flash, with VTOR
at 0x08000000. It reports on USART1, with its periods and events on time. Renode loads
every segment of the ELF, the data in SRAM included, so the board has to check the copy
from the flash. On the NUCLEO on 2026-10-03, programmed with OpenOCD's `program`, the
image ran from a reset with no debugger: 280 instances, 533 entries into Stop 2, none
late.

### The endurance test from the flash (2026-10-10)

`make FLASH=1` links every example of `Examples/nucleo-u575` into the flash, in
`build-flash/`. `tools/nucleo_flash.sh` writes one only if the flash holds another, and
it counts each write on the bench (roadmap, task 4). `tools/soak.py nucleo` takes such
an image by its entry point, in the flash. It starts the image, and starts it again
after a restart, through `tools/nucleo_flash.sh`, which then writes nothing and resets
the board. On the NUCLEO on 2026-10-10, the run of task 2 ended:

- `SoakFirmU5` (`KERNEL=SOFT`) was written, then found the same at a second call, and
  nothing was written. It ran 10 min from 06:47 UTC, with every count at 0. The latest
  pulse was 48 µs late, and the (m,k)-firm task's latest end came 3,863 µs into its 5
  ms. The link carried 384,379 bytes with no error.
- `SoakU5` of the hard kernel was written, and `soak.py`'s loader started it again
  without a write. It then ran 10 min from 06:58 UTC, with every count at 0. The latest
  pulse was 42 µs late, and the link carried 387,198 bytes with no error.

From SRAM over 24 h, the pulse was up to 56 µs late, and the firm task's end up to 4,016
µs: ten minutes from the flash decide no difference. The release's runs (roadmap, task
14, 8 hours a kernel since 2026-10-10) were then to run from the flash. Two writes were
counted on the bench.

### The NUCLEO's MCU measured with a PPK2 (2026-10-03)

A Nordic PPK2, firmware 1.2.4, is on the UNO Q's hub. `tools/ppk2_nucleo.py` reads it
with IRNAS's ppk2-api 0.9.2. The PPK2 is an ampere meter in place of JP5, the IDD jumper
(UM2861, 6.4.6):

- its VIN is on the pin of JP5 that comes from JP4, the board's 3V3;
- its VOUT is on the pin that goes to the MCU;
- its GND is on the board's ground.

Its logic input D0 is on D13 (CN7, pin 10). `SleepU5` built with `PHASES=30` drives D13
high in its 30 s of Stop 2 and low in its 30 s of Sleep. The PPK2's VCC is on the
board's 3V3 (CN8, pin 7). JP4 is on 3V3, and the MCU is at 3.3 V. VDDA is on the board's
3V3, not behind JP5 (SB54, UM2861 table 9), and it is not in the figures.

The measurement goes in this order:

- `SleepU5Flash` is programmed with JP5 fitted;
- CN1 is unplugged, so that the debug domain that the probe powered goes down;
- the PPK2 goes in place of JP5, with its switch closed;
- CN1 is plugged back, and the MCU starts from its flash with no debugger since.

Three things read nothing on the way, and each was mistaken for something else first
(`method.md`):

- VIN and VOUT the wrong way round: the MCU still runs through the body diode of the
  PPK2's switch, and the PPK2 reads 0;
- the PPK2 as a source on VDD_MCU: the board then feeds VDD_MCU by another way, and the
  PPK2 supplies nothing once CN1 is in;
- the debugger, which never reached the MCU through the PPK2, only with JP5 fitted.

| `SleepU5`, one of each 30 s phase, 3.3 V | LDO | SMPS (`make SMPS=1`) |
|---|---|---|
| Sleep phase, mean | 10.97 mA | 6.20 mA |
| Stop 2 phase, mean | 1.02 mA | 0.60 mA |
| Stop 2, the level a long sleep reaches | some 21 to 22 µA | some 6.5 to 7.5 µA |

The means of the Stop 2 phase are mostly the time awake:

- two wake-ups every 100 ms, each in Sleep for the 3 ms before its event, at some 11 or
  6 mA;
- the wake-up itself, 58 ticks of LPTIM1 waiting for the HSE;
- a report on USART1 each second.

The level of Stop 2 itself, with every SRAM retained, compares with the datasheet's
(DS13737, Stop 2: some 20 µA on the LDO, 8.2 µA on the SMPS at 25 °C). Each sleep reads
low first, some 1.4 µA, and climbs to that level over some tens of ms: for a small
current the PPK2 takes a large shunt, through which the MCU's decoupling capacitors
charge back. That shifts charge in time and leaves the means right. The level is read at
the end of a long sleep, and a short sleep never reaches it. The temperature was a
living room's, and it was not measured. Each figure comes from one run.

The margin of the wake-up, `OS_STOP2_WAKE_US`, 3 ms, is the Sleep that follows each
wake-up until the event, at some 6 mA on the SMPS. `OSSetStop2Wake()` sets it at run
time. `SleepU5` built with `PHASES=30 WAKE=2200` alternates 30 s at 3 ms, D13 high, and
30 s at 2.2 ms, D13 low, both in Stop 2. On the NUCLEO on the SMPS on 2026-10-03, four
phases of each gave 588 µA at 3 ms and 500 µA at 2.2 ms. That is some 110 µA a
millisecond of margin, at twenty wake-ups a second. No wake-up was late over 2,720
instances. The longest took 62 ticks of LPTIM1, 1.89 ms, with its HSE starting. The
default stays 3 ms. The UNO Q's wake-up, 19 ticks, 0.58 ms, would need far less.

### Slower clocks, measured (2026-10-03)

`make MHZ=80`, `40` or `16` builds every example at that speed instead of 160 MHz
(`OS_SYSTEM_CLOCK_HZ`, `Escapement_Processor.c`). The clock is PLL1 divided by 4 or 8 in
voltage range 2 or 3 without the booster, or the HSE itself in range 4. The flash and
the SRAM get the wait states that those ranges need (RM0456, tables 54 and 47). Renode's
platform cannot check them, since its clocks are fixed. The NUCLEO did check them.
`SleepU5Flash` built with `PHASES=30 SMPS=1` at each speed ran its 150 instances on
time, none late, and the HSE never missed. The PPK2 then read the figures below, one run
each, in the same order as above:

| `SleepU5`, SMPS, 3.3 V | 160 MHz | 80 MHz | 40 MHz | 16 MHz |
|---|---|---|---|---|
| Sleep phase, mean | 6.20 mA | 3.77 mA | 2.71 mA | 1.53 mA |
| Stop 2 phase, mean | 0.60 mA | 0.41 mA | 0.35 mA | 0.27 mA |
| Run, the median of the phase that computes | 10.68 mA | 5.66 mA | 3.64 mA | 1.82 mA |
| Run, a cycle | 67 pC | 71 pC | 91 pC | 113 pC |
| On the LDO: Stop 2 phase, mean | 0.98 mA | 0.65 mA | 0.50 mA | 0.36 mA |
| On the LDO: Run, the median | 20.44 mA | 10.50 mA | 6.20 mA | 2.89 mA |
| On the LDO: Run, a cycle | 128 pC | 131 pC | 155 pC | 180 pC |

Half the speed saves 39 % of the Sleep, and a tenth of it saves 75 %: part of the
current does not follow the clock. The Stop 2 phase is mostly the Sleep of the wake-up's
margin, which costs less at a slower clock. The task's work, a few µs an instance, is
too little for a slower core to show its price there. The Run rows are `SleepU5Flash`
built with `RUN=80000` too. Its second phase computes for 80 ms of each 100 ms instead
of sleeping, in a loop on a variable on the stack. The median of that phase is the core
running. The mean of that phase also counts the 20 % left in Sleep beside (9.81 mA at
160 MHz, 0.8 × 10.68 + 0.2 × 6.2). The LDO rows are the same images built without
`SMPS=1`. They were measured the same day, in the same order, one run each.

A cycle costs least at 160 MHz, in voltage range 1 with the booster: the lower ranges do
not save what the part of the current that does not follow the clock costs over the
longer time. A given work done at 160 MHz and followed by Stop 2, some 5 to 7 µA, takes
less charge than at the slower speeds:

- 6 % less than at 80 MHz;
- 27 % less than at 40;
- 41 % less than at 16.

On the LDO, the U585's only regulator, a cycle costs twice as much, and the order is the
same:

- 2 % less at 160 than at 80, within what one run tells;
- 17 % less than at 40;
- 29 % less than at 16, where the U585's datasheet figures, 84 and 73 µA/MHz, put a
  cycle 13 % cheaper at 24 MHz than at 160 (`power-aware.md`).

DVFS, the work done slower, gains nothing on either regulator. Racing to Stop 2 is the
best of the four, at every load. What a slower clock does save is the time awake doing
nothing: the margin of each wake-up, 0.60 mA in the Stop 2 phase at 160 MHz against 0.27
at 16 (below, "The margin slept on the MSIS"). The check of `tools/unoq_sleep.py` caught
two things at 16 MHz, both fixed since:

- TIM2 lost some 30 cycles a wake-up, those between the edge of LPTIM1 that ends the
  sleep and TIM2's start, and nothing made them up. The loss was −6.1 ppm against LPTIM1
  at 80 MHz, −14.2 at 40 and −38.5 at 16, at 19 wake-ups a second, and 3 to 5 ppm at 160
  before. `Stop2Idle` gives the cycles back (`RESTART_CYCLES`). The four speeds then
  read −2 to −5 ppm over 30 s, none in the order of the speed.
- The way of a timer event's interrupt to its task, some 800 cycles, took 49 µs at 16
  MHz, where the check allowed 20. The check now allows 1,600 cycles, with `MHZ=16` in
  its environment.

### The margin slept on the MSIS (2026-10-03)

The wake-up's margin, 3 ms, covers the HSE's start and the raise of the clock. The
NUCLEO's HSE takes 1.7 ms of the margin to start, and the UNO Q's takes 0.5. What the
HSE left of the margin was spent in Sleep at 160 MHz, up to the event. The idle task now
sleeps that rest on the clock that the chip woke on, the MSIS at 4 MHz in range 4
(`SlowSleep`, `Escapement_Stop2.c`):

- `_OSStartHSE` starts the HSE and waits for it, as before;
- the core sleeps on LPTIM1 until `OS_STOP2_RAISE_US`, 500 µs, before the event;
- `_OSRaiseSystemClock` then takes PLL1 up.

A byte on LPUART1 that woke the chip has the clock raised at once. The counts leave the
sleep out of the longest wake-up, and they count the wake-ups that slept so (`Slow`).

On the NUCLEO the same day, `SleepU5Flash` was built with `PHASES=30` at 160 MHz, one
run each, with the Sleep phase as a control:

| | SMPS | LDO |
|---|---|---|
| Stop 2 phase, the margin at 160 MHz (above) | 0.60 mA | 1.02 mA |
| Stop 2 phase, the margin slept on the MSIS | 0.48 mA | 0.78 mA |
| Sleep phase, the same in both | 6.22 mA | 10.98 mA |

The Stop 2 phase draws some 20 % less on either regulator. No instance was late over 250
and 150, and the longest wake-up took 56 and 55 ticks of LPTIM1. The NUCLEO's HSE leaves
some 0.8 ms of the 3 to sleep so. The UNO Q's HSE, which starts in some 19 ticks, leaves
1.9. Sleeping so should save more on the UNO Q, but its shared rail cannot show that
alone. Under Renode every wake-up of `SleepU5` slept so, and none came late
(`escapement_u5.robot`).

## Errata

The errata sheet of the chip, ES0499 (rev. 12, June 2026), was read against the port on
2026-09-26. The UNO Q's STM32U585 is revision U (DBGMCU_IDCODE 0x30076482). One erratum
touches the port. It touches only the accuracy of the port's clock, and only if the HSE
does not start. The NUCLEO-U575ZI-Q's STM32U575 is revision X (0x20016482). Its errata
were read on 2026-10-02. None of those it adds touches the port, but 2.2.27's workaround
is not available there (`SleepU5 on the NUCLEO-U575ZI-Q` above).

### 2.2.27, spurious MSI PLL unlock

The MSI may leave its PLL mode on an LSE failure that it detects wrongly. This is more
likely when cold and at a low core voltage. The MSIS then runs free again, 0.48 % fast
on this board. The kernel's clock follows it only when PLL1 had to take the MSIS. The
port takes ST's workaround. The unlock raises line 23 of the EXTI and interrupt 125
(RM0456 rev. 7, tables 118, 186 and 189). The CMSIS header of the U585 names the
interrupt `LSECSSD_IRQn`, after the LSE's clock security, which shares it. The header
has no name for the unlock nor for line 23. Until 2026-09-29, this page said that the
header named neither. The interrupt's handler turns the PLL mode off and on again and
counts the event, and `SoakU5` reports the count. On 2026-09-26, with the core halted,
line 23 was raised by software on the board. The handler counted it once, cleared the
pending flag and turned the PLL mode on again. The halt itself cost the link a byte, as
a halt alone did afterwards.

### Errata the port stays clear of

The other errata that come near the port do not touch it. The LPTIM1 driver, added on
2026-09-27, avoids two more (2.17.1 and 2.17.3).

| Erratum | Why the port is clear |
|---|---|
| 2.2.3, 2.2.16: LSE unusable at the low and medium-low drives | it sets medium-high, as Zephyr |
| 2.2.26: hang on entering Stop or Standby with the flash prefetching at 4 wait states | the prefetch is off since 2026-09-29; it was on, the images running from SRAM taken to keep the port clear, which an audit of the port doubted: the condition is the prefetch and the wait states, wherever the code runs from, and the prefetch serves nothing to images that never fetch from the flash |
| 2.2.1: PC13 toggling disturbs the LSE | neither the port nor Arduino's device tree uses PC13 |
| 2.22.3: LPUART transmitter jitter with a kernel clock 3 to 4 times the baud rate | HSI16 for 57,600 to 115,200 baud, 278 to 139 times; 17 at the 921,600 tried |
| 2.2.2: MSI slow on leaving Standby or Stop 3 | the port enters Stop 2 alone |
| 2.2.5: hang entering Stop 2 with PLL2, PLL3, HSI48 or SHSI on | the port starts none of them |
| 2.2.11: first read of a cache line after Stop 2 corrupted | the images run from SRAM through the S-bus, which ICACHE does not cache; DCACHE1 is off |
| 2.2.19: HardFault on a wake-up by an SRD peripheral with DBG_STOP set | `OSInitStop2` clears DBG_STOP, and DBG_STANDBY, which the debugger may set |
| 2.2.22: device locked by a reset in Stop 2 with an SRAM powered down | every SRAM stays powered |
| 2.17.1: disabling LPTIM by its ENABLE bit may leave its interrupt stuck and keep the chip from Stop | LPTIM1 is reset through the RCC, never disabled |
| 2.17.3: writing LPTIM_DIER clears the flag it enables | DIER is written once, at start-up, its flag clear |
| TIM break and ocref, IWDG in Stop, USART DMA and smartcard, MPU faults | not used |

The core is a Cortex-M33 r0p4 (CPUID 0x410FD214). Arm's own errata notice for it
(SDEN-756493, v9.0) leaves only 1080541 open in that revision, on the MPU. It is the
same as ES0499's 2.1.1 ([`architecture.md`](architecture.md)).
