# State of the project and direction

Escapement continues ZottaOS, a real-time kernel by Claude Evéquoz and Bertrand Hurst,
whose code last changed in 2014 and was published in 2016.
`NOTICE` gives the lineage and the third-party components.

This page separates what is open from what is done. Open items are plans, each with the
date its facts were read. Done items say where the result is measured or checked, and a
result counts as measured only where a page gives its date and conditions. Last revised
on 2026-10-05.

## Direction

Development goes to **ARM Cortex-M**, and the power-aware variant is the point of the
project. Its target is the **RP2040**. `power-aware.md` argues that dynamic voltage and
frequency scaling has a niche there because the chip sleeps poorly, and it is the board
at hand.

New work goes to the **Pico**, the **Pico 2** and the **STM32U5**. The RP2350 of the
Pico 2 has a switching core regulator and two cores with exclusive accesses. Its port was
begun on 2026-09-24 for the cores. A DVFS driver is not planned for it: measured on
2026-10-03, it would save some 10 % at best, less than sleeping deeper does (items 3
and 5).

Among the STM32, the **STM32U5** is the one kept. Its port, begun on 2026-09-25 for the
STM32U585 of the Arduino UNO Q (`stm32u5.md`), was written anew in the manner of the
RP2350's. The older STM32 ports went on 2026-09-26 (9783ab4), the F4 last, once the U5
ran on its board with a check of each commit. The U5 sleeps too well for DVFS to gain
(`power-aware.md`): measured on 2026-10-03, a cycle costs least at 160 MHz, and racing
to Stop 2 beats every slower speed on its SMPS and its LDO alike (item 4). The
**STM32L4** is set aside.

## Open work, in order

