# The STM32U5 port

`Escapement/CORTEX-Mx/STM32U5` runs the kernel on the STM32U585 of the Arduino UNO Q, a
Cortex-M33 at up to 160 MHz with 2 MB of flash and 768 KB of SRAM. The U585 is an
STM32U575 with cryptography added, which the port does not use. Work began under Renode
on 2026-09-25, and the port has run on the UNO Q since 2026-09-26. The same sources also
build `SleepU5` for a NUCLEO-U575ZI-Q, a board on which the MCU's current can be
measured.

This page covers how the port is built and loaded, what was verified on the board and
under Renode, the accuracy of the kernel clock, the idle task in Stop 2, the current
measurement, and the errata of the chip.

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

The port was written anew after the model of the RP2350 port. It is not one more family
of the inherited STM32 port, whose drivers carry the addresses of five families in
conditional compilation. Each file defines the registers it uses, and only those the
port needs. They come from RM0456, the reference manual of the STM32U5, with addresses
and bit names as in STMicroelectronics' `cmsis-device-u5` (`stm32u575xx.h`). That header
was read, not copied into the repository.

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

The examples in `Examples/uno-q` are those of the Pico 2, transposed: `TaskLEDU5`,
`UARTEchoU5`, `TestTimerEventU5`, `TaskWrapU5`, `IPCU5`, `TestLPTimerU5`, `SleepU5` and
`SoakU5`, the endurance test. In `SoakU5`, the part that the Pico 2 runs between its two
cores becomes a 4-slot buffer written by the interrupt of TIM3 and read by a task that
this interrupt preempts. The independent watchdog stands in for the RP2350's. The
examples drive four outputs (`BoardU5.h`): the green of LED3 on PH11 and the blue of
LED4 on PH15, both lit when low, and PB13 and PB14, which are D13 and D12 of the
connector. The hard and the soft kernel build under EDF and deadline-monotonic
scheduling. The power-aware kernel is not ported.

## On the board

### Loading into SRAM

The flash of the UNO Q holds Arduino's bootloader and firmware. The debug pins of the
MCU are not brought out: the board's Linux processor drives them from GPIOs, with an
OpenOCD of Arduino's in `/opt/openocd`. The images therefore run from SRAM.
`tools/unoq_load.sh` sends an image to the board over SSH, loads it and starts it, and
leaves the flash as it was. With `--reset` it returns the board to Arduino's firmware,
as any reset does.

The loader also freezes TIM2, TIM3 and TIM5 while the debugger halts the core
(DBGMCU_APB1FZR1), so that halting the board to read it does not make the tasks late.
The independent watchdog's bit in that register reads back 0. The watchdog therefore
keeps running, and a halt must stay short. While the core sleeps in the idle task, the
debugger reads zeros from SRAM and peripherals alike, so a reading halts the core first.

### The endurance test

Once `SoakU5` runs, the debugger is no longer needed. The image sends its counts to
Linux on LPUART1 every second. `tools/soak.py uno-q` runs as a service on the board's
Linux, reads them and logs them at every interval. In the other direction it sends the
MCU a count, one byte after the other, in bursts of random length at random times, so
that the interrupts come at moments Linux chooses. The MCU checks each byte against the
one before, and a byte lost or wrong is an error. Arduino's Bridge, which holds
`/dev/ttyHS1`, is stopped meanwhile. If the reports stop, the board has restarted, and
the script loads the image again. On 2026-09-26 the link carried every byte sent, 5,570
in 12 s with none lost, while every part of the test ran without error.

The same script runs the Pico's endurance test with the same checks, reading the counts
over SWD there. `SoakU5` also reports the causes of reset it found in RCC_CSR, then
clears them. These flags survive resets until cleared, so each run sees the resets since
the one before it. On 2026-09-26 a halt of 4.5 s, longer than the watchdog's 3 s,
restarted the board. The image, loaded again, reported `pin+IWDG` (the watchdog, and the
reset of the load), and the link went on without an error. The script now waits for the
new image's first report before it sends again.

An image taken over has counted the bytes sent to it before. Each board check runs the
endurance test for 2 min, then the long run takes the image over, and until 2026-10-01
its log read some 78,000 bytes more received than sent, the same offset all along. The
script now starts its count of bytes sent from the image's at each take-over.

On a NUCLEO-U575ZI-Q, the link goes over USART1 to the ST-LINK at 115,200 baud. At
00adc77, USART1 ran without its FIFO, so each byte had to be read within one frame,
87 µs. On 2026-09-29, after 62,768 s (17 h 26 min), the run lost one byte to an overrun
and counted the next as out of the count: 1 overrun, 1 error of the link, while every
count of the kernel stayed at 0. What delayed the interrupt that once cannot be told
afterwards. LPUART1 on the UNO Q, with its FIFO of 8 bytes at 57,600 baud, had lost none
in 69,000 s. USART1 now runs with its FIFO too, which allows some 700 µs: at 73a6d83, on
2026-09-30, it had carried 54 MB in 84,218 s (23 h 23 min) with no overrun and no error.

LPUART1 lost its first byte on 2026-10-01, at 1b67f30, after 2 h 20 min: 1 overrun, 1
error of the link, every count of the kernel at 0, some 300,000 s of endurance on the UNO
Q before it without one. Its FIFO allows 1.4 ms, yet the pulse, due every millisecond,
was never more than 47 µs late: no interrupt was held off that long. Nothing halted the
core either, no OpenOCD nor login on the board at that minute. What the counts do not
rule out is the core and its timers stopped together, as under a debugger's halt, while
the LPUART, on HSI16, went on receiving. Since then `SoakU5` keeps the longest burst of
the link, the bytes read within 5 µs of each other, with the time the kernel's clock
counted since the byte before them. n bytes waiting came over at least n − 1 byte times,
174 µs each at 57,600 baud; less on the clock, and `tools/soak.py` says how much the
clock lost.

