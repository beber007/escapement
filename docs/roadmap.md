# State of the project and direction

Taking over and maintaining an existing real-time code base; see `NOTICE` for
the lineage and the third-party components.

## Direction

Development effort goes to **ARM Cortex-M**, and the power-aware variant is the
point of the project. The target for it is the **RP2040**: this documentation
argues, in `power-aware.md`, that it is where dynamic voltage and frequency
scaling has a real niche, because it sleeps poorly — and it is the one board
here that has been verified on hardware.

New work goes to the **Pico and the Pico 2** only. The RP2350 of the Pico 2 is
the sequel to the RP2040 and the better reason to write the ARMv8-M kernel port
it needs: its regulator switches rather than dissipates. It comes after the
RP2040 bench, and only if that bench shows DVFS beating race-to-sleep.

Of the **STM32**, only the F4 example stays, with no new example or port. The
Pico runs every test the STM32 ran, so the L1 examples went on 2026-09-22 with
their DVFS driver; the F4 is kept because it alone executes the Cortex-M3/M4
path of the context switch, from which an RP2350 port would start, because its
suites run on the Mac where those of the Pico need Linux, and because they rest
on the platforms of Renode itself rather than on third-party models. Once an
RP2350 port runs that path, the F4 can be reconsidered. The **STM32L4** and **STM32U5**,
once the next steps, are set aside: the L4 would need its own Renode platform,
and the U5 sleeps too well for DVFS to have much to gain (`power-aware.md`).

Today `EscapementHardPA` runs on the RP2040 alone, and in the host test.

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
- [x] **Power-aware variant on Cortex-M**: `stm32l-discovery-pa` scheduled its
      three tasks and reprogrammed the PLL, verified in CI — removed since with
      the L1, the Pico having taken over.
- [x] **Measure the per-activation cost again on the Pico.** Under EDF as under
      deadline-monotonic, 3.2 µs on average and 8 µs at worst; 4.4 µs for the
      soft kernel and 4.2 µs for the power-aware one (`rp2040.md`). The share of
      the processor first published, 0.43 % at 1,340 rounds a second, did not
      reproduce, even from the revision that published it: 0.33 % at 1,000.
- [ ] Propose the two fixes to the Renode `Timers.STM32_Timer` upstream.
- [x] **Write the DVFS driver for the RP2040.** `OSSetProcessorSpeed` moves
      between 12, 50 and 125 MHz with the system PLL kept locked, so no change
      waits for it, and sets the core voltage through `VREG`: 1.10 V at 125 MHz
      and 1.05 V below, the bottom of what the datasheet guarantees. `make
      UNDERVOLT=1` goes down to 0.95 and 0.90 V for the bench, outside the
      specification (`power-aware.md`). `TaskLEDPico` has a power-aware branch,
      and three more builds of the `emulation-rp2040` job run it: write hooks
      check that the clock never runs faster than the voltage allows, on a model
      of the regulator the RP2040 models lack.
- [x] **Run the power-aware kernel on the board.** Through a trace the firmware
      writes and the debugger reads without stopping a core (`make TRACE=1`,
      `tools/read_trace.py`), since stopping one made the tasks miss their
      deadlines. It found a timer interrupt left pending by the previous image,
      which stalled the kernel at start after a load from the debugger, now
      cleared by the port; then showed the DVFS driver at work on the silicon,
      down to 12 MHz for the probe and back to 125 MHz in the idle task
      (`rp2040.md`). The timer events followed on 2026-09-23, on all three
      kernels, the periods on the frequency counter the same day, and the time
      the regulator takes to report its output in regulation on 2026-09-24. Left
      to do on the board: undervolting with a checked computation, and the full
      settling of the regulator, which only an instrument on the core supply
      can show.
