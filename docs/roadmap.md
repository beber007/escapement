# State of the project and direction

Taking over and maintaining an existing real-time code base; see `NOTICE` for
the lineage and the third-party components.

## Direction

Development effort goes to **ARM Cortex-M**, and primarily to the STM32L4 and
STM32U5: their core voltage scaling and their low-power modes are what gives
the power-aware variant its point. Today `EscapementHardPA` is referenced by
both ports, but the only working DVFS example (`PA/`) targets an MSP430F5419A,
and the core voltage driver `VCORE.c` is MSP430-specific.

The **MSP430 port is frozen**: kept, not developed. It is only 7,600 lines, it
carries the one existing power-aware demonstration, and the FRAM of the
MSP430FRxx parts — non-volatile, byte-addressable, nearly free to write in
energy terms — still has no equivalent for *intermittent computing* under
energy harvesting. Its fate will be settled once a power-aware example runs on
Cortex-M. Note that TI no longer adds MSP430 families and steers new designs
towards MSPM0 (Cortex-M0+).

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
- [ ] Propose the two fixes to the Renode `Timers.STM32_Timer` upstream.
- [ ] **Replay `IccMeasure.c` on an STM32L-Discovery board** and record the
      real gain of the three voltage ranges. That is the only way to know
      whether DVFS beats *race-to-sleep* on this family.
- [ ] Depending on the result, port to a target where the gain is structurally
      larger — see “Which MCU to port to next?” in `power-aware.md`.
- [x] **Fix what `-O2` exposed**: pending an exception did not take effect
      before the next instruction, so an optimised `OSEndTask` returned instead
      of switching context and faulted with `INVPC`. Barriers added; the build
      is at `-O2`, the mean cost of a scheduling round is down from 7.0 to
      3.2 µs and the worst case from 26 to 8 µs.
- [ ] Unit-test the scheduler on the host, so that it can be exercised well
      beyond four tasks, and so that the 2^30 wrap of its clock can be reached
      in a test rather than after eighteen minutes of running.
- [ ] **Exercise the 2^30 wrap under emulation.** Placing the counter near the
      boundary before the kernel starts does not test the wrap: it tests
      starting at an arbitrary time, which the kernel does not support — the
      first arrivals are computed from a large current time, the next ones land
      back near zero, every task looks overdue at once and the overload guard
      fires. Measured under Renode: 164 output pulses in 80 ms from a normal
      start, none at all when the counter starts at 0x3FFFF000. Reaching the
      boundary honestly needs a dedicated example whose timer runs far faster
      with task periods scaled to match, so that 2^30 ticks pass in about a
      second of emulated time.
- [ ] **Give the STM32 port a time origin**, as the RP2040 port has. The kernel
      assumes its counter starts near zero. That holds after a reset, so no
      shipped example is affected, but it is an unstated requirement of the
      whole time base — and it is what the experiment above ran into.
- [ ] Rebuild the user documentation (the original manual and reference notes
      were removed along with the rebranding).
- [ ] MSP430, only if the port is revived: rebuild the configuration generator
      that produced the per-derivative headers (`Escapement_msp430xNNN.h`),
      absent from the repository, and a build system — there is none for this
      target.
