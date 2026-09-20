# State of the project and direction

Taking over and maintaining an existing real-time code base; see `NOTICE` for
the lineage and the third-party components.

## Direction

Development effort goes to **ARM Cortex-M**, and the power-aware variant is the
point of the project. The target for it is the **RP2040**: this documentation
argues, in `power-aware.md`, that it is where dynamic voltage and frequency
scaling has a real niche, because it sleeps poorly — and it is the one board
here that has been verified on hardware.

A **STM32L4** would come next among the STM32 parts, being a Cortex-M4 the
generic layer already covers. It is a larger job than it looks: Renode ships no
L4 platform, so the port would have to bring its own. The **STM32U5** is further
still, being a Cortex-M33 and therefore a kernel port to ARMv8-M rather than a
board port. Today `EscapementHardPA` is referenced by
both ports, and `stm32l-discovery-pa` demonstrates it on Cortex-M.

## The MSP430 port was removed

Escapement began life on the TI MSP430, and the port went on 2026-09-20 along
with its three examples — 19,406 lines across 60 files, a fifth of what the
repository carried.

It had not been built once since the takeover: the original projects were IAR
and Code Composer projects that are not in the repository, and the per-derivative
headers it needs were produced by a configurator tool that is not either. Keeping
it meant carrying a quarter of the source tree that no test could reach and no
reader could trust, in a project whose point is that everything it claims is
verified.

What was lost: the FRAM of the MSP430FRxx parts — non-volatile, byte-addressable,
nearly free to write in energy terms — still has no equivalent for *intermittent
computing* under energy harvesting, and that was the one reason to keep the port.
TI adds no new MSP430 families and steers new designs towards MSPM0, a Cortex-M0+
the generic layer here already covers.

The history keeps all of it, and so does the archived `beber007/zottaos`.

## Open work

- [x] Repair the `Makefile`s: the five STM32 examples build.
- [x] Audit the new code of the RP2040 port and of the Cortex-M layer — four
      defects fixed, see `method.md`.
- [x] Compilation verified in CI (`.github/workflows/build.yml`).
- [x] **Run the kernel.** The three tasks of `TaskLEDF4` are scheduled at their
      periods under Renode (see `emulation.md`).
- [x] Emulation replayed in CI with `renode-test`: running the kernel has
      become a regression test.
- [x] **Power-aware variant on Cortex-M**: `stm32l-discovery-pa` schedules its
      three tasks and reprograms the PLL, verified in CI.
- [ ] **Measure the per-activation cost again on the Pico.** The published
      figures — 3.2 µs mean, 8 µs worst case — were taken while the kernel still
      selected deadline-monotonic scheduling; ordering the ready queue by
      deadline is not the same work. `tools/measure_cost.sh` does the run, with
      the instrumentation switched on in `Escapement_Config.h`.
- [ ] Propose the two fixes to the Renode `Timers.STM32_Timer` upstream.
- [ ] **Write the DVFS driver for the RP2040.** `Escapement_Processor.h` declares
      12, 48 and 125 MHz, but `OSSetProcessorSpeed` does not exist: the PLL has to
      be reconfigured and the core voltage set through `VREG_CTRL`. Testable under
      emulation, and a prerequisite to measuring anything.
- [ ] Then build the current measurement bench described in `power-aware.md` — a
      plain Pico rather than a Pico W, an INA226 read from the Bus Pirate — and
      answer, on the target this documentation calls the most promising, whether
      DVFS beats race-to-sleep.
- [ ] Only if that answer calls for it: an STM32L-Discovery board would allow
      `IccMeasure.c`, written by the original authors and still here, to be
      replayed on the L1. Needs a board nobody has, and the RP2040 bench above
      needs none.
- [x] **Fix what `-O2` exposed**: pending an exception did not take effect
      before the next instruction, so an optimised `OSEndTask` returned instead
      of switching context and faulted with `INVPC`. Barriers added; the build
      is at `-O2`, the mean cost of a scheduling round is down from 7.0 to
      3.2 µs and the worst case from 26 to 8 µs.
- [x] **Run the scheduler on the host.** `test/host` builds the kernel as it
      ships with a simulated target layer and exercises it with ten periodic
      tasks over 200,000 ticks: every task is activated exactly as often as its
      period calls for.
- [x] Check in that host test that no deadline is ever missed. What the mirror
      of the task control block was reading turned out to be the real finding:
      the kernel was scheduling deadline-monotonic, not by deadline, so the
      field did not exist. Both are now selectable and the check passes.
- [x] **Give the STM32 port a defined starting time.** The kernel assumed its
      counter started near zero; `_OSStartTimer` now clears it, which costs one
      store. Shown under Renode: started with the counter at 0x3FFFF000 the
      kernel produced no output at all, and now produces the same 164 pulses in
      80 ms as a normal start.
- [ ] Rebuild the user documentation (the original manual and reference notes
      were removed along with the rebranding).