## What is verified

### On the board

On the UNO Q on 2026-09-26, the clock set-up reached 160 MHz: PLL1 locked with the
system clock on it, voltage range 1 with the booster ready, and TIM2 counting
microseconds. `TaskLEDU5` ran, the idle task asleep between rounds. `SoakU5` ran for
22 s with every part active and none in error. Its pulse was at most 40 µs late, its
timer events at most 23 µs.

The endurance test has run for hours since, restarted at each commit the bench checks.
Over the night of 2026-09-26 to 27, on 52440a3, it ran 9.3 hours across 31 wraps of the
kernel clock with no error and no restart. The pulse was at most 48 µs late and the
timer events at most 49 µs, and 21 MB came from Linux over the link without an error.

The first load found a defect that Renode could not show. The update event that loads
TIM2's prescaler raised its flag a few timer cycles after the write, after the clear
that followed it had already run. The kernel took that flag for an overflow before the
timer had started, and stopped on its overload check. With `URS` set, only a wrap of the
counter raises the flag (`Escapement_Timer.c`).

### Under Renode

Renode runs the port on a platform of the project's own
(`emulation/renode/escapement_u5.repl`, see [`emulation.md`](emulation.md)), which has
the pins of the UNO Q since 2026-09-26. The twelve tests of `escapement_u5.robot` pass
under each of the four builds, hard and soft kernel under EDF and deadline-monotonic.
They cover:

- the probe task every millisecond, the three periodic tasks, the UART echo and the
  timer events;
- the 2^30 wrap of the kernel clock;
- tasks preempting one another inside the FIFO queue and a slot buffer, R8-R11 kept
  across;
- 3.5 s of the endurance test with every part active and none in error;
- LPTIM1's count against TIM2's, and the idle task sleeping on LPTIM1, up to the wrap
  when the next arrival lies beyond it;
- the endurance test built for the NUCLEO-U575ZI-Q reporting on USART1, the ST-LINK's
  virtual COM port, and not on LPUART1 (`escapement_u5_nucleo.repl`, which adds the
  ports A and F of that board's pins).

The idle task never reaches Stop 2 there. The platform does not report it
(PWR_SR.STOPF), and the clock is not restarted. `SleepWrapU5` takes the idle task across
the 2^30 wrap: it is `SleepU5` with its times and margins a thousand times longer, with
TIM2, TIM5 and LPTIM1 clocked a thousand times faster. It crosses the wrap twice in
2.5 s with no instance late (2026-09-27). Every start came within 0.5 µs of its period
and every timer event within 10.5 µs of its time, the start of the event task not being
a thousand times faster.

An audit of the port found on 2026-09-29 that the idle task could sleep past the wrap.
An event-driven task signalled before its period is over waits for the period's end in
the arrival queue, its time kept whole, which may lie beyond the wrap; the kernel arms
TIM2's compare with it, above the counter's range, where it never matches. The idle task
took that compare for the next event, slept on past the wrap in steps of LPTIM1, then
set TIM2 back to just before it: the kernel's clock lost the overshoot, up to a whole
sleep, 2 s on the board, and counted nothing late. `Stop2EventWrapU5` shows it under
Renode: TIM2 and TIM5, both moved on by LPTIM1 after each sleep, drift apart by some
46 µs a period there, the same within 1.5 µs every period, but by 1.55 ms more over the
one across the wrap. The idle task now sleeps no further than the wrap when the compare
lies beyond it (`Escapement_Stop2.c`), and that period falls in with the others.

The endurance test found that the port routed to the dispatcher only the interrupts of
its own drivers, and sent TIM3's to the trap for an undefined one. Every interrupt of
the chip now reaches the dispatcher. The compiled order of the slot buffers and of the
task-level stores holds (`tools/check_order.py`), and the static analysis finds nothing.
The CI runs all of it.

### What this does not verify

The endurance test starts and reloads the watchdog, and the emulated board did not
restart in 3.5 s. Whether Renode's model would restart it after 3 s without a reload was
not checked. The platform also acknowledges every clock request without checking it, so
a wrong divider or a missing wait would pass. Only the board can show that the clock
set-up is right, which it has done for the steps above.

## Accuracy of the kernel clock

The clock was measured against Linux's own, which NTP keeps. The first measurements
compared the seconds `SoakU5` reports with the times `tools/soak.py` receives them. With
the MSIS running free, the kernel counted 0.48 % too many seconds: +4,800 ppm over
2,564 s on 2026-09-26, and +4,300 to +4,800 ppm over four shorter runs that day. Locked
on the board's 32.768 kHz crystal (MSIPLLEN), as Arduino's firmware has it, the kernel
counted 1,140 s in 1,140 s the same day, which is all that whole seconds over 19 minutes
can tell. Over the next hour and a quarter it counted 4,579 s in 4,583, some 800 ppm
slow.

`tools/unoq_drift.py` measures more closely. It times on Linux the arrival of each
report, sent at the start of each of the kernel's seconds. With the MSIS locked, a
second of the kernel's lasted 653 ppm too long (2026-09-26, 100 s, standard error
2 ppm). The datasheet explains why. In PLL mode the MSI runs at a whole multiple of
32,768 Hz, 3.998 MHz in range 4 (DS13086 rev. 10, table 83), which is 576 ppm short of
4 MHz; the LSE's own error makes up the rest. No whole prescaler makes a microsecond of
that frequency.

