# State of the project and direction

Escapement continues ZottaOS, a real-time kernel by Claude Evéquoz and Bertrand Hurst,
whose code last changed in 2014 and was published in 2016.
`NOTICE` gives the lineage and the third-party components.

This page separates what is open from what is done. Open items are plans, each with the
date its facts were read. Done items say where the result is measured or checked, and a
result counts as measured only where a page gives its date and conditions. Last revised
on 2026-09-27.

## Direction

Development goes to **ARM Cortex-M**, and the power-aware variant is the point of the
project. Its target is the **RP2040**. `power-aware.md` argues that dynamic voltage and
frequency scaling has a niche there because the chip sleeps poorly, and it is the board
at hand.

New work goes to the **Pico**, the **Pico 2** and the **STM32U5**. The RP2350 of the
Pico 2 has a switching core regulator and two cores with exclusive accesses. Its port was
begun on 2026-09-24 for the cores. Its DVFS driver waits for the RP2040 bench, and will be
written only if that bench shows DVFS beating race-to-sleep.

Among the STM32, the **STM32U5** is the one kept. Its port, begun on 2026-09-25 for the
STM32U585 of the Arduino UNO Q (`stm32u5.md`), was written anew in the manner of the
RP2350's. The older STM32 ports went on 2026-09-26 (9783ab4), the F4 last, once the U5
ran on its board with a check of each commit. The U5 sleeps too well for DVFS to gain
much (`power-aware.md`), so a power-aware kernel for it, if any, comes after the verdict
on the RP2040. The **STM32L4** is set aside.

## Open work, in order

0. **Two weeks of endurance on a board** (`rp2040.md`, "The endurance test"). `SoakPico`
   is to run on the Pico W from the week of 2026-09-28, a week under the hard kernel and a
   week under the power-aware one. The Pico and the Pico 2 ordered for the board CI free
   the Pico W for it. Instances under Renode run alongside on another machine, each with
   its own build and seed.

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

   Built with `SLEEP_SPEED=0`, the power-aware kernel's idle task sleeps at 12 MHz with
   the PLL still locked (`power-aware.md`); the 0.39 mA had it stopped, and the datasheet
   gives no lock time (§2.18). The steps on the bench, then:
   1. the idle task as it is, at 125 and at 12 MHz;
   2. SLEEP, with the timer, its tick and the interrupts in use left on. Whether the tick
      also needs CLK_SYS_WATCHDOG is for the board to say;
   3. the PLL's lock time, to decide whether a long sleep pays for stopping it, as the U5
      stops its clocks in Stop 2.

2. **The Pico 2 on the board.** It needs a Pico 2 and an OpenOCD that knows the RP2350,
   which neither Homebrew's 0.12 nor Debian's does. Raspberry Pi's fork was built on the
   UNO Q on 2026-09-26 (`tools/board_ci.md`) and waits for the board. Only the board can
   say that the clocks are programmed right; the Renode platform acknowledges every
   request blindly. Then come the nine examples, `ThreeSlotCoresPico2` first, and litmus
   tests of the order in which each core sees the other's accesses.

3. **What is left to verify between the cores.** The queue between the cores
   (`Escapement_CoreQueue.c`) had a DMB between any two of its accesses to different
   words, fifteen in all. A model of weakly ordered cores (`test/model/fifo_mp.py`,
   2026-09-26) kept six (a6f8b6e, 2026-09-27). Five of them were shown needed, and the six
   enough within the model's bounds, on a machine of 30 GB; the CI explores only a part.
   The sixth, before a dequeue returns, could not be shown superfluous. Since 2026-09-26
   each core of `FIFOCoresPico2` is both producer and consumer of the same queues, as in
   the model. No Pico 2 has run the queue yet, and Renode, whose cores keep program
   order, cannot show the barriers at work.

4. **DVFS on the RP2350**, if the verdict of item 1 is for it. Its regulator and its
   power manager differ from the RP2040's, and the driver is to be written from the
   pico-sdk headers.

