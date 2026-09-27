# State of the project and direction

Escapement continues ZottaOS, a real-time kernel whose development stopped in 2016;
`NOTICE` gives the lineage and the third-party components.

## Direction

Development goes to **ARM Cortex-M**, and the power-aware variant is the point of the
project. Its target is the **RP2040**: `power-aware.md` argues that dynamic voltage and
frequency scaling has a niche there because the chip sleeps poorly; it is also the board
at hand.

New work goes to the **Pico and the Pico 2** only. The RP2350 of the Pico 2 has a
switching core regulator and two cores with exclusive accesses. Its port was begun on
2026-09-24 for the cores; its DVFS driver waits for the RP2040 bench, and is written
only if that bench shows DVFS beating race-to-sleep.

Of the **STM32**, the **STM32U5** is the one kept: its port, begun on 2026-09-25 for
the STM32U585 of the Arduino UNO Q (`stm32u5.md`), is written anew in the manner of the
RP2350's; the older STM32 ports, the F4 example last, went on 2026-09-26, once it ran on
its board with a check of each commit. The argument of `power-aware.md` stands: the U5 sleeps too well for DVFS to gain
much there, so its power-aware kernel, if ever, comes after the verdict of the RP2040.
The **STM32L4** is set aside.

## Open work, in order

0. **Two weeks of endurance on a board** (`rp2040.md`, "The endurance test"): `SoakPico` on
   the Pico W, freed from the board CI by the Pico and the Pico 2 ordered, from the week
   of 2026-09-28 — a week under the hard kernel, a week under the power-aware one —, and
   instances under Renode on another machine, each with its own build and seed.

1. **The energy verdict on the RP2040.** Build the bench of `power-aware.md` — a plain
   Pico rather than a Pico W, powered and measured by a Power Profiler Kit II, chosen
   on 2026-09-24 over an INA226 — and answer whether DVFS beats race-to-sleep. The same
   bench settles what the documentation leaves open: whether the idle task should sleep
   at 12 MHz rather than 125 (`rp2040.md`), whether DRA, DR_OTE or DM_SLACK save
   anything over OTE, how long the regulator really takes to settle, and whether the
   core undervolted still computes right, checked by a computation whose result is
   verified.
2. **The Pico 2 on the board.** It needs a Pico 2, and an OpenOCD that knows the
   RP2350, which neither Homebrew's 0.12 nor Debian's does: Raspberry Pi's fork, built
   on the UNO Q on 2026-09-26 (`tools/board_ci.md`), waits for the board. The board alone can say that the clocks are
   programmed right, which the Renode platform acknowledges blindly; then the six
   examples, `ThreeSlotCoresPico2` first, and litmus tests of the order in which each
   core sees the other's accesses.
3. **What is left to verify between the cores.** The queue between the cores
   (`Escapement_CoreQueue.c`) had a DMB between any two of its accesses to different
   words, fifteen; a model of weakly ordered cores (`test/model/fifo_mp.py`, 2026-09-26)
   kept six, five of them shown needed and the six enough within its bounds, on a
   machine of 30 GB, where the CI explores only a part. The sixth, before a dequeue
   returns, could not be shown superfluous. `FIFOCoresPico2` makes each core a producer
   and a consumer of the same queues since 2026-09-26, as the model does. No Pico 2 has
   run the queue yet, and Renode, whose cores keep program order, cannot show the
   barriers at work.
4. **DVFS on the RP2350**, if the verdict of item 1 is for it: its regulator and its
   power manager differ from the RP2040's, and the driver is to be written from the
   pico-sdk headers.
5. **The STM32U5 on the board.** On the Arduino UNO Q since 2026-09-26, from SRAM: the
   clock set-up runs, and 22 s of the endurance test passed; since 2026-09-26 the clock
   comes from the board's 16 MHz crystal, within some 25 ppm of NTP (`stm32u5.md`). A
   board check of each commit runs on the UNO Q's own Linux since 2026-09-26
   (`tools/unoq_check.sh`, status `board/u5`): the endurance test for two minutes and the
   clock within 300 ppm, the long endurance run then carried on to the commit. The F4,
   the last older STM32 port, went the same day. For energy (`power-aware.md`, read from
   the datasheet on 2026-09-26): a time base on LPTIM1 and the 32.768 kHz crystal, which
   runs through Stop 2, and an idle task in Stop 2 rather than Sleep, some four times
   less current at light load; the UNO Q's U585 has no SMPS, and DVFS would add some 10 %
   at most.

   The plan for Stop 2, read from RM0456 rev. 7 and ES0499 on 2026-09-26. LPTIM1 cannot
   be the kernel's clock: 16 bits, wrapping every 2 s on the LSE in steps of 30.5 µs, its
   counter read twice to be trusted, and a new compare waited for (CMPOK) after a latency
   RM0456 does not quantify for it (§58.4). TIM2 stays the kernel's clock while running.
   The idle task, finding the next event far enough off, arms LPTIM1 on the LSE to wake it
   early and enters Stop 2; LPTIM1 wakes the chip from it (table 599). Stop 2 turns the
   HSE off and leaves the chip in range 4: waking takes the HSE's 2 ms, up to 47 µs for
   range 1, 50 µs for the booster and 25 to 50 µs for PLL1, so Stop 2 pays only for waits
   of some milliseconds, and TIM2 is then moved on by the time LPTIM1 counted, the idle
   task finishing the wait in Sleep. No Stop 2 while a timer event is pending, TIM5
   stopping with it — taken from the definition of Stop, not read for TIM5 itself — nor
   while a UART must receive: LPUART1 runs on PCLK3, so SoakU5, fed by Linux, would
   hardly ever enter it, and another example must show the gain. Errata: never clear
   LPTIM1's ENABLE, reset it through the RCC instead (2.17.1); writing DIER clears the
   flag it enables (2.17.3); a HardFault may follow a wake-up by LPTIM1, of the SRD
   domain, when debugging with DBG_STOP (2.2.19). Renode's STM32L0_LpTimer has the same
   first registers and may serve. The gain, some 5.3 against 1.4 mA at a tenth busy, is
   the datasheet's; the PPK2 is to measure it.

   Step 1, the driver of LPTIM1 on the LSE (`Escapement_LPTimer.c`) and its example
   `TestLPTimerU5`, done on 2026-09-27: on the board, the LSE 15 ppm slow against the HSE
   over 30 s, no compare missed; under Renode, STM32L0_LpTimer serves. Step 2, the idle
   task in Stop 2 (`Escapement_Stop2.c`, example `SleepU5`), done the same day: on the
   board 388 entries in 43 s, every start one period after the last to the microsecond,
   the longest wake-up 885 µs, TIM2 2.1 ppm ahead of LPTIM1 (`stm32u5.md`). Next, the
   PPK2.