PLL1 now takes the board's 16 MHz crystal, the HSE, which divides into a microsecond
exactly. The same evening the second lasted 0.1 ppm too little over 300 s, and 22.5 ppm
too long over 600 s. The two results differ by more than their standard errors (2.4 and
0.4 ppm) allow, because the reference moved: Linux's clock took in an offset of 35 ms
from NTP meanwhile. The rate is known within some 25 ppm, then, which is as close as
that reference tells and what a crystal gives. Over the 9.2 hours of the following
night, the seconds `SoakU5` counted matched Linux's to the second. A line through the
543 readings gives 21 ppm slow, with a standard error of 1.7 ppm. That morning Linux's
clock, kept by systemd-timesyncd, was itself 61 ms off NTP with 54 ms of jitter. Within
those 25 ppm the U5's clock and Linux's cannot be told apart. Should the HSE not start,
the MSIS remains PLL1's input. Until 2026-09-30 that was its range 4, 3.998 MHz, under
the 4 MHz RM0456 gives as the floor of both the VCO's input and the booster's clock
(RCC_PLL1CFGR). The port now raises it to range 2, whose MSIRC0 runs at 1,465 periods of
the LSE, 48.00512 MHz, divided by 3: 16.0017 MHz, the booster's clock 8.0009 through its
prescaler of 2, the VCO's input 5.3339 through M = 3, and 160.017 MHz after x 60 / 2,
107 ppm fast by the datasheet's figures where range 4 gave 576 ppm slow. After each
wake-up the MSI takes up to 0.8 ms to come within 1 % of its frequency again, running
meanwhile as in MSI mode, within some 1.4 % at 30 °C and 3 V and a further −4 to +2 %
over temperature (DS13086 rev. 10, table 83): at 5.4 % slow both inputs stay within
their ranges, 7.6 and 5.05 MHz; range 3 divided by 3, 4.0004 MHz, would not. The SRAM
the images run from reads at 0 wait states up to 16 MHz only in voltage range 4 (RM0456,
table 47), where the chip starts and wakes from Stop 2; one is set before the MSIS goes
to 16 MHz and before each Stop 2, and taken off in range 1. This commit read the 1 % as a
bound during the lock and missed the SRAM; an independent review found both the same
day. The system clock itself may run a few % above its 160 MHz until the MSI is locked,
as it could with range 4. `escapement_u5.robot` checks the registers on a platform without
the HSE (`escapement_u5_nohse.repl`). The NUCLEO-U575ZI-Q was taken for a board without
it, and its endurance runs since 2026-09-30 (7d069ea) for a check of this path on the
board. On 2026-10-02 its RCC read the HSE ready and PLL1 on it, M = 4: its crystal X3 is
fitted (UM2861, 6.7, leaves it to the variant). Those runs checked the HSE, and the MSIS
of range 2 had run under Renode only, on no board. `SleepNoHSEU5`, `SleepU5` built with
`OS_NO_HSE` (`make NOHSE=1` for any image), starts as a board without the HSE would: PLL1
takes the MSIS of range 2 at start and at every wake-up from Stop 2. A Renode test checks
it on the platform that enters Stop 2, and fails when the option does nothing; the board
check of each commit runs it on the UNO Q for a minute after `SleepU5`. Its first run on
the board, in the check of d9a32eb on 2026-10-02, was the first of that path on a chip:
600 instances, 1,225 entries into Stop 2, none late, every byte of the link received,
and the longest wake-up 4 ticks of LPTIM1 against 19 waiting for the HSE. The check
still failed, on TIM2 against LPTIM1: it expected TIM2 106.7 ppm ahead, PLL1 on the MSIS
being that fast, and read -4.1. TIM2 is set from LPTIM1 at every wake-up from Stop 2,
where the image spends most of its time, and runs on PLL1 only awake; it is now checked
as `SleepU5`'s, within 20 ppm.

Since 2026-09-28 the board check times the U5 against `CLOCK_MONOTONIC_RAW`, the crystal
of the board's Qualcomm processor as it is, which NTP does not pull (`tools/unoq_drift.py`).
That crystal was measured against NTP on 2026-09-29, from 16:50 to 17:35 UTC, NTP
synchronized for hours with a stratum 1 server:

- straight against the server, 266 SNTP exchanges timed on the raw clock, the shortest
  of each minute kept: the raw clock 0.2 ppm fast, with a standard error of 6.3 ppm, too
  wide to tell a rate that small. The Wi-Fi limits it, with round trips of 8 to 470 ms,
  210 ms the median;
- through the correction NTP applies, which is the rate of `CLOCK_MONOTONIC` against the
  raw clock and needs no network: +0.73 ppm over those 45 minutes, which includes NTP
  taking in an offset of a few milliseconds, and +1.19 to +1.42 ppm in the frequency
  systemd-timesyncd had settled on (`adjtimex`).

Over a longer span the correction tells the crystal closer, as the offsets NTP takes in
weigh less: from 17:37 to 20:57 UTC the same day, 11,989 s of the raw clock, the clock
NTP disciplines gained 21.09 ms on it, 1.76 ppm, within some 0.5 ppm for the few
milliseconds NTP may be off at either end. The raw clock runs slow by some 1.8 ppm, then,
a hundred and fifty times less than the check's bound of 300 ppm.

## The idle task in Stop 2

### The plan, as read on 2026-09-26

The plan for Stop 2, as read from RM0456 rev. 7 and ES0499 on 2026-09-26, before any of
it was written. LPTIM1 cannot be the kernel's clock. It has 16 bits and wraps every 2 s
on the LSE, in steps of 30.5 µs. Its counter must be read twice to be trusted, and a new
compare waited for (CMPOK) after a latency RM0456 does not quantify for it (§58.4). So
TIM2 stays the kernel's clock while the core runs. The idle task, finding the next event
far enough off, arms LPTIM1 on the LSE to wake it early and enters Stop 2, from which
LPTIM1 wakes the chip (table 599). Stop 2 turns the HSE off and leaves the chip in range
4. Waking takes the HSE's 2 ms, up to 47 µs for range 1, 50 µs for the booster and 25 to
50 µs for PLL1, so Stop 2 pays only for waits of some milliseconds. TIM2 is then moved
on by the time LPTIM1 counted, and the idle task finishes the wait in Sleep. The errata
kept in view: never clear LPTIM1's ENABLE, reset it through the RCC instead (2.17.1);
writing DIER clears the flag it enables (2.17.3); a HardFault may follow a wake-up by
LPTIM1, of the SRD domain, when debugging with DBG_STOP (2.2.19). Renode's
STM32L0_LpTimer has the same first registers and was expected to serve.