- [ ] **Test the other power-management policies.** The power-aware kernel offers
      four, chosen by `POWER_MANAGEMENT` in `EscapementHardPA.h`: OTE, the default,
      under either scheduling algorithm; DRA and DR_OTE, which force EDF*; DM_SLACK,
      which forces deadline-monotonic. Only OTE had ever run — on the host, under
      Renode and on the board, and on the board under EDF only. The three others
      were said to compile and nothing more; on 2026-09-24 they did not compile
      at all, on the target or on the host, and their host test found four
      defects (`method.md`). They now pass it, including `busy`, `early` and
      `slack`, where tasks take time and the speed the kernel picks decides
      whether they meet their deadlines; `make KERNEL=PA POWER=DRA` builds them
      for the Pico, and the CI runs all three under Renode: DR_OTE and DM_SLACK
      pass the whole RP2040 suite; DRA passes it too, but never changes speed in
      `TaskLEDPico`, having nothing to reclaim there, which the suite now expects.
      On the board, on 2026-09-24, all three keep every period of `TaskLEDPico`:
      DRA without changing speed, DR_OTE and DM_SLACK taking the speeds of OTE; a
      round costs 7.4 µs under DRA, 6.5 under DR_OTE, 4.2 under DM_SLACK as under
      OTE (`rp2040.md`). Left for the energy bench: whether any of them saves
      anything over OTE. Two things the host test does not catch, found by giving it faulty kernels: DM_SLACK's slack never
      running out, and DM_SLACK reclaiming nothing at all — at the three speeds
      of the RP2040 its slack is almost never enough to drop a step, and it
      picked the same speeds as OTE in every run.