0. **Two weeks of endurance on a board** (`rp2040.md`, "The endurance test"). `SoakPico`
   runs on the Pico W, on a probe of its own, since 2026-09-28 at 20:03 UTC: a week under
   the hard kernel, EDF, at 69825e4, then a week under the power-aware one. On 2026-10-04
   at 22:36 UTC it had run 6 days 1 h 48 min across 488 wraps of the kernel clock, with
   no error and no restart. Two STM32U5 run alongside until stopped (`stm32u5.md`, "The
   endurance test"): the UNO Q's own, restarted by each board check on its commit, and a
   NUCLEO-U575ZI-Q, 25 h 56 min across 86 wraps at f197a35 on 2026-10-04, no error and no
   overrun. The statuses `board/soak`, `board/soak-u5` and `board/soak-nucleo` of each
   commit give their state since (`tools/board_ci.md`).

1. **The energy verdict on the RP2040.** Build the bench of `power-aware.md`: a plain Pico
   rather than a Pico W, powered and measured by a Power Profiler Kit II, chosen on
   2026-09-24 over an INA226. Then answer whether DVFS beats race-to-sleep. The same bench
   settles what the documentation leaves open:
   - whether the idle task should sleep at 12 MHz rather than 125 (`rp2040.md`);
   - whether DRA, DR_OTE or DM_SLACK save anything over OTE;
   - how long the regulator really takes to settle;
   - whether the core, undervolted, still computes right, checked by a computation whose
     result is verified.

   For the idle task, the RP2040 datasheet (build of 2025-02-20) was read on 2026-09-27.
   SLEEP is entered when both cores wait in WFI. It gates the clocks the SLEEP_EN
   registers name, while the oscillators and the PLLs run on and the state is kept
   (§2.11.2, p. 161). The kernel's timer and its tick, from clk_ref, can stay on
   (SLEEP_EN1, CLK_SYS_TIMER, p. 211-213). The kernel's time then stays exact, and its
   alarm can wake the core. The datasheet's example of SLEEP draws 0.39 mA typical, with
   every clock on the 12 MHz crystal, the PLLs stopped and the timer gated off, against
   9.0 mA idle in BOOTSEL (table 637, p. 623). DORMANT stops every clock, clk_ref and the
   timer with them (§2.11.3). A timed wake-up from it can come only from the RTC, to the
   second, and only on an external clock on a GPIN (§4.8). That is no use to an idle task
   that keeps time to the microsecond. No erratum touches SLEEP, DORMANT or the RTC.
   The idle task in SLEEP (`make SLEEP_GATE=1`) kept every deadline on the board on
   2026-09-28 (`rp2040.md`, "SLEEP rather than WFI alone"); what it draws is for the
   bench to measure.

   Built with `SLEEP_SPEED=0`, the power-aware kernel's idle task sleeps at 12 MHz with
   the PLL still locked (`power-aware.md`); the 0.39 mA had it stopped, and the datasheet
   gives no lock time (§2.18). The steps on the bench, then:
   1. the idle task as it is, at 125 and at 12 MHz;
   2. SLEEP, with the timer and the interrupts in use left on; the tick, from clk_ref,
      needs no more (`rp2040.md`, "SLEEP rather than WFI alone");
   3. the PLL's lock time, to decide whether a long sleep pays for stopping it, as the U5
      stops its clocks in Stop 2.

2. **The Pico 2 on the board — done on 2026-09-30.** Every example has run on a Pico 2,
   and the bench checks six of them and the UART at each commit (Done). Left: the
   power-aware kernel and DVFS (item 3).

3. **DVFS on the RP2350 — measured on 2026-10-03, worth some 10 % at best.** Its
   regulator and its power manager differ from the RP2040's, and a driver would be
   written from the pico-sdk headers. It is not planned: SLEEP comes first (item 5).

   The Pico 2 measured on 2026-10-03, `SleepPico2` built with `RUN=1`: the core
   computing without a pause, 30 s at each point, the board powered by the PPK2 at 5 V
   on VSYS, its regulator included; 75 and 37.5 MHz are PLL_SYS's 150 divided in
   clk_sys, the VCO still at 1.5 GHz, and 12 MHz the crystal with PLL_SYS stopped:

   | core voltage | 150 MHz | 75 MHz | 37.5 MHz | 12 MHz |
   |---|---|---|---|---|
   | 1.10 V | 15.99 mA, 107 pC a cycle | 8.84 mA, 118 pC | 5.79 mA, 154 pC | 2.94 mA, 245 pC |
   | 1.00 V | | 7.60 mA, 101 pC | 5.11 mA, 136 pC | 2.69 mA, 225 pC |

   As on the U5 a cycle costs least at full speed at a given voltage; what decides is
   the sleep the time saved goes to. Racing at 150 MHz beats 75 MHz at 1.10 V only if
   that sleep draws under 1.7 mA, and 37.5 MHz under 2.4: SLEEP, 4.2 mA (item 5), does
   not, DORMANT, 0.36 mA, does. With a tenth of the processor busy and the idle task in
   SLEEP, 75 MHz saves some 5 % and 37.5 some 10 %, at 1.10 V; SLEEP itself took 61 %
   off WFI. At 1.00 V, 75 MHz costs less a cycle than 150 at 1.10 and beats racing to any
   sleep, but the datasheet guarantees 1.1 V only (6.3.2), and its brown-out detector
   resets the chip under some 0.95 V: a bench setting, as UNDERVOLT is on the RP2040.
   Within the specification, then, DVFS on the RP2350 is worth some 10 % if the idle
   task sleeps in SLEEP with PLL_SYS running, and next to nothing once it stops PLL_SYS,
   1.99 mA, just over the 1.7 that 75 MHz needs, or once DORMANT keeps the time (item 5,
   step 3). SLEEP comes first, PLL_SYS stopped.

4. **The STM32U5's energy.** The idle task sleeps in Stop 2 on the board since 2026-09-27
   (see Done). Its current was measured on a NUCLEO-U575ZI-Q with a PPK2 on 2026-10-03
   (`stm32u5.md`, "The NUCLEO's MCU measured with a PPK2"): Stop 2 at some 21 µA on the
   LDO and 7 µA on the SMPS, `SleepU5`'s Stop 2 phase at 1.02 and 0.60 mA against 10.97
   and 6.20 mA in Sleep, at 3.3 V. Left: the UNO Q's own U585, which only a difference
   can show, and the PPK2 as a check of each commit. The margin of the wake-up was priced
   the same day: some 110 µA a millisecond at twenty wake-ups a second on the SMPS, 3 ms
   where 2.2 sufficed on the NUCLEO; `OSSetStop2Wake()` sets it per board, the default
   left at 3 ms. Slower clocks, `make MHZ=80`, `40` or `16`, ran on it the same day: the
   Sleep phase at 3.77, 2.71 and 1.53 mA on the SMPS, the Stop 2 phase at 0.41, 0.35 and
   0.27 mA (`stm32u5.md`, "Slower clocks, measured"). The core computing costs 67 pC a
   cycle at 160 MHz, 71 at 80, 91 at 40, 113 at 16, and on the LDO, the U585's, 128,
   131, 155 and 180: DVFS gains nothing on the U5, racing to Stop 2 wins at every load,
   and no power-aware kernel is planned for it. What a slower clock saves, the margin of
   the wake-up, the idle task now sleeps on the MSIS at 4 MHz until 500 µs before the
   event: the Stop 2 phase at 0.48 mA rather than 0.60 on the SMPS, 0.78 rather than 1.02
   on the LDO (`stm32u5.md`, "The margin slept on the MSIS"). The datasheet, read on
   2026-09-26 (`power-aware.md`), puts it at some four times less current than Sleep at
   light load, some 5.3 against 1.4 mA with a tenth of the processor busy. The UNO Q's
   U585 has no SMPS.

   The plan for Stop 2, as read on 2026-09-26, and the two parts of it that changed the
   next day are in `stm32u5.md`, "The plan, as read on 2026-09-26".

5. **A deeper sleep on the RP2350.** Its idle task sleeps in SLEEP, PLL_SYS stopped,
   with `make SLEEP_GATE=1` (Done); DORMANT is left. The RP2350 datasheet was read on
   2026-09-27:
   - DORMANT stops every oscillator and keeps the state, the code going on after the
     instruction that entered it (§6.5.3, p. 489-490), as Stop 2 does on the U5;
   - TIMER0, the kernel's, stops with them (p. 570);
   - an alarm of the always-on timer or a GPIO wakes the chip;
   - the PLLs must be stopped before and restarted after, and the 12 MHz crystal takes
     more than 1 ms to start (p. 557);
   - the power-down states P1.x reboot through the bootrom instead (p. 446), which is no
     use to an idle task;
   - no erratum touches DORMANT, SLEEP or the always-on timer.

   Two things differ from the U5. The always-on timer counts in steps of 1 ms, 62.5 µs at
   best, and the Pico 2 has no 32.768 kHz crystal. The timer then runs on LPOSC, an RC
   oscillator within 20 % untrimmed and 1.5 % trimmed, drifting 14 % with temperature
   and 20 % with the supply (§8.4.1, p. 569-570; §12.10.5.1 says 1 %). The U5's crystal
   held 15 ppm. A sleep of 100 ms could move the kernel's time by some 1.5 ms. Second,
   the datasheet gives no current for WFI, SLEEP or DORMANT (§14.9.7), only 11 mA for a
   core at 150 MHz (p. 1347). The steps:
   1. measure the three on a Pico 2 with the PPK2 — done on 2026-10-03 (Done);
   2. if SLEEP saves enough, gate the clocks the kernel does not need while TIMER0 runs
      on (SLEEP_EN1, p. 550), which keeps the time exact — done on 2026-10-04 (Done);
   3. for DORMANT, either calibrate LPOSC against the crystal before each sleep and
      measure the error left, or give the always-on timer an external 32.768 kHz clock
      on GPIO 12, 14, 20 or 22 (§12.10.7), which is hardware for the bench.

   Left: step 3. The measurements and the gate are in Done, "Sleep on the RP2350,
   measured and gated". Its first half measured on 2026-10-05: `LposcPico2` (Examples/
   pico2) counts the always-on timer, on LPOSC at its nominal 32.768 kHz, against TIMER0
   on the crystal, in windows of 60 s, awake, at room temperature. On the bench's Pico
   2, 35 windows: LPOSC at 30.099 kHz, 8.1 % slow; from one window to the next it moved
   135 ppm on average and 432 at most, against 18 ppm of resolution. Calibrated a
   minute before, a sleep of 100 ms would then be off by 43 µs at most, and one of a
   second by 0.4 ms, where the uncalibrated 8 % gives 8 ms. FC0 alone cannot calibrate
   it: its 1/32 kHz is some 1,000 ppm of LPOSC; the comparison with TIMER0 can. Left:
   the same across temperature and supply, and asleep, where the timer runs from LPOSC
   with clk_ref on it (`SleepPico2.c`).

