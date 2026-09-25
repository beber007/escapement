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

Of the **STM32**, the F4 example stays, with no new example or port: its suites run on
Renode's own platforms and on the Mac, and it was the first to run the Cortex-M3/M4
path of the context switch, which the RP2350 port now takes too. Once a Pico 2 board
runs that path, the F4 can be reconsidered. The **STM32L4** and **STM32U5** are set
aside: the L4 would need a Renode platform of its own, and the U5 sleeps too well for
DVFS to gain much (`power-aware.md`).

## Open work, in order

1. **The energy verdict on the RP2040.** Build the bench of `power-aware.md` — a plain
   Pico rather than a Pico W, powered and measured by a Power Profiler Kit II, chosen
   on 2026-09-24 over an INA226 — and answer whether DVFS beats race-to-sleep. The same
   bench settles what the documentation leaves open: whether the idle task should sleep
   at 12 MHz rather than 125 (`rp2040.md`), whether DRA, DR_OTE or DM_SLACK save
   anything over OTE, how long the regulator really takes to settle, and whether the
   core undervolted still computes right, checked by a computation whose result is
   verified.
2. **The Pico 2 on the board.** It needs a Pico 2 and an OpenOCD that knows the RP2350,
   which Homebrew's 0.12 does not. The board alone can say that the clocks are
   programmed right, which the Renode platform acknowledges blindly; then the six
   examples, `ThreeSlotCoresPico2` first, and litmus tests of the order in which each
   core sees the other's accesses.
3. **What is left to verify between the cores.** The queue between the cores
   (`Escapement_CoreQueue.c`) puts a DMB between any two of its accesses to different
   words, which lets its model take each core's accesses in program order; a model of
   weakly ordered cores, as the slot buffers have, would keep only those it needs. Its
   demo has one producer and one consumer per queue, where the model has two of each.
4. **DVFS on the RP2350**, if the verdict of item 1 is for it: its regulator and its
   power manager differ from the RP2040's, and the driver is to be written from the
   pico-sdk headers.
5. **Audit the inherited kernel** line by line, as the RP2040 port and the Cortex-M
   layer were (`method.md`).

## Done

- **The kernel runs, and is run in CI.** The Makefiles repaired, every example built on
  every push, and executed under Renode as a regression test: the STM32F4 on Renode's
  platform with two fixes to its timer model, the RP2040 on the models of
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
- **Checks on the board on every change of `main`**, pulled by a machine at home
  rather than pushed to a self-hosted runner (`tools/board_ci.md`).
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