Two parts of that plan changed on 2026-09-27. It kept Stop 2 away while a timer event
was pending, TIM5 stopping with it, a rule taken from the definition of Stop rather
than read for TIM5 itself; TIM5 is now carried through Stop 2 instead (06e2e60). It
also kept Stop 2 away while a UART had to receive, since LPUART1 runs on PCLK3, and
`SoakU5`, fed by Linux, would hardly ever have entered it. LPUART1 then received
through Stop 2 on HSI16, at 57,600 baud, the rate its start allows for a first byte
(2673f02, "LPUART1 through Stop 2" below); since 2026-10-02 at 115,200 behind a wake-up
byte ("The wake-up byte").

### LPTIM1

LPTIM1, on the 32.768 kHz crystal (`Escapement_LPTimer.c`), was the first step toward an
idle task in Stop 2 ([`roadmap.md`](roadmap.md)). Every 250 ms, `TestLPTimerU5` sums the
ticks LPTIM1 counted and the microseconds TIM2 counted. It also arms a compare some
10 ms ahead, which the next instance must find raised. On the board on 2026-09-27 it
read 983,025 ticks in 30,000,000 µs. The LSE thus ran 15 ppm slow against the HSE,
within what either crystal gives, and no compare was missed in 120 instances.

### Entering and leaving Stop 2

The idle task enters Stop 2 once the application has called `OSInitStop2`
(`Escapement_Stop2.c`), provided three conditions hold: the next event is 5 ms off or
more, no UART is sending, and USART1 is not receiving. That event is an arrival or the
wrap of TIM2, or a timer event of TIM5. The idle task arms LPTIM1 3 ms short of the
event, stops TIM2 and TIM5 on an edge of LPTIM1 and enters Stop 2. On waking it takes
the clock back to 160 MHz and starts both timers again on an edge, moved on by the ticks
counted in between. Until 2026-09-27 a pending timer event kept the idle task in Sleep.

`SleepU5` runs one task every 100 ms over this idle task. On the board on 2026-09-27,
over 43 s:

- 388 entries into Stop 2 for 430 instances; the others were the instances that sent a
  report on LPUART1;
- each instance started 100,000 µs after the one before, to the microsecond;
- the longest wake-up took 29 ticks, 885 µs, of the 3 ms allowed, and none woke past its
  event;
- TIM2 counted 43,000,000 µs where LPTIM1 counted 1,409,021 ticks, 2.1 ppm more, some
  0.2 µs a sleep.

Each instance of `SleepU5` also has a timer event wake a second task 40 ms later, since
TIM5 is carried through Stop 2. On the board on 2026-09-27, over 60 s, there were 1,160
entries into Stop 2 for 610 instances: two a period, one before the event and one before
the next instance. Each event came within 5 µs of the time it was due, the longest
wake-up took 19 ticks, and none was late. The board check runs this image
(`tools/unoq_sleep.py`).

### A bound on the HSE's start

Until 2026-10-02 the wake-up waited for the HSE as reset does, some 20 ms with
interrupts masked, then gave it up for good, PLL1 on the MSIS from then on. DS13086 gives
its start 2 ms typical and no maximum (table 80); on the board the whole wake-up took at
most 29 ticks, 885 µs. The wake-up now waits 64 ticks of LPTIM1, 1.95 ms
(`Escapement_Processor.c`). Past that, PLL1 takes the MSIS of range 2 for that wake-up
only, 160.017 MHz, some 100 ppm fast, and the next tries the HSE again; `SleepU5` counts
these wake-ups in word 14 of its results and `tools/unoq_sleep.py` shows them. The SRAM's
wait state before Stop 2 follows the MSIS's range rather than the HSE's absence, since a
miss leaves the MSIS in range 2.

Under Renode, on a platform that enters Stop 2 (`escapement_u5_stop2.repl`), every
wake-up of `SleepU5` took PLL1 back to the HSE when it started within the bound, and,
when it never started, every one went on the MSIS in 70 ticks at most, none late of the
98 allowed, and the first wake-up after the HSE started again took it. The old wait fails
that test. On the board, in the check of 3faaf23 on 2026-10-02, `SleepU5` entered Stop 2
2,239 times in 60 s; the longest wake-up took 19 ticks, the HSE missed none, and all
3,085 bytes sent from Linux were received.

### Loading an image while the core sleeps

An image that sleeps in Stop 2 with DBG_STOP cleared leaves the debug port unpowered
most of the time. Loading the next image then failed on an SWD parity error, OpenOCD
having connected while the core slept, and `SleepU5` ran on. In the board check of
2dbcece on 2026-09-27, the endurance test that came next saw no report and counted
restarts. `tools/unoq_load.sh` now connects with the reset held (`connect_assert_srst`,
with `srst_nogate`) and fails on any error from OpenOCD. After that change, three loads
out of three following `SleepU5` succeeded.

### LPUART1 through Stop 2