6. **LPUART1 at 115,200 baud through Stop 2 — done on 2026-10-02** (Done, "LPUART1 at
   115,200 baud through Stop 2"). Left: how often a wake-up byte comes out wrong, which
   no check of a minute can bound.

## Done

- **No periodic tick, checked (2026-10-05).** The host test's trace prints each
  interrupt of the comparator, and `tools/differential.py` fails a trace in which the
  timer interrupts where nothing is released, or twice at an instant. Every build
  passes; a host port made to interrupt every 500 ticks fails its first task set.
- **A day of endurance under Renode (2026-10-04).** Four instances, each with its own
  build and seed, the hard, soft, deadline-monotonic and DRA kernels, ran a day of
  virtual time each at 1b67f30, 80 wraps, with no error (`emulation.md`, status
  `emulation/soak`).
- **The Pico 2 on the board (2026-09-28 to 30).** On probe3, driven by Raspberry Pi's
  OpenOCD: its clocks right, the six examples that count in memory passing the criteria
  of the Renode suite, the litmus tests showing no reordering between the cores, the
  periods right on the frequency counter, the UART echo and the two senders through the
  probe's UART, and `SoakPico2` across two 2^30 wraps in 40 minutes without an error
  (`rp2040.md`, "The Pico 2 on the board"; `tools/pico2_check.py`,
  `tools/pico2_uart.py`, `tools/pico2_soak.py`). The bench checks all of it but the
  wrap, 18 minutes a time, at each commit (status `board/pico2`).

- **A stack that faults (2026-10-04 and 05).** The one stack grows down toward the
  globals, with nothing between them; ZottaOS's comment said an overflow corrupted
  nothing. Since 2026-10-04 the Cortex-M33 ports set MSPLIM at the end of the globals
  (`_OSResetHandler`), which the Renode suites of the Pico 2 and of the STM32U5 still
  pass. Renode does not model MSPLIM, so that the limit holds is for the board to show.
  On the Pico 2 it did, on 2026-10-04, `StackGuardPico2` loaded on probe3 from the Mac
  over the gateway: with the limit, the core in its HardFault handler, CFSR 0x00100000
  (STKOF) and HFSR FORCED, the main stack pointer stopped at 0x20001600 above the end of
  the globals at 0x20001210, 513 levels deep; built without it, the stack went three
  levels further, to 0x20000a24, through the globals and into the image's code, which
  runs from SRAM below them, and the core was lost at 0x20000fce with no fault recorded.
  `Sentinel` read 0xA5A5A5A5 both times, under Renode too: it is no witness, the stack
  pointer is. On the STM32U5 of the UNO Q too, `StackGuardU5`, the same evening: with
  the limit, the HardFault handler, CFSR 0x00100000 (STKOF), HFSR FORCED, the main stack
  pointer at 0x20001b70 above the end of the globals at 0x20001878, 759 levels; without
  it, three levels further, to 0x20000f70, `Sentinel` written over (0xFAFAFAFA, the low
  byte of level 762) and the core faulting on an undefined instruction, the stack's
  bytes run as code, which says nothing of the cause. The Cortex-M0+ of the RP2040 has
  no stack limit: since 2026-10-05 the linker script keeps 1 KB between the globals and
  the stack, which a region of the MPU closes to any access. On the Pico (probe1),
  `StackGuardPico` ended in the HardFault handler, 507 levels of 512 bytes, the stack
  pointer at 0x20001780 inside the region, above the end of the globals at 0x20001258,
  `Sentinel` kept; without the region, five levels further, to 0x20000f84, through the
  globals and into the code, `Sentinel` written over (0xFFFFFFFF) and the core still
  recursing. The core does not lock up, as was expected: HFNMIENA at 0 turns the MPU off
  at the HardFault's priority, and its stacking lands in the region. A frame of 1 KB or
  more steps over it: GCC first put two levels of the test in one frame of 1,536 bytes,
  which wrote over `Sentinel` in the deadline-monotonic build under Renode; the test now
  keeps its function from being inlined. Renode stops the overflow too, but in a
  MemManage handler, a fault ARMv6-M does not have. `SoakPico` paints and reads its
  stack from the end of the region.

- **Sleep on the RP2350, measured and gated (2026-10-03 and 04)**, steps 1 and 2 of item
  5. `SleepPico2` (Examples/pico2) measured WFI, SLEEP and DORMANT on 2026-10-03: no
  kernel, some 100 µs of work every 100 ms on core 0, core 1 off, the ring oscillator,
  PLL_USB and the USB, ADC and HSTX clocks stopped, 30 s of each phase told apart on the
  PPK2's D0 and D1. The PPK2 powered the board at 5 V on VSYS, its regulator included,
  the LED off; three phases of each, one run:

  | | mean | median, the level between wake-ups |
  |---|---|---|
  | WFI, every clock running | 13.15 mA | 12.6 mA |
  | SLEEP, every clock gated but the tick and TIMER0 | 5.10 mA | 4.2 mA |
  | DORMANT, the crystal and PLL_SYS stopped | 1.29 mA | 0.36 mA |

  SLEEP alone takes 61 % off WFI and keeps the kernel's time exact: step 2 is worth
  doing. DORMANT's mean is mostly its wake-ups, the crystal's start of 6 ms (its STARTUP
  delay, 6 times the millisecond, as the port sets it) and PLL_SYS locked again, every
  100 ms. Two things met on the way. The always-on timer's alarm did not wake this
  DORMANT: its count runs on LPOSC, but its alarm is compared on the power manager's
  clock, which follows clk_ref, here the stopped crystal; it fired awake and never
  asleep. The image is woken instead by a byte the UNO Q sends every 100 ms on the
  probe's UART, whose falling edge on GP1 is a DORMANT wake-up (`tools/
  pico2_sleep_load.sh --wake`). A kernel would run clk_ref from LPOSC first, as the
  pico-extras do, or take step 3's external clock.

  The cheaper sleeps a kernel could take, `make DEEP=1`, the same night, three phases of
  each, one run:

  | | mean | median |
  |---|---|---|
  | SLEEP, PLL_SYS running (the first run read 5.10 mA) | 4.47 mA | 4.1 mA |
  | SLEEP, clk_sys on the crystal, PLL_SYS stopped, locked again on waking | 1.99 mA | 1.4 mA |
  | DORMANT, the crystal's start at 1 ms rather than 6 | 1.23 mA | 0.35 mA |

  Stopping PLL_SYS through the sleep takes 55 % more off SLEEP, 85 % off WFI in all,
  TIMER0 still counting the crystal: that is the idle task to write, as the U5's sleeps
  its wake-up's margin on the MSIS. The crystal's start is a small part of DORMANT's
  mean, 1.29 to 1.23 mA; the rest of its wake-ups was not told apart.

  And TIMER0 ran at half speed: the
  firmware in the Pico 2's flash leaves clk_ref divided by 2, which a debugger's reset
  keeps, and both ports never set the divider back; they do since, for clk_ref, clk_sys
  and, on the RP2350, clk_peri.

  Step 2 was written on 2026-10-04: `Escapement_SleepGate.c` on the RP2350, as on the
  RP2040, its idle task called through `_OSIdleHook`. It gates every clock but those the
  image keeps, moves clk_sys onto the crystal and stops PLL_SYS, then sleeps until
  150 µs before the first alarm of TIMER0 armed, on its alarm 3, and locks the PLL
  again on waking. ALARMn cannot be read back: the port keeps what it wrote in
  `_OSAlarmTime`. `IdlePico2` (Examples/pico2), a task of some 100 µs every 100 ms and a
  timer event 40 ms after each, measured on the PPK2 as `SleepPico2` was, 30 s of each,
  one run:

  | `IdlePico2` | mean | median | instances' jitter, events' offset |
  |---|---|---|---|
  | WFI (`make`) | 13.76 mA | 13.3 mA | 1 µs, 5 µs |
  | SLEEP, PLL_SYS stopped (`make SLEEP_GATE=1`) | 2.46 mA | 1.8 mA | 0 µs, 4 µs |

  82 % off the idle task's current, the kernel's time kept: the jitter and offsets were
  read while it ran, 251 instances. Without the early wake-up the PLL's lock made them
  56 and 61 µs. Two defects met on the way, both fixed in the RP2040's port too: an image
  loaded after one that gated its clocks slept with the bus gated, SLEEP_EN outliving
  the debugger's reset, which `OSInitializeSystemClocks` now sets back; and core 0, its
  gates and deep sleep already set, entered SLEEP while waiting for core 1's bootrom on
  the inter-core FIFO, whose clock was gated: `OSInitSleepGate` launches core 1 first.

- **LPUART1 at 115,200 baud through Stop 2 (2026-10-02).** It ran at 57,600 until then,
  and 57,600 stays the default; `SleepU5` and `SoakU5` run at 115,200 (below). Its
  margin on waking was read on 2026-10-02 (`stm32u5.md`, "LPUART1 through Stop 2"):
  - with its FIFO on, an overrun comes when a byte is complete and the 8 places are
    full (RM0456, LPUART, "Overrun error"): from the byte that wakes the chip, 8 frames,
    1.39 ms at 57,600 baud, 694 µs at 115,200;
  - interrupts stay masked meanwhile: leaving Stop 2, 20 to 60 µs (DS13086, table 74),
    then `_OSRaiseSystemClock`, 18 to 29 ticks of LPTIM1 measured over 454,487
    wake-ups, at most 916 µs, then the timer's interrupt, of a higher priority: some
    1.0 ms, a margin of about 30 % at 57,600 baud and none at 115,200;
  - the HSE dominates, and has no maximum: 2 ms typical, "can vary significantly with
    the crystal manufacturer" (DS13086, table 80). On this board at room temperature it
    has always started in less, which nothing guarantees elsewhere or in the cold.

  `SoakU5` never enters Stop 2, its pulse due every millisecond: the overrun of
  2026-10-01 is not this. The ways weighed that day:
  - hardware flow control, PG6, the LPUART's RTS and the Linux side's CTS, left to the
    LPUART: the FIFO never overflows, but the first byte is still sampled while HSI16
    starts, which keeps 57,600 baud;
  - PG6 held at "stop" in Stop 2, with two frames of silence checked before, and "ready"
    after the clock is raised: 115,200 baud and some 20 µA, but the UART no longer wakes
    the chip, Linux waiting up to the length of a sleep, 1.83 s at most;
  - HSI16 kept on in Stop 2 (HSIKERON): 115,200 baud and the UART wakes the chip, at
    some 150 µA against 20.5, and the FIFO still needs flow control or draining;
  - the LPDMA, autonomous in Stop 2, filling a ring in SRAM4: no limit, a new receive
    path;
  - the FIFO drained in the waits of `_OSRaiseSystemClock`, the handler then called with
    the kernel's clock stopped;
  - a wake-up byte, followed by an acknowledgement from the MCU before the message.

  The way chosen and built on 2026-10-02: **a wake-up byte without acknowledgement**.
  The client sends one byte, waits T, then its message. It holds on three conditions:
  1. T is bounded by the code, not only measured: the wait for the HSE capped at 64
     ticks of LPTIM1, 1.95 ms, beyond which that wake-up goes on PLL1 from the MSIS, as
     on a board without the HSE, 160.017 MHz, some 100 ppm off, the HSE tried again at
     the next; until then `NoHSE` gave it up for good, after some 20 ms, as it still does
     at reset. T is then leaving Stop 2, the capped HSE, and
     the maxima of the voltage range, booster and PLL1 lock, to read in DS13086; some
     5 ms for the client;
  2. the MCU stays awake W after each byte received, `_OSUARTIdle` refusing Stop 2
     meanwhile, else it sleeps again as soon as the wake-up byte is read. W covers T, the
     jitter of a Linux process and the gaps in a message, some 20 ms;
  3. the wake-up byte can be dropped without doubt. Sampled while HSI16 starts, its value
     is anything; received awake, it looks like data; and "the first byte after a
     silence" does not tell it apart, the message's first byte coming T after it. A
     framed protocol does, COBS with a CRC: the client sends 0x00, waits T, then 0x00,
     the frame and 0x00, and whatever the wake-up byte became ends in an empty or invalid
     frame, dropped as noise.

  This keeps 115,200 baud, 20 µA in Stop 2, the UART waking the chip in some 5 ms, and
  relies on no flow control. The order: the cap on the HSE first, which also keeps one
  slow start from leaving the chip on the MSIS for good, testable under Renode, done on
  2026-10-02 (`stm32u5.md`, "A bound on the HSE's start") and seen on the board the same
  day; then the window W, done under Renode on 2026-10-02 (`stm32u5.md`, "The window
  after a byte"); then the link of `SleepU5` and `tools/unoq_sleep.py` framed, at
  115,200 baud, done under Renode on 2026-10-02 (`stm32u5.md`, "The wake-up byte"),
  both seen on the board the same day, every byte received at 115,200 through Stop 2.
  `SoakU5`, which never enters Stop 2, moved to 115,200 too on 2026-10-02, its link
  raw as before. Awake, it ran without an overrun up to 921,600 baud the same day
  (`stm32u5.md`, "Faster, awake"): a rate above 115,200 is open to a link that needs
  it.

- **The order between the cores, as far as it can be verified (closed 2026-09-30).** The
  queue between the cores (`Escapement_CoreQueue.c`) had a DMB between any two of its
  accesses to different words, fifteen in all. A model of weakly ordered cores
  (`test/model/fifo_mp.py`, 2026-09-26) kept six (a6f8b6e, 2026-09-27). On 2026-09-29 it
  stopped assuming that a store is never performed before an SC ahead of it, which
  Armv8-M does not promise (DDI0553B.y, B7.2.3): two more were needed, after E15 and D15,
  and the one before a dequeue returns went. The queue has seven, each shown needed and
  the seven enough at the full windows (`fifo_mp.py --wide`, some 40 minutes; the CI
  explores a part). The 3-slot buffer holds under the same model (`threeslot.py`): after
  its SC each core reads Reading again, and what it stores next takes its slot from that
  read. Since 2026-09-30 the CI holds the queue's compiled code to the same seven, as it
  did the slot buffers', each barrier's removal caught (`tools/check_order.py`,
  `tools/check_order_mutants.sh`). On the Pico 2 the queue ran with each core both
  producer and consumer (`FIFOCoresPico2`, four runs on 2026-09-28), every record taken
  once, whole and in order, and the checks of each commit run it. No
  instrument can show more: the litmus tests found no reordering between the RP2350's
  cores in 69.4 million rounds each (`architecture.md`), and Renode's cores keep program
  order. The barriers stay because the architecture allows what the chip did not show;
  the models are what shows each one needed.

- **Response times against their analysis, on the Pico (2026-09-27, 42db832).** The
  bound of ZottaOS's manual (eq. 2.3), the kernel's costs taken from the trace (7 to
  16 µs from an alarm to the task it releases), held for every response seen under DM
  and EDF: 58, 140 and 526 µs under DM, 58, 141 and 527 under EDF, against bounds of
  123, 220 and 620 µs, the last at the 60 ms task's longest instances, traced four
  minutes after start. A figure sets the
  schedule computed from the task set beside the trace; they agree within 2 µs
  (`rp2040.md`, "Response times against their analysis"; `tools/response_times.py`,
  `tools/schedule_figure.py`).

- **The STM32U5 port, under Renode (2026-09-25).** `Escapement/CORTEX-Mx/STM32U5` runs
  the hard and the soft kernel under both algorithms. It has its examples with the
  endurance test, and `SleepWrapU5` and `Stop2EventWrapU5`, scaled to cross the 2^30
  wrap under Renode. Its Renode platform is our own (`escapement_u5.repl`), and the suite
  passes under each of the four builds, in the CI (`stm32u5.md`).

- **The STM32U5 on the board (2026-09-26).** On the Arduino UNO Q, from SRAM. On the
  first day the clock set-up ran and `SoakU5` ran 22 s without error. The clock has come
  from the board's 16 MHz crystal since 2026-09-26, within some 25 ppm of NTP
  (`stm32u5.md`). A board check of each commit runs on the UNO Q's own Linux since
  2026-09-26 (`tools/unoq_check.sh`, status `board/u5`). Since 2026-09-27 it runs the
  idle task in Stop 2 for a minute (`SleepU5`), and since 2026-10-02 a minute more
  without the HSE (`SleepNoHSEU5`), then the endurance test for two minutes
  and the clock within 300 ppm over five, and the long endurance run then goes on with the
  commit's image.
  - Step 1 of the Stop 2 plan, the LPTIM1 driver on the LSE (`Escapement_LPTimer.c`) and
    its example `TestLPTimerU5`, done on 2026-09-27. On the board the LSE ran 15 ppm slow
    against the HSE over 30 s, with no compare missed. Under Renode, STM32L0_LpTimer
    serves.
  - Step 2, the idle task in Stop 2 (`Escapement_Stop2.c`, example `SleepU5`), done the
    same day. On the board: 388 entries in 43 s, every start one period after the last
    to the microsecond, the longest wake-up 885 µs, TIM2 2.1 ppm ahead of LPTIM1
    (`stm32u5.md`). Since that day TIM5 is carried through Stop 2 and LPUART1 receives
    through it (item 4).

- **The kernel runs, and is run in CI.** The Makefiles were repaired. Every example is
  built on each push to `main` and executed under Renode as a regression test (`emulation.md`,
  `emulation/renode/RP2040.md`):
  - the STM32F4 on Renode's platform with two fixes to its timer model, until its
    removal on 2026-09-26;
  - the RP2040 on the models of matgla/Renode_RP2040 with a fixed timer, since
    2026-09-21;
  - the RP2350 and the STM32U5 on platforms of our own.
- **Every kernel and algorithm.** EDF, which the examples claimed and none ran until the
  host test showed it, and deadline-monotonic. The hard and the soft kernel run on every
  target, the power-aware kernel on the RP2040 and the host, under its four policies
  (`architecture.md`, `method.md`).
- **The scheduler on the host.** Every kernel runs under AddressSanitizer, crossing the
  2^30 wrap three times. The host test also covers what no example runs: event-driven
  tasks, the queue, the slot buffers, tasks that take time (`test/host/README.md`).
- **Every interleaving of the lock-free mechanisms**, in exhaustive models. They found
  six defects, all fixed, two of them in the queue between the cores (`method.md`).
- **The RP2040 port on the board**: all three kernels, the DVFS driver on the silicon,
  the timer events, the cost of a scheduling round, the periods on a frequency counter,
  the regulator's response, the 4-slot buffer between the cores. All of it was watched
  through a trace rather than a halted core (`rp2040.md`).
- **Checks on the board on every change of `main`**, on the images the CI builds. The
  bench pulls them rather than being pushed to a self-hosted runner. Since 2026-09-26 the
  bench is an Arduino UNO Q, which checks the Pico (status `board/pico`), its own
  STM32U5 (status `board/u5`) and, since 2026-09-28, the Pico 2 (status `board/pico2`)
  (`tools/board_ci.md`).
- **The RP2350 port (from 2026-09-24).** The generic layer was taken to ARMv8-M, and both
  cores run under Renode, with `ACTLR.EXTEXCLALL` set and memory barriers between the
  cores (`architecture.md`). The CI checks their compiled order against the models
  (`tools/check_order.py`). The 3-slot buffer runs across the cores under Renode, with
  the RP2350's exclusive monitor played in place of Renode's (`emulation.md`). A FIFO
  queue between the cores adapts Figure 3 of Evéquoz's paper to the RP2350's LL/SC
  (`Escapement_CoreQueue.c`, `test/model/fifo_mp.py`).
- **`-O2` (2026-09-20)**, once the barrier that makes a pended exception take effect
  went in (`method.md`): 3.2 µs a round instead of 7.0.
- **The queue sentinels made whole task control blocks (2026-09-22).** Two defects came
  from reading a task field through the idle task, past a block allocated to the few
  fields it used. The head and the tail are now whole TCBs in `.bss`, for about sixty
  bytes of RAM per kernel. GCC's `-fanalyzer`, tried first, reported nothing on the code
  before or after, because the blocks came from `OSMalloc` and were reached through
  casts.
- **A defined start for the STM32 timer (2026-09-20).** `_OSStartTimer` clears the
  counter. Started under Renode at 0x3FFFF000, the kernel produced no output at all;
  since the fix, it produces the 164 pulses in 80 ms of a normal start. The U5 port's
  timer does the same.
- **A user guide**, `api.md`, written from the headers and checked against the code where
  they disagree. For instance, on Cortex-M the kernel does not mask the source of an
  interrupt, as the headers say of the original port.
- **Reported upstream (2026-10-02): the two defects of Renode's STM32 timer**, as
  renode/renode#1023, with the reproducer of `emulation.md`; dropped on 2026-09-24, taken
  up again once the defects were found still in Renode's `master`. The fixed copy lives in
  `emulation/renode`, where the CI loads it, now for the U5 platform, so nothing waits on
  the issue. The older STM32 port was kept at the time because, until the
  RP2350 port, it was the only one to run `LDREX`, `STREX` and `CLREX`.

## The MSP430 port was removed

Escapement began life on the TI MSP430. The port went on 2026-09-20 with its three
examples (8276933): 19,406 lines across 58 files. It had not been built once since the takeover. The original IAR and Code
Composer projects are not in the repository, nor is the configurator that produced its
per-derivative headers, and no test could reach it.

What was lost is the FRAM of the MSP430FRxx parts. Non-volatile, byte-addressable and
nearly free to write, it still has no equivalent for intermittent computing under energy
harvesting, and it was the one reason to keep the port. TI adds no new MSP430 families
and steers new designs towards MSPM0, a Cortex-M0+ the generic layer already covers. The
history keeps it all, and so does the archived `beber007/zottaos`.