## Done

- **The STM32U5 port, under Renode (2026-09-25).** `Escapement/CORTEX-Mx/STM32U5`: the
  hard and the soft kernel under both algorithms, eight examples with the endurance test,
  a Renode platform of our own (`escapement_u5.repl`) whose suite passes 9 tests of 9
  under the four builds,
  in the CI; not yet on a board (`stm32u5.md`).

- **The kernel runs, and is run in CI.** The Makefiles repaired, every example built on
  every push, and executed under Renode as a regression test: the STM32F4 on Renode's
  platform with two fixes to its timer model (until its removal on 2026-09-26), the RP2040 on the models of
  matgla/Renode_RP2040 with a fixed timer, the RP2350 on a platform of our
  own (`emulation.md`, `emulation/renode/RP2040.md`).
- **Every kernel and algorithm.** EDF, which the examples claimed and none ran until the
  host test showed it, and deadline-monotonic; the hard and the soft kernel on every
  target, the power-aware kernel on the RP2040 and the host, under its four policies
  (`architecture.md`, `method.md`).
- **The scheduler on the host**, every kernel under AddressSanitizer, the 2^30 wrap
  crossed three times, and what no example runs: event-driven tasks, the queue, the
  slot buffers, tasks that take time (`test/host/README.md`).
- **Every interleaving of the lock-free mechanisms**, in exhaustive models: four
  defects found and fixed, the last two between cores (`method.md`).
- **The RP2040 port on the board**: all three kernels, the DVFS driver on the silicon,
  the timer events, the cost of a scheduling round, the periods on a frequency counter,
  the regulator's response, the 4-slot buffer between the cores; watched through a
  trace rather than a halted core (`rp2040.md`).
- **Checks on the board on every change of `main`**, pulled by the bench rather than
  pushed to a self-hosted runner, on the images the CI builds; the bench is an Arduino
  UNO Q since 2026-09-26 (`tools/board_ci.md`).
- **The RP2350 port**: the generic layer taken to ARMv8-M, both cores running under
  Renode, `ACTLR.EXTEXCLALL` set, and memory barriers between the cores
  (`architecture.md`), whose compiled order the CI checks against the models
  (`tools/check_order.py`); the 3-slot buffer across the cores under Renode, with the
  RP2350's exclusive monitor played in place of Renode's (`emulation.md`); a FIFO queue
  between the cores, Figure 3 of Evéquoz's paper adapted to the RP2350's LL/SC
  (`Escapement_CoreQueue.c`, `test/model/fifo_mp.py`).
- **`-O2`**, once the barrier that makes a pended exception take effect went in
  (`method.md`): 3.2 µs a round instead of 7.0.
- **The queue sentinels made whole task control blocks.** Two defects came from reading
  a task field through the idle task, past a block allocated to the few fields it used;
  the head and the tail are now whole TCBs in `.bss`, for about sixty bytes of RAM per
  kernel. GCC's `-fanalyzer`, tried first, reported nothing on the code before or after:
  the blocks came from `OSMalloc` and were reached through casts.
- **A defined start for the STM32 timer.** `_OSStartTimer` clears the counter: started
  under Renode at 0x3FFFF000, the kernel produced no output at all, and now the 164
  pulses in 80 ms of a normal start.
- **A user guide**, `api.md`, written from the headers and checked against the code where
  they disagree: on Cortex-M the kernel does not mask the source of an interrupt, as the
  headers say of the original port.
- **Dropped**: proposing the two fixes to Renode's STM32 timer upstream (2026-09-24). The
  fixed copy lives in `emulation/renode` and the CI loads it, so nothing waits on it. The
  STM32 port stayed: until the RP2350 port, it was the only one to run `LDREX`, `STREX`
  and `CLREX`.

## The MSP430 port was removed

Escapement began life on the TI MSP430, and the port went on 2026-09-20 with its three
examples — 19,406 lines across 60 files, a fifth of what the repository carried. It had
not been built once since the takeover: the original IAR and Code Composer projects are
not in the repository, nor the configurator that produced its per-derivative headers,
and no test could reach it.

What was lost: the FRAM of the MSP430FRxx parts — non-volatile, byte-addressable, nearly
free to write — still has no equivalent for intermittent computing under energy
harvesting, and that was the one reason to keep the port. TI adds no new MSP430
families and steers new designs towards MSPM0, a Cortex-M0+ the generic layer already
covers. The history keeps it all, and so does the archived `beber007/zottaos`.