LPUART1 receives through Stop 2 since 2026-09-27 (`Escapement_UART.c`). Its kernel clock
is HSI16, which the LPUART wakes by itself when a byte comes (UESM, autonomous mode,
RM0456 67.4.15). The first byte is sampled while HSI16 starts, which takes up to 3.6 µs
(DS13086, table 82). At 115,200 baud that is 3.8 % of a frame, past the 3.41 % the
receiver tolerates; at 57,600 baud it is 1.9 %. LPUART1 and the tools that read it
therefore ran at 57,600 baud, until the wake-up byte below. The receive FIFO holds the bytes that come while the clock
is raised again, interrupts masked. Keeping HSI16 on in Stop 2 instead (HSIKERON) would
have kept 115,200 baud at some 150 µA (table 82), against 20.5 µA for Stop 2 itself with
every SRAM retained, at 25 °C (table 56).

On the board the same day, `SleepU5` received over 60 s all 2,761 bytes that
`tools/unoq_sleep.py` sent in bursts of 1 to 32. None was out of the count and there was
no overrun, with 2,444 entries into Stop 2. `SoakU5` at 57,600 baud ran 2 min without
error, its link included.

### The window after a byte

Since 2026-10-02 a byte received on LPUART1, or a wake-up from Stop 2 that LPTIM1 did not
cause, keeps the idle task in Sleep for `OS_STOP2_LINK_WINDOW_US` after it, 20 ms by
default (`Escapement_Stop2.c`). It is the second step of the wake-up byte
([`roadmap.md`](roadmap.md), item 6): the bytes that follow the first come while HSI16
runs, so that they could be sampled at 115,200 baud. The window is counted on LPTIM1,
whose compare ends it if nothing else wakes the chip, so that Stop 2 resumes then rather
than at the next event. The compare is written once a window: its write waits some two
ticks with interrupts masked, and the bytes of a message come every 87 µs at 115,200 baud.
`SleepU5` counts the sleeps the window held in word 15 of its results.

Under Renode, on the platform that enters Stop 2, a byte sent 50 ms into a period woke the
chip from Stop 2; TIM2 ran for the 20 ms after it, then stood still again in Stop 2 before
the next period, none late. A byte every 10 ms for half a second kept the idle task out
of Stop 2 throughout, all its bytes received in order. Without the window both tests
fail; without the compare at its end the first does.

On the board, in the check of 29348d8 on 2026-10-02, still at 57,600 baud and without a
wake-up byte, `SleepU5` entered Stop 2 1,239 times in 60 s against 2,239 at 3faaf23, the
window holding 2,759 sleeps in Sleep after the bursts from Linux. All 2,785 bytes were
received, none late, the longest wake-up 19 ticks as before.

### The wake-up byte

`SleepU5`'s link runs at 115,200 baud since 2026-10-02 (`OS_LPUART1_BAUD_RATE`, set by
the UNO Q's Makefile; `SoakU5` too, which never enters Stop 2, its tools with it). Before each
message `tools/unoq_sleep.py` sends a wake-up byte, 0x00, waits 5 ms, more than the 3 ms a
wake-up is allowed (`OS_STOP2_WAKE_US`), then the message as a frame: 0x00, the bytes and
their CRC-16 (CCITT, from 0xFFFF) encoded with COBS, 0x00. Sampled while HSI16 starts,
the wake-up byte may come out as anything: 0x00, it is an empty frame, ignored; any
other value, it ends at the 0x00 that opens the message as a frame too short, dropped and
counted in word 16 of `SleepU5`'s results. A frame of the message dropped would show as
bytes missing from the count.

Under Renode, a wake-up byte sent as 0x5A into Stop 2 was dropped, the frame 5 ms after it
received whole, a frame with its CRC off by one dropped, and the count went on in the
next, none out of it. Accepting any CRC fails that test.

On the board, in the check of 9ff2e2f on 2026-10-02, `SleepU5` at 115,200 baud received
all 2,655 bytes `tools/unoq_sleep.py` sent over 60 s in frames behind their wake-up
bytes, none out of the count, no overrun, and dropped no frame: no wake-up byte came out
as a value other than 0x00, or none came out at all, which the counts do not tell apart.
It entered Stop 2 1,230 times, the window holding 3,875 sleeps; the longest wake-up took
19 ticks, the HSE missed none, none was late. One minute is no bound on how often a
wake-up byte comes out wrong.

### Faster, awake

Awake, the limit of LPUART1 is its FIFO of 8 frames against the latency of its
interrupt, which the kernel's timer outranks. On 2026-10-02 `SoakU5` ran 10 min at each
of 230,400, 460,800 and 921,600 baud on the board, images and `tools/soak.py` built for
each by hand from 9a6b1da (`OS_LPUART1_BAUD_RATE`): some 385,000 bytes of the link each,
in bursts of up to 64, none out of the count, no overrun, no restart, the pulse at most
45 µs late at every rate. At 921,600 the FIFO holds 87 µs. The limit lies beyond
921,600, the highest rate tried; and `tools/soak.py` sends some 640 bytes a second on
average, so a stream at full rate is not tried either.

### Three hours

`SleepU5` ran three hours on the board on 2026-09-27, from 11:18 to 14:18 UTC, in six
runs of 30 min at 2673f02, each across a wrap of TIM2 at 2^30. Over 108,000 instances
and 454,487 entries into Stop 2, every start was on its period to the microsecond, the
longest wake-up took 18 ticks, and none was late. Every timer event came within 7 µs of
its time. All 547,645 bytes sent from Linux were received, none out of the count, with
no overrun. TIM2 ran 7.3 to 7.6 ppm behind LPTIM1.

## Measuring the current

The UNO Q cannot show the U585's current alone. The following was read from its
schematics (ABX00162, dated 2025-10-01, pages 19, 21 and 22) and datasheet on
2026-09-27. The MCU's VDD, VDDA and VDDUSB sit on PWR_3P3V, which two buck converters in
series make from 5 V (TPS62A02, U2801 and U2802), with no jumper, 0-ohm resistor or test
point in between. The same rail feeds the ANX7625, the 3.3 V side of the Wi-Fi, the
level shifters and the 3.3 V pins of the connectors. It also feeds the power LED through
330 ohms, some 3 to 4 mA on its own. VDDIO2, and with it port G and LPUART1, comes from
the PMIC of the Qualcomm side (PM4125, VREG_L15A_1P8V). The U585 has no SMPS there
(STM32U585AII6, VCAP but no VDD11), which confirms the port's use of the LDO.

