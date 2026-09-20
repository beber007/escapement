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
- [x] Move the build to `-O2` and redo the hardware measurements: the mean cost
      of a scheduling round falls from 7.0 to 3.2 µs, the worst case from 26 to
      8 µs.
- [ ] Unit-test the scheduler on the host, so that it can be exercised well
      beyond four tasks.
- [ ] Rebuild the user documentation (the original manual and reference notes
      were removed along with the rebranding).
- [ ] MSP430, only if the port is revived: rebuild the configuration generator
      that produced the per-derivative headers (`Escapement_msp430xNNN.h`),
      absent from the repository, and a build system — there is none for this
      target.