5. **The STM32U5's energy.** The idle task sleeps in Stop 2 on the board since 2026-09-27
   (see Done). Its current is not measured yet. The datasheet, read on 2026-09-26
   (`power-aware.md`), puts it at some four times less current than Sleep at light load,
   some 5.3 against 1.4 mA with a tenth of the processor busy. The UNO Q's U585 has no
   SMPS, and DVFS would add some 10 % at most.

   Next, the PPK2, on `SleepU5` built with `make PHASES=30`, which alternates Stop 2 and
   Sleep, D13 telling them apart. On the UNO Q only the difference can show: the MCU
   shares a rail with the rest of the board, with no jumper to measure it alone
   (`stm32u5.md`, 2026-09-27). The absolute currents are for a NUCLEO-U575ZI-Q, borrowed
   for it. `Examples/nucleo-u575` builds `SleepU5` for that board, on the LDO or, with
   `make SMPS=1`, on its SMPS (bf447b8); neither the board nor the image has been tried.

   The plan for Stop 2, read from RM0456 rev. 7 and ES0499 on 2026-09-26, and what became
   of it. LPTIM1 cannot be the kernel's clock. It has 16 bits and wraps every 2 s on the
   LSE, in steps of 30.5 µs. Its counter must be read twice to be trusted, and a new
   compare waited for (CMPOK) after a latency RM0456 does not quantify for it (§58.4). So
   TIM2 stays the kernel's clock while the core runs. The idle task, finding the next
   event far enough off, arms LPTIM1 on the LSE to wake it early and enters Stop 2, from
   which LPTIM1 wakes the chip (table 599). Stop 2 turns the HSE off and leaves the chip
   in range 4. Waking takes the HSE's 2 ms, up to 47 µs for range 1, 50 µs for the
   booster and 25 to 50 µs for PLL1, so Stop 2 pays only for waits of some milliseconds.
   TIM2 is then moved on by the time LPTIM1 counted, and the idle task finishes the wait
   in Sleep. The errata kept in view: never clear LPTIM1's ENABLE, reset it through the
   RCC instead (2.17.1); writing DIER clears the flag it enables (2.17.3); a HardFault may
   follow a wake-up by LPTIM1, of the SRD domain, when debugging with DBG_STOP (2.2.19).
   Renode's STM32L0_LpTimer has the same first registers and was expected to serve.

   Two parts of that plan changed on 2026-09-27. It kept Stop 2 away while a timer event
   was pending, TIM5 stopping with it, a rule taken from the definition of Stop rather
   than read for TIM5 itself; TIM5 is now carried through Stop 2 instead (06e2e60). It
   also kept Stop 2 away while a UART had to receive, since LPUART1 runs on PCLK3, and
   `SoakU5`, fed by Linux, would hardly ever have entered it. LPUART1 now receives
   through Stop 2 on HSI16, at 57,600 baud, the rate its start allows (2673f02,
   `stm32u5.md`).

6. **A deeper sleep on the RP2350.** Its idle task sleeps by WFI with every clock
   running. The RP2350 datasheet was read on 2026-09-27:
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
   1. measure the three on a Pico 2 with the PPK2;
   2. if SLEEP saves enough, gate the clocks the kernel does not need while TIMER0 runs
      on (SLEEP_EN1, p. 550), which keeps the time exact;
   3. for DORMANT, either calibrate LPOSC against the crystal before each sleep and
      measure the error left, or give the always-on timer an external 32.768 kHz clock
      on GPIO 12, 14, 20 or 22 (§12.10.7), which is hardware for the bench.
7. **The schedule beside the trace.** The response times of `TaskLEDPico` are measured
   against the manual's analysis since 2026-09-27 (see Done). Left: a figure of the task
   set's theoretical schedule beside the trace of the board, and the 60 ms task traced
   once it reaches its longest instances, some four minutes after start.

## Done

- **Response times against their analysis, on the Pico (2026-09-27, 42db832).** The
  bound of ZottaOS's manual (eq. 2.3), the kernel's costs taken from the trace (7 to
  16 µs from an alarm to the task it releases), held for every response seen under DM
  and EDF: 58, 140 and 191 µs against bounds of 123, 220 and 557 µs
  (`rp2040.md`, "Response times against their analysis"; `tools/response_times.py`).

- **The STM32U5 port, under Renode (2026-09-25).** `Escapement/CORTEX-Mx/STM32U5` runs
  the hard and the soft kernel under both algorithms. It has eight examples with the
  endurance test, and `SleepWrapU5`, `SleepU5` scaled to cross the 2^30 wrap under
  Renode. Its Renode platform is our own (`escapement_u5.repl`), and the suite passes its
  10 tests under each of the four builds, in the CI (`stm32u5.md`).

- **The STM32U5 on the board (2026-09-26).** On the Arduino UNO Q, from SRAM. On the
  first day the clock set-up ran and `SoakU5` ran 22 s without error. The clock has come
  from the board's 16 MHz crystal since 2026-09-26, within some 25 ppm of NTP
  (`stm32u5.md`). A board check of each commit runs on the UNO Q's own Linux since
  2026-09-26 (`tools/unoq_check.sh`, status `board/u5`). Since 2026-09-27 it runs the
  idle task in Stop 2 for a minute (`SleepU5`), then the endurance test for two minutes
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
    through it (item 5).

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
  four defects, all fixed, the last two between cores (`method.md`).
- **The RP2040 port on the board**: all three kernels, the DVFS driver on the silicon,
  the timer events, the cost of a scheduling round, the periods on a frequency counter,
  the regulator's response, the 4-slot buffer between the cores. All of it was watched
  through a trace rather than a halted core (`rp2040.md`).
- **Checks on the board on every change of `main`**, on the images the CI builds. The
  bench pulls them rather than being pushed to a self-hosted runner. Since 2026-09-26 the
  bench is an Arduino UNO Q, which checks the Pico (status `board/pico`) and its own
  STM32U5 (status `board/u5`) (`tools/board_ci.md`).
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
- **Dropped (2026-09-24): proposing the two fixes to Renode's STM32 timer upstream.** The
  fixed copy lives in `emulation/renode`, where the CI loads it, now for the U5 platform,
  so nothing waits on it. The older STM32 port was kept at the time because, until the
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