A PPK2 at the board's 5 V input measures the whole board: some 0.6 to 0.7 W running, and
28 mA with Linux powered off (a user's figures on the Arduino forum, 2026-05-28). The
20 µA of Stop 2 are lost in that. Only the difference between Sleep and Stop 2, some mA
at the MCU, may show, and `make PHASES=30` is built for it. `SleepU5` then alternates
30 s in Stop 2 with D13 high and 30 s in Sleep with D13 low (`OSAllowStop2`), under the
same load, and the PPK2 records both with D13 on its digital input. The CI builds that
image at each commit among the board's (`tools/board_images.sh`,
`ppk2_u5/SleepU5-phases30.elf`). With 5 s phases on the board the same day, there were
nine entries into Stop 2 a second in one phase and none in the other, every start on its
period in both. D13 itself has not yet been observed.

The absolute currents are for a NUCLEO-U575ZI-Q, whose board has a jumper for the MCU's
current, JP5 (UM2861, 6.4.5). `Examples/nucleo-u575` builds `SleepU5` for it from the
same sources. Its outputs are on pins that drive no LED, since the LEDs sit on the rail
that jumper measures. This one has its HSE crystal, X3, fitted, which this page denied
until 2026-10-02 on a reading of UM2861 (6.7), that leaves it to the variant: its RCC
read the HSE ready and PLL1 on it. A board without it would have the port fall back to
the MSIS locked on the LSE, the HSE given up after one failed start. The board's STM32U575ZIT6Q
has the SMPS that the UNO Q's U585 lacks: 8.2 against 20.5 µA in Stop 2 with every SRAM
retained at 25 °C, by the datasheet (DS13737 rev. 4, tables 54 and 56). The port keeps
the LDO, as reset leaves it, unless built with `make SMPS=1`. That option selects the
SMPS before the voltage range is raised (PWR_CR3.REGSEL), so that both can be measured
on the same board. The NUCLEO on the bench has its VDD at 1.8 V, JP4 on [2-3], not at
the 3.3 V it ships with (UM2861, 6.4.4.3): OpenOCD read a target voltage of 1.80 V on
2026-10-02, the first time it was read, and the jumper was found there; it was set back
to [1-2], 3.3 V, the same evening, while looking for the hang below. A current measured
at 1.8 V is to be weighed against the datasheet at the VDD of each table. The CI builds both images at each commit
(`ppk2_u5/SleepU5-nucleo-phases30.elf` and `SleepU5-nucleo-smps-phases30.elf`). The
board has run the endurance test on the bench since 2026-09-28; these two `PHASES=30`
images have not been tried on it yet.

### SleepU5 on the NUCLEO-U575ZI-Q

Built for the NUCLEO, `SleepU5` sends its reports on USART1, the virtual COM port of the
ST-LINK, and receives nothing there: USART1 receiving would keep the idle task out of
Stop 2 (`_OSUARTIdle`). `tools/unoq_sleep.py 60 nucleo` reads them on the UNO Q, and a
Renode test checks that the reports go there and that the idle task still arms LPTIM1.

On the board on 2026-10-02 it first never sent a report. Two causes, found in turn:

- JP2, which carries the ST-LINK's reset to the MCU (UM2861), was off. OpenOCD's "reset
  halt" then reset nothing: the core was found in its HardFault handler after it, the
  image was loaded and started there, at that priority, and locked up at the kernel's
  first exception. `tools/nucleo_load.sh` still reported the load done.
- With JP2 back, the image entered Stop 2 and never woke; OpenOCD reached the debug port
  but could not halt the core, the reset held did not get it back, and only a power-off
  did. A build that set DBG_STOP ran; the image as it was failed alike with the SMPS
  (`make SMPS=1`) and at 3.3 V. The NUCLEO's chip is revision X (DBGMCU_IDCODE 0x20016482), the first, whose
  errata were then read too (ES0499 rev. 12): none accounts for it. The cause was the
  loader. `tools/nucleo_load.sh` runs Debian's OpenOCD 0.12.0, whose
  `stm32x5x_common.cfg` sets DBG_STOP and DBG_STANDBY at each connection; the UNO Q's
  own OpenOCD clears both. `OSInitStop2` cleared DBG_STOP alone, and DBGMCU_CR read
  0x6 on the NUCLEO: the chip entered Stop 2 with DBG_STANDBY set, which RM0456 says
  holds off the reset (75.2.4) and which a system reset leaves (75.12.4). With DBG_STOP
  set the clocks never stop, which is why that build ran.

`OSInitStop2` now clears both. `SleepU5` then ran 60 s on the NUCLEO at 3.3 V: 600
instances, 1,141 entries into Stop 2, every start on its period to the microsecond, the
events within 5 µs, TIM2 within 5.6 ppm of LPTIM1, none late; and the reset held got the
core back while it slept, its SRAM intact. Its longest wake-up took 58 ticks of LPTIM1,
and 61 to 66 with DBG_STOP set, against 19 on the UNO Q. The wake-up waits for the HSE,
which this board has (above), and its HSE starts in some 1.8 ms where the UNO Q's takes
under 0.6: under the 64 ticks, 1.95 ms, the wake-up allows it before PLL1 goes on the MSIS
(`Escapement_Processor.c`), with little to spare. None missed in these runs; the count
of `SleepU5` would show one that did. The chip being revision X, the MSI PLL's unlock line, 23 of the EXTI, and
interrupt 125, which the port enables for erratum 2.2.27, are reserved there (RM0456,
tables 118 and 186, notes 2), though the erratum touches revision X too. The port reads
DBGMCU_IDCODE and leaves both alone on revision X, which therefore goes without the
workaround; a Renode test on a platform whose IDCODE reads revision X
(`escapement_u5_nucleo_revx.repl`) checks it, and fails when they are enabled regardless.