- [ ] **Show the lock-free mechanisms between the two cores.** The scheduler stays
      on one core; what can be shown is the communication between them. On the
      Pico, only Simpson's four slots work across cores — the Cortex-M0+ has no
      exclusive accesses, and the emulated LL/SC holds on one core: core 1, bare,
      writes records whose bytes all carry the same rising counter, a task on
      core 0 reads them and counts torn reads and values going backwards, the two
      properties Rushby model-checked; a single unprotected buffer, run the same
      way, is the negative control. The port must first start core 1. On the Pico
      2, the three-slot buffer and the FIFO queue too, with `LDREX`/`STREX` made
      coherent between the cores by `ACTLR.EXTEXCLALL` (the SIO spinlocks are
      unreliable there, erratum RP2350-E2 of the
      [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf);
      see how [TinyGo](https://github.com/tinygo-org/tinygo/pull/5708) and the
      [pico-sdk](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_sync_spin_lock/include/hardware/sync/spin_lock.h)
      deal with both), and the FIFO in the multiprocessor
      form of Evéquoz's paper: the announced operation of the kernel's queue
      assumes a single core.
- [ ] Then build the current measurement bench described in `power-aware.md` — a
      plain Pico rather than a Pico W, an INA226 read from the Bus Pirate — and
      answer, on the target this documentation calls the most promising, whether
      DVFS beats race-to-sleep.
- [ ] If it does, port to the **RP2350** (Pico 2) rather than the STM32U5: the
      same ARMv8-M kernel port, on a target where DVFS stands a better chance —
      a switching core regulator, and a PLL and regulator scheme close to the
      RP2040's. Its sleep is better, which has to be weighed on the bench too;
      see the table in `power-aware.md`.
      No Renode model of the RP2350 exists (checked 2026-09-22): Renode ships
      none, and the `rp2350Blinking` branch of matgla/Renode_RP2040, begun in
      November 2024, stopped at a GPIO and a SIO — its author has since frozen
      the project, porting every peripheral being too costly. Renode does emulate
      the Cortex-M33 core. What the Escapement tests use is small: the core and
      its SRAM, the timer (the fixed copy of `Escapement_RP2040_Timer.cs`, moved
      to the addresses of the RP2350), a PL011 UART, which Renode models, the
      GPIO outputs of the SIO, and ready bits for the clocks, the PLL, the resets
      and the regulator, as the VREG model of `escapement_pico.repl` does. A
      platform of our own, a few hundred lines, would keep the port under
      emulation; without it, the port could be checked on the board alone.
      Nothing else covers the RP2350 either: QEMU has an RFC for the RP2040 only
      (v3, September 2026, not merged; it models the timer, the clocks, VREG,
      the SIO and the UART, and could replace the frozen models for the Pico
      once merged) and a mere feature request for the RP2350; Wokwi runs in the
      cloud, closed, with an RP2350 still incomplete. The lasting answer is a
      bench on the board, run by the CI from a machine of the house.
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
- [x] **Cross the 2^30 wrap in the host test.** `test_scheduler wrap` jumps the
      clock from one event to the next over three wraps, serves the arrival just
      short of each one late and leaves tasks in the ready queue across it.
      Deleting any one of the three time shifts of the arrival and ready queues
      makes it fail — one of them only reached through that latency, and one
      making the kernel loop forever, which a 10 s alarm now reports.
- [x] **Test what no example exercises.** The host tests covered 24 % of the
      lines of `EscapementHard.c`: nothing ran the FIFO queue or the slot
      buffers, and the event-driven tasks only in examples that are built, not
      run. `test_scheduler events` wakes event-driven tasks from periodic tasks,
      from themselves and from a buffer slot filling up; `test_ipc` takes the
      FIFO queue past the wrap of its indices and both slot buffers through
      their states. Coverage is now 90 %; what remains is mostly the paths a
      preempted operation takes, which a single-threaded host cannot reach.
      Fourteen of fifteen deliberate defects fail a check; the one that does not
      (event-driven deadlines no longer following one another) has no effect
      when tasks run in zero time.
- [x] **Run `TestTimerEventF4` under Renode.** `escapement_f4_events.robot`
      checks the high time and the period of both outputs within 2 %, which
      takes the timer-event handler on TIM14 and the event-driven tasks it
      wakes. TIM14 now uses the fixed timer model as TIM2 does: its driver also
      forces a compare event through CC1G, a path this example happens not to
      take.
- [x] **Run the RP2040 port under Renode in CI.** `escapement_pico.robot`
      times the 1 ms probe within 2 %, checks the three tasks and the UART
      echo, on Renode 1.16.1 and the models of matgla/Renode_RP2040, both
      pinned. Core 1 has to be halted, or the LED tester times edges from the
      wrong core (`emulation/renode/RP2040.md`).
- [x] **Validate every kernel on the Pico.** `TaskLEDPico` gained the soft branch
      it lacked — the first two tasks (1,3)-firm as in `TaskLEDF4`, the 60 ms task
      and the 1 ms probe hard — and the `Makefile` links `EscapementSoft.o`, which
      it never did: `make KERNEL=SOFT` did not build. The `emulation-rp2040` job is
      now a matrix of the four builds, each running `escapement_pico.robot`. Run
      first on a Linux machine of the house, under podman, since the models need
      the linux-dotnet package (`emulation/renode/RP2040.md`). The power-aware
      kernel followed with the DVFS driver above.
- [x] **Cross the 2^30 wrap on the Pico.** The port rebuilds the wrap of the
      kernel clock with `ALARM1`, eighteen minutes after the kernel starts at the
      1 µs tick: no test had reached it, on the board or under Renode. The
      `escapement_pico.robot` suite now runs `TaskWrapPico` on a copy of the
      timer model clocked at 1 GHz (`Escapement_RP2040_Timer.cs`, since the model
      of matgla/Renode_RP2040 fixes its frequency), periods scaled to match: after
      the boundary every task still runs and the probe keeps its 50 ms within 2 %.
- [x] **Timer events and event-driven tasks on the Pico**, which the F4 ran with
      `TestTimerEventF4` and the Pico not at all. `Escapement_TimerEvent.c` now
      has an RP2040 counterpart on alarm 2 or 3: event times are the lower 32
      bits of the counter, compared by signed difference, so nothing is ever
      shifted, and the queue is guarded by masking interrupts over at most its
      few nodes. `TestTimerEventPico` and its test run on every Pico build. Two
      things stood in the way: the vector table sent alarms 2 and 3 to the
      handler of undefined interrupts, and the port wrote `INTE` and `INTF`
      whole, which would have cleared the bits of another alarm; it now goes
      through the atomic set and clear aliases. The Pico suite runs on a fixed
      copy of the timer model, whose alarms interfered with one another.
- [x] **Make the queue sentinels whole task control blocks.** The head and the
      tail of the queues, the tail being the idle task, were allocated to the size
      of the few fields they use and then handled as tasks: two defects this year
      came from reading a task field through the idle task, past its block. They
      are now whole TCBs in `.bss`, zeroed at start-up, for about sixty bytes of
      RAM per kernel. The analyser of GCC (`-fanalyzer`) was tried on the kernels
      first: it reports nothing on them, and nothing either on the code as it was
      before those two defects were fixed, since the blocks come from `OSMalloc`
      and reach the fields through casts. It was left out of the CI; what caught
      one of the two is AddressSanitizer in the host test.
- [x] **Run the power-aware kernel on the host.** `test/host` now builds it under
      both algorithms, with AddressSanitizer as the other two, and runs the task
      set, the wrap, the event-driven tasks and the communication tests on it:
      90 % of the lines it compiles to, in its shipped configuration (one task
      extension), run. The host records the speeds it asks for: always one of the
      operating points of the RP2040, changing over four thousand times on the
      task set and the wrap, and never with event-driven tasks in the set — the
      one task extension only slows a task down when nothing can arrive before it
      ends, and a waiting event-driven task can be woken at any time. A kernel
      that never slows down fails. Two limits: tasks take no time here, so that
      slowing down keeps the deadlines is not something this test can see; and
      the defect of the idle task found on the Pico would not have shown either,
      since it needed the assembler context switch of the Cortex-M0.
- [x] **Settle `EscapementSoft` and deadline-monotonic scheduling: kept, and
      tested.** The host test now builds both kernels under both algorithms,
      adds an (m,k)-firm scenario under a declared overload of 220 % and
      checks that tasks released together run in priority order; the `variants`
      CI job runs the Renode suites on the soft kernel and under
      deadline-monotonic scheduling. It found the soft and the power-aware
      headers still forcing deadline-monotonic, as the hard one had, and the
      port testing the algorithm before its names were defined (`method.md`).
      One consequence: `stm32l-discovery-pa` now runs EDF, as its configuration
      always said. Of the deliberate defects in the soft kernel, one is not
      caught: ignoring the interference of other tasks in the schedulability
      test of optional instances, which only shows when tasks take time.
- [x] **Two things the soft kernel left to its caller.** Under EDF it computed
      the workload of an event-driven task as `(wcet << 8) / aperiodicUtilization`
      and ignored the one passed: `TestTimerEventF4` and `TestTimerEventL1` pass 0
      for both in their soft branch, which divides by zero — 0 on a Cortex-M, a
      crash on x86. A workload given is now taken as is, and a creation with
      neither is refused. And under deadline-monotonic scheduling it shifted at
      every 2^30 wrap the deadline of mandatory instances, which it never set,
      until the value overflowed; it is now set for every instance. The host test
      builds with `-fsanitize=signed-integer-overflow` and fails on the old code;
      both soft variants in CI now run `escapement_f4_events.robot` too, the soft
      branch of the example taking the periods of the hard one.
- [x] **Give the STM32 port a defined starting time.** The kernel assumed its
      counter started near zero; `_OSStartTimer` now clears it, which costs one
      store. Shown under Renode: started with the counter at 0x3FFFF000 the
      kernel produced no output at all, and now produces the same 164 pulses in
      80 ms as a normal start.
- [ ] Rebuild the user documentation (the original manual and reference notes
      were removed along with the rebranding).