Both loaders, `tools/nucleo_load.sh` and `tools/unoq_load.sh`, now refuse a load when the
core is in an exception after their "reset halt" (ICSR.VECTACTIVE not 0): the reset did
not reach it, as with JP2 off, and the image would start inside a handler.

### SleepU5 from the flash, for the PPK2

A current of a few µA in Stop 2 is to be measured after a power cycle with no debugger
connected since: ST's examples for the NUCLEO-U575ZI-Q ask for a power reset after a
load, a probe once connected leaving the debug domain powered until then, and the
DBGMCU registers survive anything less (RM0456, 75.12.4). An image in SRAM does not
survive that power cycle. `SleepU5Flash.elf` (`Examples/nucleo-u575`) is `SleepU5`
linked into the flash, `STM32U5_FLASH.ld`: its vector table at 0x08000000, where the
NUCLEO boots (FLASH_OPTR and NSBOOTADD0R read 0x1FEFF8AA and 0x0800007F on 2026-10-03),
its code and constants in the flash, its data copied into SRAM by `_OSResetHandler`. The
UNO Q keeps its images in SRAM, its flash holding Arduino's firmware. The NUCLEO's flash
as it was on 2026-10-03 is kept off the repository, to be written back.

Code run from the flash goes through ICACHE, which the port enables. On revision X, the
NUCLEO's, the first fetch from the cache line last used before Stop 2 may read wrong after
it (ES0499, 2.2.11): `Stop2Idle` disables ICACHE before its WFI and enables it again after,
ST's workaround. An image in SRAM, fetched through the S-bus, is not cached and did not
need it.

Under Renode, on the NUCLEO's platform, `SleepU5Flash` starts from the flash, VTOR at
0x08000000, and reports on USART1, its periods and events on time. Renode loads every
segment of the ELF, the data in SRAM included, so the copy from the flash is the board's
to check: on the NUCLEO on 2026-10-03, programmed with OpenOCD's `program`, it ran from a
reset with no debugger, 280 instances, 533 entries into Stop 2, none late.

### The NUCLEO's MCU measured with a PPK2 (2026-10-03)

A Nordic PPK2, firmware 1.2.4, is on the UNO Q's hub, read by `tools/ppk2_nucleo.py`
with IRNAS's ppk2-api 0.9.2. It is an ampere meter in place of JP5, the IDD jumper
(UM2861, 6.4.6): its VIN on the pin of JP5 that comes from JP4, the board's 3V3, its
VOUT on the pin that goes to the MCU, its GND on the board's. Its logic input D0 is on
D13 (CN7, pin 10), which `SleepU5` built with `PHASES=30` drives high in its 30 s of Stop
2 and low in its 30 s of Sleep, its VCC on the board's 3V3 (CN8, pin 7). JP4 on 3V3, the
MCU at 3.3 V; VDDA is on the board's 3V3, not behind JP5 (SB54, UM2861 table 9), and is
not in the figures.

The order that measures: `SleepU5Flash` programmed with JP5 fitted; CN1 unplugged, so
that the debug domain the probe powered goes down; the PPK2 in place of JP5, its switch
closed; CN1 plugged back, the MCU starting from its flash with no debugger since. Three
things that read nothing on the way, each mistaken for something else first
(`method.md`): VIN and VOUT the wrong way round, where the MCU still runs through the
body diode of the PPK2's switch and the PPK2 reads 0; the PPK2 as a source on VDD_MCU,
which the board then feeds by another way, the PPK2 supplying nothing once CN1 is in;
and the debugger, which never reached the MCU through the PPK2, only with JP5 fitted.

| `SleepU5`, one of each 30 s phase, 3.3 V | LDO | SMPS (`make SMPS=1`) |
|---|---|---|
| Sleep phase, mean | 10.97 mA | 6.20 mA |
| Stop 2 phase, mean | 1.02 mA | 0.60 mA |
| Stop 2, the level a long sleep reaches | some 21 to 22 µA | some 6.5 to 7.5 µA |

The means of the Stop 2 phase are mostly the time awake: two wake-ups every 100 ms, each
in Sleep for the 3 ms before its event at some 11 or 6 mA, the wake-up itself, 58 ticks
of LPTIM1 waiting for the HSE, and a report on USART1 each second. The level of Stop 2
itself, every SRAM retained, compares with the datasheet's (DS13737, Stop 2: some 20 µA
on the LDO, 8.2 µA on the SMPS at 25 °C). Each sleep reads low first, some 1.4 µA, and
climbs to that level over some tens of ms: for a small current the PPK2 takes a large
shunt, through which the MCU's decoupling capacitors charge back. That shifts charge in
time and leaves the means right; the level is read at the end of a long sleep, and a
short sleep never reaches it. The temperature was a living room's, not measured. Each
figure is one run.

The margin of the wake-up, `OS_STOP2_WAKE_US`, 3 ms, is the Sleep that follows each
wake-up until the event, at some 6 mA on the SMPS. `OSSetStop2Wake()` sets it at run time,
and `SleepU5` built with `PHASES=30 WAKE=2200` alternates 30 s at 3 ms, D13 high, and 30
s at 2.2 ms, D13 low, both in Stop 2. On the NUCLEO on the SMPS on 2026-10-03, four
phases of each: 588 µA at 3 ms, 500 µA at 2.2 ms, some 110 µA a millisecond of margin
at its twenty wake-ups a second, and no wake-up late over 2,720 instances, the longest
62 ticks of LPTIM1, 1.89 ms, its HSE starting. The default stays 3 ms; the UNO Q's
wake-up, 19 ticks, 0.58 ms, would need far less.

### Slower clocks, measured (2026-10-03)

`make MHZ=80`, `40` or `16` builds every example at that speed instead of 160 MHz
(`OS_SYSTEM_CLOCK_HZ`, `Escapement_Processor.c`): PLL1 divided by 4 or 8 in voltage
range 2 or 3 without the booster, or the HSE itself in range 4, with the flash's and
the SRAM's wait states those ranges need (RM0456, tables 54 and 47). Renode's platform
cannot check them, its clocks fixed; the NUCLEO did. `SleepU5Flash` built with
`PHASES=30 SMPS=1` at each speed ran its 150 instances on time, none late, the HSE
never missed, and the PPK2 then read, one run each, the same order as above:

| `SleepU5`, SMPS, 3.3 V | 160 MHz | 80 MHz | 40 MHz | 16 MHz |
|---|---|---|---|---|
| Sleep phase, mean | 6.20 mA | 3.77 mA | 2.71 mA | 1.53 mA |
| Stop 2 phase, mean | 0.60 mA | 0.41 mA | 0.35 mA | 0.27 mA |
| Run, the median of the phase that computes | 10.68 mA | 5.66 mA | 3.64 mA | 1.82 mA |
| Run, a cycle | 67 pC | 71 pC | 91 pC | 113 pC |
| On the LDO: Stop 2 phase, mean | 0.98 mA | 0.65 mA | 0.50 mA | 0.36 mA |
| On the LDO: Run, the median | 20.44 mA | 10.50 mA | 6.20 mA | 2.89 mA |
| On the LDO: Run, a cycle | 128 pC | 131 pC | 155 pC | 180 pC |

Half the speed saves 39 % of the Sleep, a tenth of it 75 %: part of the current does not
follow the clock. The Stop 2 phase is mostly the Sleep of the wake-up's margin, which
costs less slower; the task's work, a few µs an instance, is too little for a slower
core to show its price there. The Run rows are `SleepU5Flash` built with `RUN=80000`
too: its second phase computes for 80 ms of each 100 ms instead of sleeping, a loop on
a variable on the stack, and the median of that phase is the core running, its mean
the 20 % left in Sleep beside (9.81 mA at 160 MHz, 0.8 × 10.68 + 0.2 × 6.2). The LDO
rows are the same images built without `SMPS=1`. The same day, same order, one run
each.

A cycle costs least at 160 MHz, in voltage range 1 with the booster: the lower ranges
do not save what the part of the current that does not follow the clock costs over the
longer time. A given work done at 160 MHz and followed by Stop 2, some 5 to 7 µA, takes
6 % less charge than at 80 MHz, 27 % less than at 40, 41 % less than at 16. On the LDO,
the U585's only regulator, a cycle costs twice as much and the order is the same: 2 %
less at 160 than at 80, within what one run tells, 17 % less than at 40, 29 % less than
at 16, where its datasheet's figures, 84 and 73 µA/MHz, put a cycle 13 % cheaper at 24
MHz than at 160 (`power-aware.md`). DVFS, the work done slower, gains nothing on either
regulator; racing to Stop 2 is the best of the four, at every load. What a slower clock does save is the time
awake doing nothing: the margin of each wake-up, 0.60 mA in the Stop 2 phase at 160 MHz
against 0.27 at 16. Two things the check of `tools/unoq_sleep.py` caught at 16
MHz, fixed since:

- TIM2 lost some 30 cycles a wake-up, those between the edge of LPTIM1 that ends the
  sleep and its start, made up by none: −6.1 ppm against LPTIM1 at 80 MHz, −14.2 at 40,
  −38.5 at 16, at 19 wake-ups a second, and 3 to 5 ppm at 160 before. `Stop2Idle` gives
  them back (`RESTART_CYCLES`); the four speeds then read −2 to −5 ppm over 30 s, none in
  the order of the speed.
- The way of a timer event's interrupt to its task, some 800 cycles, took 49 µs at 16
  MHz where the check allowed 20; it allows 1,600 cycles now, `MHZ=16` in its
  environment.

## Errata

The errata sheet of the chip, ES0499 (rev. 12, June 2026), was read against the port on
2026-09-26. The UNO Q's STM32U585 is revision U (DBGMCU_IDCODE 0x30076482). One erratum
touches the port, and only the accuracy of its clock, and that only should the HSE not
start. The NUCLEO-U575ZI-Q's STM32U575 is revision X (0x20016482); its errata were read on
2026-10-02: none of those it adds touches the port, but 2.2.27's workaround is not
available there (`SleepU5 on the NUCLEO-U575ZI-Q` above).

### 2.2.27, spurious MSI PLL unlock

The MSI may leave its PLL mode on an LSE failure that it detects wrongly, more likely
when cold and at a low core voltage. The MSIS then runs free again, 0.48 % fast on this
board. The kernel's clock follows it only when PLL1 had to take the MSIS. The port takes
ST's workaround. The unlock raises line 23 of the EXTI and interrupt 125 (RM0456 rev. 7,
tables 118, 186 and 189). The CMSIS header of the U585 names the interrupt
`LSECSSD_IRQn`, after the LSE's clock security, which shares it, and has no name for the
unlock nor for line 23: this page said until 2026-09-29 that it named neither. Its handler turns the PLL mode off and on again and counts the event, and
`SoakU5` reports the count. On 2026-09-26, with the core halted, line 23 was raised by
software on the board. The handler counted it once, cleared the pending flag and turned
the PLL mode on again. The halt itself cost the link a byte, as a halt alone did
afterwards.

### Errata the port stays clear of

The others that come near the port do not touch it. The LPTIM1 driver, added on
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
