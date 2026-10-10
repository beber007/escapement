# Verifying AI-assisted development

This project is written with an AI assistant, so the repository has to show that what
was produced is correct. This page describes the levels of verification, then records
the hypotheses that turned out wrong and the defects found along the way, with their
dates.

The working rule fits in one line: **the AI proposes, the instrument decides.** Nothing
here counts as established because it was asserted confidently, by the assistant or by
anyone else. It is established when something outside the claim confirms it.

## Six independent levels

| Level | Means | Catches |
|---|---|---|
| Compilation | GitHub Actions, each push to `main` and each pull request: GCC 16.2, GCC 14.2 (since 2026-09-26) and clang | code that does not build, and anything one compiler forgives that the other does not |
| The scheduler alone | the kernel built for the host, with time as a variable, in CI | a kernel that does not run the algorithm it claims, the 2^30 wrap of its clock, which the board reaches only after eighteen minutes, and the parts of the kernel no example exercises: event-driven tasks, FIFO queue, slot buffers |
| Every interleaving | small models of the slot buffers and of the FIFO queue, explored exhaustively in CI (`test/model`) | what no test that runs one path at a time can reach: an interrupt at the one instruction where it matters |
| Replayable execution | Renode, replayed by `renode-test` in CI | a kernel that builds but does not schedule |
| Internal state on hardware | OpenOCD and SWD | an emulator that models the hardware wrongly |
| Independent instrument | frequency counter of a Bus Pirate v4 | everything above at once — it trusts no software from this repository |

The levels are ordered by how much they cost and by how little they assume. The sixth
exists because the fifth still runs through a debugger, and that turned out to matter
(the `TIMER_DBGPAUSE` investigation in `rp2040.md`). Beside these levels, the CI runs:

- static analysis: cppcheck over each port, GCC's `-fanalyzer`, clang
  (`tools/clang_check.sh`);
- the compiled order of the accesses the lock-free code depends on (below);
- since 2026-10-04, coverage: a commit fails that leaves a line of the kernels unrun by
  the host test without saying why (`tools/coverage.py`);
- since 2026-10-04, a differential test: the traces of random task sets, run by the
  kernels on the host, checked against the algorithm each build claims
  (`tools/differential.py`). Who runs must be a valid choice of EDF or deadline-monotonic
  scheduling at every instant, the processor never idles while work waits, every
  instance ends when its work is done and by its deadline, and under EDF within the
  bound of Spuri's response time analysis (`tools/response_times.py`). Kernels made
  wrong on purpose — either order reversed, arrivals sorted the wrong way, a speed one
  step too low or always the slowest — fail it within the first few task sets of each
  build.

## Hypotheses that were wrong

The wrong hypotheses are kept here, each with what tested it. The assistant can be
wrong with confidence, and the record has to show how each conclusion was reached.

| Stated | What refuted it | What was actually true |
|---|---|---|
| The Renode timer model is conformant — then, once suspected, that it was the culprit, with no evidence either way | A reproducer of two register writes, outside of any kernel | Two real defects in the model: the `UG` callback ignored the value written, and `CC1G`–`CC4G` were unimplemented |
| `TIMER_DBGPAUSE` is not the cause of the kernel starting late | The binary under test was stale: no `Makefile` tracked header dependencies | It was the cause. The build system was fixed as a result |
| The narrow pulses of the 20 and 60 ms tasks defeat the frequency counter | Eight consecutive readings, stable to one part in a hundred thousand | The instrument was fine; the earlier failure came from the firmware |
| The per-activation cost of the kernel is what trips the overload guard | Measurement: 7.0 µs per round, 0.7 % of the processor | The core was still running on the crystal; the guard was right |
| Rewriting history to drop deleted blobs would shrink `.git` from 12 MB to 7 MB | Measuring the repository after the attempt | No gain at all, that day; the operation was reverted. Measured again later, on a copy, the same idea gave 12 MB down to 7 — so the first measurement was of something else, and what it was has not been reconstructed |
| Including the dependency files at the top of a `Makefile` is harmless | `make` stopped building anything but one object file | The first rule read becomes the default goal, and a `.d` file provides one |
| Building at `-O2` was a matter of adding the flag: it built, both emulation suites passed, and the board measured 3.2 µs per round instead of 7.0 | The CI, on its own toolchain | The kernel HardFaults there. Local success proved only that one compiler version was forgiving |
| The soft and the power-aware kernels honour the algorithm an application selects, now that the hard one does | Running the soft kernel on the host, where the deadline of every mandatory instance read 0 | Both headers set deadline-monotonic themselves, as `EscapementHard.h` had. The soft kernel had never run under EDF since the takeover, and the power-aware example, configured for EDF, ran deadline-monotonic |
| Building the soft kernel under EDF only needs the header fixed | The `_Static_assert` on the task control block, which failed | The port tested the algorithm before any kernel header had defined its names, and an undefined name is 0 to the preprocessor: `EDF == DEADLINE_MONOTONIC_SCHEDULING` read `0 == 0`, and the context switch would have read the task's entry point four bytes off. The names now live in `Escapement_Modes.h`, included first |
| The power-aware kernel stalls on the board, and so, once looked at closely, does the hard one | A trace written by the firmware itself and read without stopping a core | The hard kernel stalled only while the debugger stopped core 0 to read where it was: time ran on, the probe missed its deadline, the overload guard fired. The power-aware kernel did stall on its own, from a timer interrupt the previous image had left pending — a defect of the port, which neither Renode nor a power-up start could show |
| The host test of the soft kernel is deterministic: time is a variable, and nothing else varies | The same code, green on `main`, failing the next CI run with a signed overflow | The kernel read the period of the idle task, which it does not have, from whatever the heap held past its block. AddressSanitizer now reports it on every run |
| The four Pico builds passing their suite meant the Cortex-M0 context switch was sound | The power-aware kernel on the Pico, which read and wrote past the end of the RAM from its timer handler — a warning in the Renode log, not a failed test | The context switch reduced the state of every starting task to its running bit with an `ANDS`, where the Cortex-M3 variant tests into another register. The idle task lost `TASKTYPE_BLOCKING`, and the power-aware kernel took it for a periodic task with work left, 44 bytes into a 20-byte block. The hard and soft kernels only consult that flag on the idle task to sort arrivals ahead of it, and then read a field it does not have, which happened to do no harm in the examples; no Pico example has an event-driven task, the other kind it would have hurt. The suite now fails on any access past the RAM |
| The Cortex-M0 context switch saves what a task needs: R4-R7, as the ZottaOS port for it said, R8-R11 being out of reach of Thumb-1 code | `IPCPico` under Renode, failing under the soft kernel and deadline-monotonic scheduling once the two fixes the endurance test called for were both in, and passing as soon as anything observed it. A watchpoint set only past 35.98 ms caught the Filler writing a slot's number into a node of the FIFO queue (2026-09-25) | R8-R11 are preserved across calls as R4-R7 are, and GCC uses them in Thumb-1 code. A task that preempts another and ends is wiped from the stack with whatever it left in them: the Filler resumed inside `OSWriteBuffer` with the R8 of the task that had preempted it as the index of its latest slot, and its next slot pointed nine slots past the three. The fixes had only led GCC to keep a value in R8 across a point where a task can be preempted. The handler now saves R8-R11 through R4-R7; `IPCPico` checks them around a delay, with the tasks that preempt it leaving them changed, and fails on the old handler under the hard, the soft and the power-aware kernels |
| `SoakPico2` stopping under Renode on the kernel's overload check, only with core 1 running, was Renode's exclusives, which the tests between the cores already replace by the RP2350's monitor — stated by the assistant, and the test changed to play that monitor | The soft kernel under deadline-monotonic scheduling stopping in 4 runs of 4 all the same, then watchpoints on the state of the task that missed its deadline, on `_OSNoSaveContext` and on `_OSActiveTask` (2026-09-25) | `OSEndTask` marks the task a zombie, then says its context is not to be saved; nothing kept GCC from storing the flag first, and it did for the Cortex-M33 of the Pico 2 and the M4 of the F4, under the hard and the soft kernels; for the M0+ it happened to keep the order. An interrupt in between found a running task that was no zombie, left it in the ready queue, and wiped its stack in the switch it made; the task, still running for the kernel, was later resumed with the context of the task it had preempted, the idle task here, and stopped the kernel at its next arrival. Core 1 only moved the interrupts onto that instruction. A compiler barrier now separates the two stores in all three kernels, the monitor is out of the test, and it passes without it: 6 runs of 6 alone under the hard kernel, and the suite twice under each of the four builds of the Pico 2 |
| The 3-slot buffer is Chen and Burns' mechanism, with an LL/SC pair where they use compare-and-swap, and so as sound as theirs | An exhaustive model of the reader and the writer (`test/model/threeslot.py`), with the LL/SC pair emulated as on the Cortex-M0+ | A compare-and-swap fails only when the value changed; a store-conditional fails also when an interrupt merely came between it and its load-linked. The reader then left `Reading` at 3 and read slot 3 of three. The reader now tries again, and the host test makes store-conditionals fail on purpose to check it |
| The 3-slot buffer, its reader retrying, is sound between two cores as it is on one | The model taken to two cores, each with its own reservation, which a store of the other core to `Reading` clears (2026-09-24) | The writer tried its store-conditional once. On one core that was harmless: the interrupt that makes it fail clears the reader's reservation too. Between two cores the reader's survives, its store-conditional hands it a `Latest` read before the writer published, and the writer, choosing the slot neither `Reading` nor the new `Latest`, writes the one being read. The writer now tries again, in all three kernels; the model also fails with monitors local to each core, which is why the RP2350 needs `ACTLR.EXTEXCLALL` |
| The slot buffers need nothing more between two cores than on one: the 4-slot buffer carried 320,000 reads between the cores of the Pico with none torn, and the models of both held on two cores | The same models with each core free to perform its loads and stores out of program order, as Armv6-M and Armv8-M allow for Normal memory (2026-09-25) | Nothing in the kernels ordered a slot's bytes before its announcement, nor the reader's announcement before its copy. Each buffer needs four barriers, and the model catches the removal of any one: a read then mixes two records. The kernels now have them, a `DMB` on the RP2040 and the RP2350. That the board showed no torn read without them proves only that its cores did not reorder those accesses in that test; the architecture does not promise it |
| The FIFO queue of Evéquoz's Figure 3 can be taken between the two cores of the RP2350 as it stands: it is lock-free, and built on LL/SC, which the chip has | An exhaustive model of two cores running it (`test/model/fifo_mp.py`), with the monitor of the RP2350 and SCs that may fail for no reason (2026-09-25) | Figure 3 assumes the LL/SC of the paper's Figure 2, where an SC fails only when another thread's succeeded on the same word: an enqueuer whose one SC on Tail fails may take it that Tail moved on. The paper warns that real LL/SC give less and offers another algorithm for them (its Section 5 and Figure 5). On the RP2350 an SC also fails when the other core wrote anywhere in the granule, or for no visible reason: Tail then stays behind an enqueue that returned, and a dequeue that follows answers that the queue is empty while it holds the item. Head likewise. The model shows Figure 3 holding under the paper's LL/SC and failing under the chip's; `Escapement_CoreQueue.c` keeps Figure 3 and tries that SC again while the LL still finds the old index |
| A DMB between any two accesses of the queue between the cores to different words, fifteen, is more than it needs, and a model of weakly ordered cores would say which to keep | `explore_weak` in `test/model/fifo_mp.py`, each core performing its accesses in any order Armv8-M allows, at first with three of them pending at most, then, on a machine of 30 GB, with as many as the longest run of accesses between two barriers (2026-09-26) | Five are needed, each caught when left out: without the one after D5, a dequeue answers that the queue is empty while it holds an item; without any, a dequeuer reads an item's contents before its enqueuer wrote them. The narrow window could say nothing of the ten others: it held without the barrier at the entry of Enqueue, whose store the architecture lets the publishing SC overtake but for another DMB. The window as wide as the runs between barriers, which no access crosses, showed nine superfluous one at a time, and the five with the one before a dequeue returns enough, each scenario in 25 minutes and 16 GB at most; without that last one a case outgrew 26 GB. The queue keeps six, within the model's bounds: two places, two operations a core, two rounds of each loop. On 2026-09-29, each state kept as a fingerprint of 16 bytes, the five held without the sixth too, the longest window 9 |
| The queue between the cores may take an SC's write as seen before the writes after it: the model had always assumed it | The model letting a store pass an SC ahead of it, each core's reservation following program order and ended only by a write to its Location, after the architecture read (DDI0553B.y, B7.2.3: a control dependency starts from a read; atomic-ordered-before orders an SC after its own LL alone), 2026-09-29, prompted by an audit of the port | Wrong: with the six, Tail was seen advanced before the item it counts was in its place, a dequeuer found the place empty and helped Head past it, and the item was lost, in all three scenarios; the RP2350's monitor, covering the granule, still let one run through. A DMB after E15 and one after D15, each needed, close it; the eight hold at their full windows, 5, 6 and 4, and without the one before a dequeue returns, the seven left hold too, at 5, 6 and 7: the queue keeps seven, each needed. The chip likely never shows it, its SC waiting for the bus's answer, and neither the board nor Renode could have: the architecture allows it, which is what the model is for. The 3-slot buffer's model, which made the same assumption, holds without it: after its SC each core reads Reading again, which waits for the SC, and its next stores take their slot from that read |
| The exclusive monitor of the RP2350, played under Renode by hooks on the LL/SC functions, is played right: each hook runs whole | `FIFOCoresPico2` with each core a producer and a consumer of the same queues, at a slice of 1 us (2026-09-26) | 1,004 records taken of 1,000 written, the sums of their counters off. The queue's model holds with two of each; with the played monitor under a lock, every record came through once. The hooks of the two cores can run at once, and two SCs on one granule had both found their reservation |
| The FIFO queue of the kernels is wait-free on one core, its announced operation completed by whoever preempts it | An exhaustive model of the queue (`test/model/fifo.py`), run by run checked for linearizability | A dequeue that signals an event and finds the queue empty leaves the signal, then gets preempted before marking itself done. The first helper to see the signal left without marking it either; once a waiting task had taken the signal, a second helper, preempted since before, found the slot empty and put a signal back. One event, two tasks woken. The dequeue now marks itself done when it finds its signal |
| A full FIFO queue refusing a node would corrupt its tail, having no capacity test like the one in Evéquoz's paper — stated by the assistant from reading the code | The host test, strengthened to check the queue after the refusal | The enqueue only ever writes into an empty slot, a guard the reading had missed; the queue refuses cleanly |
| DRA, DR_OTE and DM_SLACK compile, and only running them is left — the roadmap, from reading the build | Building the power-aware kernel with each of them, on the target and on the host (2026-09-24) | None compiled. DRA and DR_OTE called an interrupt intrinsic of the MSP430 and gave the task control block a third link where the context switch reads its fields; DM_SLACK read a clock variable that no longer exists. Once built, the host test found four defects: the speed computation multiplied a time by a ratio and overflowed past 21 s, which OTE shares; DRA shifted a time that nothing updates without event-driven tasks and overflowed at the third wraparound; DRA looked for the running task in its simulation queue and read through a null link when the task had outlived its WCET there; and DM_SLACK gave the time a task left unused to tasks of higher priority, which never counted it in their response time — a hand-built task set shows the first task missing its deadline, and the time now goes to tasks of lower priority, as the kernel's own comment said |
| `SleepU5`, loaded on the NUCLEO-U575ZI-Q without an error from OpenOCD, was running there and failing on its own | Halting the core after the load: it was in its HardFault handler before the image started (2026-10-02) | JP2, which carries the ST-LINK's reset to the MCU, was off. The "reset halt" reset nothing, the image was started inside the handler, and the loader reported success |
| `SleepU5` hanging in Stop 2 on the NUCLEO came from the board: its revision X silicon, or its supply, VDD at 1.8 V on the LDO — stated by the assistant after the first trials | The same hang on the SMPS and at 3.3 V, then a second agent, started afresh, comparing the two boards' loaders down to their OpenOCD scripts (2026-10-02) | The loader. Debian's OpenOCD sets DBG_STOP and DBG_STANDBY at each connection, the UNO Q's own clears them; the port cleared DBG_STOP alone, and the chip entered Stop 2 with DBG_STANDBY set, which holds off the reset. Clearing both, it ran |
| The NUCLEO-U575ZI-Q has no HSE, so its endurance runs since 2026-09-30 checked PLL1 on the MSIS of range 2 — from the board's user manual (UM2861, 6.7), as the assistant read it | Its RCC, read over SWD while its run was redeployed, prompted by a wake-up of some 60 ticks of LPTIM1, near the 64 the port waits for an HSE (2026-10-02) | HSE ready and PLL1 on it, M = 4: its crystal X3 is fitted. The manual leaves the HSE to the variant. The runs checked the HSE path, and the MSIS of range 2 has run under Renode only |
| On PLL1 from the MSIS, 160.017 MHz, `SleepNoHSEU5`'s TIM2 would run 106.7 ppm ahead of LPTIM1 — computed by the assistant, and made the board check's bound | The check itself, red on its first run (2026-10-02): -4.1 ppm | TIM2 is set from LPTIM1 at every wake-up from Stop 2, where the image spends most of its time; PLL1's rate shows only while it is awake. The computation was right about the clock and wrong about what TIM2 counts. The path itself ran without a fault |
| The PPK2 read nothing on the NUCLEO, or the debugger could not reach the MCU through it, from a voltage drop in its shunt, then from VOUT on the board's rail, then from a fault of its range switching that its firmware once had — three causes stated in turn by the assistant and an agent (2026-10-03) | The firmware found up to date, the rail found at 3.31 V with the PPK2 at 3.5 V, then photos of the wiring | VIN and VOUT were the wrong way round on JP5: the MCU ran through the body diode of the PPK2's switch and the PPK2 read 0. Turned round, as an ampere meter, it measured. The debugger still does not reach the MCU through it, which is left unexplained: the image is put in the flash with JP5 fitted |
| The host test covered 97 % of the kernels' lines, as clang's coverage merged over the nine builds said (2026-10-04) | Reading each test binary on its own, then writing a test for each branch never taken | The merged view had left out lines compiled in one build only, two of DM_SLACK never run; 68 lines in all had never run. A test written for one of them found a defect inherited from ZottaOS: the soft kernel set an event-driven task's next release again each time it elected it, and under deadline-monotonic scheduling an instance ended 419 ticks after its signal, its deadline 300 and its response by analysis 210 |
| CBMC could prove the kernels' FIFO queue as compiled, `goto-instrument --isr` placing an interrupt before each shared access, nested as on one core (2026-10-04) | A first run successful in half a second, while an assertion that the interrupt never preempted the operation held too; then the same with the interrupt in place | The interrupt routine had been dropped as unused before it was instrumented, and nothing ran inside the operation. In place, one operation and one interrupt did not conclude in 20 minutes and 3.6 GB. The proof stays with `test/model/fifo.py`, a model written anew, and the compiled order with `tools/check_order.py` |
| The kernel schedules by earliest deadline first — the first line of the README, and what sets the project apart | Running the scheduler on the host, where a mirror of the task control block read a pointer where a deadline belonged | Every example shipped was scheduling deadline-monotonic. `EscapementHard.h` set the algorithm itself, with no `#ifndef`, and no configuration file overrode it |
| The RP2040's region of the MPU over the 1 KB below the stack would lock the core up on an overflow, the HardFault's own stacking faulting in the region — written by the assistant on 2026-10-04, and in the code's comment | `StackGuardPico` on the Pico (2026-10-05): DHCSR's S_LOCKUP at 0, the core in its HardFault handler | HFNMIENA at 0 turns the MPU off at the HardFault's priority: its stacking lands in the region, and the handler runs, the globals untouched |
| `Sentinel` written over under Renode in the deadline-monotonic build came from Renode, whose stacking of the fault walked down past the region — stated by the assistant, since the EDF build kept it and Renode takes a MemManage that ARMv6-M does not have | The disassembly of `Recurse` | GCC had put two levels of it in one frame of 1,536 bytes, which stepped over the 1 KB region: the test now keeps it out of line, and a frame of 1 KB or more is a limit of the guard, written in its comment |
| Under EDF the soft kernel's test of an optional instance need count only the mandatory instances due by its deadline, the others waiting for it; most of the instances it drops would then run — proposed by the assistant on 2026-10-07, and accepted | The kernel changed so, under `tools/differential.py`: a task set whose instances all take their WCET hung on the overload guard, a mandatory instance ending at 2,412, its deadline 2,384 | The instances that wait for it must still keep their own deadlines, which counting every one released before its deadline protected. `worst_case_end` simulated only to the optional instance's end, and counted as safe drops that made others miss; it now runs until the processor would first be free, and fails the kernel so changed at 1,703, before the hang. The change was not kept |
| The test of an optional instance by processor demand is exact, and keeps every deadline every instance taking its WCET — stated by the assistant on 2026-10-07, and ported into the kernel with the user's agreement | `BenchAdmissionPico` on the Pico the same night: the kernel's overload guard stopped the board after 42 tests, then after 634 with interrupts unmasked, where the count it replaced ran 20,000 | Exact for a kernel that costs nothing: the test itself took up to 420 µs at 125 MHz, from the instant it assumed the instance would start, and nothing counted it. The host, the simulation and the reference all had the kernel's code take no time |

One more mistake was of a different kind. An early rebranding pass deleted a comment
terminator in 32 files, and the first attempt to repair them corrupted 57 healthy ones.
It was undone with `git checkout` and redone from an exact list. Working in a
repository where every step is committed made that cheap.

## What `-O2` exposed

Adding `-O2` to the Makefiles built cleanly and passed both Renode suites locally three
times over. The board reported the per-activation cost falling from 7.0 to 3.2 µs. The
CI then failed the first assertion of the stm32f4 suite. Widening the tester window
changed nothing, so the window was not the cause.

The ELF the CI had built, run under the *local* emulator, reproduced the failure at
once: the binary was at fault, not the runner. Its outputs showed the kernel raising one
output at 19 µs and then stopping dead. The program counter sat in
`HardFaultException`, and `CFSR` read `0x00040000`, which is `INVPC`, an invalid
exception return. The context switch builds its own exception frame by hand. The
architecture tolerates what it does only as long as the compiler leaves the surrounding
code alone.

The defect itself was a missing barrier. Setting PendSV pending does not take the
exception at once. The write has to reach the NVIC, and the processor has to observe
the pending state before it runs what follows. `OSEndTask` depends on never returning.
A task starts with `0xFFFFFFF9` in `LR`, an `EXC_RETURN` value, so returning from
thread mode faults. At `-O0` the epilogue was long enough for the exception to arrive
first. Optimised, `bx lr` sits one instruction after the store. A `dsb` and an `isb`
close the window, the CI agrees, and the build is at `-O2`.

A local build passing is a statement about one version of one compiler. Here the
toolchain in the CI, not the one on the developer's machine, was the only thing between
this defect and a repository that claimed to be measured and verified. The first
diagnosis was wrong, too. Because the registers were written through non-volatile
pointers, it concluded that the compiler had dropped the store. The disassembly of the
CI binary showed the store there all along. The `volatile` was added anyway, since the
code had no right to count on that store being kept, but it was not the bug.

## The repository was not scheduling the way it said

The README put earliest-deadline-first scheduling first, and EDF is what sets this
kernel apart from a fixed-priority one. `EscapementHard.h` nevertheless selected
deadline-monotonic scheduling itself, in a plain `#define` with no `#ifndef` around it.
No example overrode it, and none could have.

The examples scheduled correctly, the emulation tests passed, and the periods were right
to the part per hundred thousand on a frequency counter. All of that is just as true
under deadline-monotonic scheduling. The problem surfaced once the scheduler ran on the
host. A build there read the deadline field of the elected task and got a pointer back.
Under deadline-monotonic scheduling the kernel inserts a priority byte and drops the two
deadline fields, which moves everything after them.

The algorithm is now chosen in `Escapement_Config.h`, the header only supplies the
fallback, and every example selects EDF. All three emulation suites still pass. The host
test also checks that no deadline is missed, a check that could not be written while
the field it reads did not exist.

## Mutants: how strong the tests are

Coverage says a line runs; it does not say a test would notice the line being wrong.
`tools/mutants.py` makes one small fault at a time in a kernel's source, a comparison
turned round, `+` for `-`, `&&` for `||`, 0 for 1, a statement of one line left out, and
runs the host test and `tools/differential.py` on the builds of that kernel. A mutant is
killed when a test fails, survives when none does. Lines no build of the host compiles,
code of a variant the host does not build, are left out: a mutant there changes nothing
the host runs.

The first run, on 2026-10-04, killed 355 of the hard kernel's 484 mutants that compiled,
73.3 %; 487 of the soft one's 718, 67.8 %; and 536 of the power-aware one's 798, 67.2 %.
Read one by one, the survivors of
the hard kernel fell in five kinds:

| Kind | Hard | What they were |
|---|---:|---|
| Equivalent: nothing a run can see changes | ~20 | ties broken the other way, a field set to the 0 it already held, a bound moved where no value reaches it |
| The target's only | ~30 | interrupts masked, compiler and memory barriers, the timer started, PendSV pended: the host's port does nothing there, and `check_order.py`, Renode and the boards are what read them |
| Hidden by the host | ~30 | every field a creation sets: the host's `OSMalloc` handed out zeroed blocks, where the target's SRAM holds what the last image left, so a field left unset read 0 and passed |
| The helpers of the wait-free queue | 7 | an operation finding its work done, which two nested interrupts reach: `test/model/fifo.py` explores them |
| Behaviour no test read | ~30 | below |

The last kind was the work, and it found three things beyond tests to write:

- the host test's blocks are now filled with 0xA5, as SRAM is not zeros: no field the
  kernels set turned out forgotten, but leaving one out now fails;
- `RunAcross`, the loop that jumps the host's clock from event to event, ran a task of
  a period past 2^30 a whole turn late, which no test had; fixed, and `longperiod` tests
  it (`test/host/README.md`);
- the variant of `EnqueueRescheduleQueue` for a port without `NonMaskableSoftwareTimer`
  was compiled by no build: the three ports and the host defined it. It was ZottaOS's,
  and went on 2026-10-05, the 97 images of the examples the same byte for byte.

The tests added for the rest: the edges of what a creation accepts (`createbounds`), a
period past 2^30 run over three wraps (`longperiod`), two events signalled at once
(`twosignals`), an event-driven task started at its signal and spaced by its workload
from the arrival queue (`eventspacing`), the edges of the slot buffers, and the harness
checking that a task ending leaves its context unsaved and asks for a switch, as the
timer handler must, which the host's port only counts.

`tools/differential.py` now also draws event-driven tasks, a workload apart under DM and
a total bandwidth server under EDF, and (m,k)-firm sets that overload unless optional
instances are dropped. Each task takes its WCET, so an optional instance wrongly admitted
misses its deadline.

Run again on the survivors, the hard kernel's score rose to 79.5 % (385 of 484), the soft
one's to 74.2 % (533 of 718), and the power-aware one's to 71.8 % (573 of 798). Run whole
on 2026-10-06 at the 300 task sets a build the CI runs, rather than 100: the soft kernel
74.2 % again (533 of 718), the hard one 79.5 % (385 of 484). The hard kernel's score was
first written 80.4 % (389 of 484), and the run of 2026-10-06 seemed to lose four mutants.
Its survivors run again at f7622b7, the source of that day, gave 30 killed, not 34, and
left the same 99 survivors as the run of 2026-10-06: the 389 was miscounted, and nothing
was lost. Under the hard kernel's builds, 300 task sets cannot kill fewer mutants than
100, the first 100 being the same sets drawn from the same seed. The power-aware kernel, run whole the same
evening at 300 task sets: 76.9 % (610 of 793), from 71.8 %. Of the mutants both runs
hold, 85 survived on 2026-10-04 and are killed now, by the tests and the speed reference
added since and by the larger runs together, the runs not telling which: 12 of them in
`DRASimUpdateElapseTime`, the excess of the event-driven tasks that the reference took
in that day. The 183 left are mostly in the timer's handler (28), the choice of speed
(21) and the initialisation of the slot buffers (15).

The 21 of the choice of speed, `GetProcessorSpeed`, were read one by one that evening,
and none is a behaviour left unchecked. One is the task given its own slack back, read
above. Fourteen move the edge of a comparison where both
sides give the same speed: a bound set to the value it is compared with, or a time left
equal to the work, which the fastest speed alone does at the operating points of the
RP2040 and the host, 24 and 102 in 256ths. Two read DRA's excess with the opposite sign:
`DRASimUpdateElapseTime`, called before each choice, has already spent it, and a build
made to trap on a non-zero excess there ran 4,000 task sets under DRA and DR_OTE and the
host test without trapping. Two give DM_SLACK a slack of 0 or 1 tick, which the fastest
speed absorbs at those points. One starts the slack's bound at 1 rather than 0, the same
again; and the last runs at the slowest speed a task with no work left on record, which
the task ends at the instant it is dispatched, no trace showing it run.

The same evening the random task sets were taken across the wraparound: the host test's
trace starts at any phase of the counter, and half the sets of `tools/differential.py`
now start just short of 2^30, the trace still counting time from the start, so that
every check and the speed reference read them unchanged. Every set had run within its
first 6,000 ticks, far from the wraparound, which only the hand-written tests `timewrap`,
`firmwrap` and `longperiod` reached. The hard kernels held. The others did not, three
ways:

- **OTE lost its time before each wraparound.** The power-aware kernel read the next
  arrival from its low part alone: an arrival past 2^30 seemed past, and the last
  instance before each wraparound ran at the fastest speed. Faster than needed, no
  deadline at risk. `GetNextArrival` counts the wraparound in.
- **A task ending at the very tick of the wraparound read a time 2^30 short.** The
  counter had wrapped, the timer's handler not yet shifted the kernel's times. DM_SLACK
  credited the slack that task left with 2^30, which the handler then took from the work
  of the next task; the soft kernel saw every deadline 2^30 ahead, admitted an optional
  instance that could not end in time, and the host test's scheduler never returned.
  Slowed down on 2^30 of slack, tasks of lower priority could miss their deadlines: the
  window is the few cycles between the counter's wrap and the handler, every eighteen
  minutes on the RP2040. A time read by a task is now taken past 2^30 when it lies half
  the range of the counter before the last time the kernel set (`GetTaskTime` in both
  kernels); `timewrap` and `firmwrapend` end a task at that tick, and the second fails
  without the correction.
- **The reference was wrong twice**, the kernel right: the handler zeroes the server's
  deadline and the time the excess was counted to when it shifts them below 0, losing the
  excess counted before the wraparound, as the kernel's comment says; and a task whose
  WCET the simulation has used up, dispatched again at the wraparound, gets no time from
  DRA, as `GetDRASlackTime`'s comment says. The reference takes both now.

Both corrections of the kernels were made with the user's agreement.

Run again across the wraparound, the power-aware kernel's mutants scored 77.5 % (621 of
801), the hard kernel's survivors lost 3 more, the code that shifts its times (388 of 484,
80.2 %), and the soft kernel's 4 of the 179 found again, all in the code that carries the
arrivals of its mandatory instances past the wraparound. A mutant read and shown equivalent is since declared in
`test/host/equivalent-mutants.jsonl`, with its reason, found by what it changes as
`--survivors` finds them: it still runs, and one declared equivalent and killed is
reported, the declaration being wrong. The score is given twice, of all the valid mutants
and of those not declared: the power-aware kernel's 26 read that evening, the 21 of the
choice of speed and 5 of DM_SLACK's slack, give 80.1 % of the 775 others. No mutant is
declared that has not been read, and none for being hard to kill. The term those two mutants
of the excess changed, always 0 there, was then taken out of `GetProcessorSpeed`, with the
user's agreement, and its mutants with it: 24 remain declared.

The hard kernel's 96 survivors were read the same evening, each settled where it could be
by a build of the mutant, a model given the fault, or a crafted task set. 47 change
nothing the kernel promises: stores to zeroed sentinels or overwritten before any read,
ties the algorithm leaves open, compiler barriers whose ARM code is the same without them,
assertions of DEBUG_MODE. 46 of them are declared (one, the deadline-monotonic priorities
all shifted by one, was left out, its argument near 255 tasks not tight enough): 88.6 % of
the 438 others, from 80.2 % of all. 5 more the host cannot see and `tools/check_order.py`
kills on the ARM builds. The 44 left are behaviours no test reads, 17 of them visible only
on a board, where any example would catch most (the timer never started, the idle task
left masked). Those the host can reach, ranked by what they would cost: an event-driven
task elected still a zombie, its context then discarded; queue indices taken from leftover
SRAM, which the host's uniform fill of 0xA5 hides; an event released late across the
wraparound; the 3-slot writer taking the slot being read; a signal dropped until the next
interrupt. The reading also found a fault of the host test, not of the kernel: a deadline
armed past the wraparound made `HostTicksToNextEvent` skip the interrupt of the wrap, which
the random task sets, their signals far apart, never met.

Tests were then written for the survivors the host can reach, and each kill checked by
running the mutant again:

- a task the kernel elects is never a zombie but while it ends (212);
- the host's port asks what a board needs at start: the timer initialised, then started
  with interrupts masked; the handler only once it is; the start asking a context switch
  and unmasking; the idle task sleeping; the comparator's flag raised at the deadline
  armed. Eighteen mutants seen only on a board die so (12 to 19, 89, 222, 241 to 248);
- memory from OSMalloc filled with a small count in each word rather than one byte, under
  which two indices forgotten no longer read equal and a size forgotten no longer reads
  huge (338, 340, 348, 370). It hid two others, a narrow field forgotten reading the 0 of
  a small count's upper bytes as if set (46, 385): no fill sees all, and the IPC tests run
  under both, 0xA5 staying the default;
- queues of 2 to 4 nodes taken across the wrap of their indices from 12 phases (250 to
  254, 330, 331), and a slot of one byte read by reference (460).

Two of the 3-slot buffer's table, 413 and 416, the writer taking the slot being read,
still survive a reference held from every state: the models and the litmus tests catch
them, `mutants.py` does not. And `tools/differential.py` draws since the same night an
event-driven task signalled at the edges, a signal just before the wraparound and a second
within its workload, or one at the very tick of the wrap, or at time 0. Its first run
failed on most builds; read with the trace, no failure was the kernel's. Its model gave
the server's deadlines in the order of the signals where the kernel gives them in the
order of the releases, a signal before its task's last deadline being released at that
deadline, after later signals of other tasks; it counted signals the kernel coalesces, one
kept pending per event, the rest dropped (TestSignals), which sets that may coalesce are now
left out for; and a ninth task, past the host's eight, was dropped unseen, which the host
now refuses aloud. 2,000 sets a build agree.

Run whole on 9d9157c, at 300 task sets a build, the hard kernel's mutants scored 86.4 %
(418 of 484), 95.4 % of the 438 not declared; the power-aware kernel's 81.5 % (651 of
799), 84.0 % of the 775 not declared; the soft kernel's 79.0 % (572 of 724). The run also
showed a fault of `tools/mutants.py`: a test that ran past its time limit was reported
killed, but only `make` was stopped, and three soft mutants (217, 218, 247) kept spinning
an hour and forty minutes after the campaign, their parent gone. Each step now runs in a
process group of its own, killed whole on the limit.

Eight of the hard kernel's survivors had never been read (0, 127, 130, 206, 225, 226, 236,
459 of that run); they were on 2026-10-10, each against a build of the mutant, and six
are killed since by a test that fails on it:

- the server's deadlines starting from 1 rather than 0 (0): an event-driven task signalled
  at time 0 got the deadline 301 for 300, and a periodic one arriving at 151 with the
  deadline 301 preempted it, one tick behind (`signalzero`);
- the comparator's alarm in the window between the timer handler arming it and its
  PENDSTCLR, as when the next arrival is a tick away (130): the clear drops the soft
  interrupt the alarm raised, and the handler, looking at the alarm's flag only with an
  event-driven task to reschedule, left the arrival to the next interrupt, the
  wraparound, 18 minutes on the RP2040. The host's clear did nothing; it now drops the
  requests made so far, as the target's single pending bit is, and `windowalarm` raises
  the alarm just before it. The same test finds the clear left out (127), which on the
  target only runs the handler again for nothing;
- the start no longer setting the stack's base (226), which an application that
  allocated nothing before it would have started at 0: the host's OSMalloc keeps that
  base as the port's does, and `notask` and every start check it;
- the indices of an event's queue left to wrap at what OSMalloc held (236): under the
  count fill, three tasks on one event lose their turns after 8 operations, 8 not being a
  multiple of 3 (`eventturns`);
- a slot of no byte, which `OSInitBuffer` takes, read by reference as data (459): the
  reference must be NULL, as the function's comment says.

One is equivalent and declared (206): an event-driven task signalled at the very instant
of its previous deadline goes to the arrival queue for that instant rather than straight
to the ready queue, and the same pass of the handler, finding the timer already past it,
releases it at once with the same deadline; only the order of the releases of one pass
may change, which the list they come from, last in first out, already leaves open. And
one is a behaviour no test reads (225): the start allocating 4 bytes rather than 0, the
heap 4 bytes smaller and the stacks starting 4 bytes lower, which the host's OSMalloc,
taken from the C library, does not count. The survivors run again at 300 task sets, the
six died, and the hard kernel's mutants score 87.6 % (424 of 484), 97.0 % of the 437 not
declared. That run showed a fault of `tools/mutants.py`: scoring a part of the mutants,
it ranked mutants alike among that part alone, and a declaration of the second of two
alike, 98, matched no survivor; they are ranked among the whole source's since.

Most of the soft kernel's survivors are in the test of its optional instances,
`IsTaskSchedulable`: a mutant that leaves out a task's last partial instance in the
window under-counts by one WCET at most, and random task sets seldom come that close. A
reference of that test, computed apart and compared decision by decision, would read
them; it is not written. What the test promises is checked instead, since 2026-10-05:
`tools/differential.py` runs each (m,k)-firm set a second time, tasks taking less than
their WCET, and simulates from each optional instance admitted the schedule in which
every instance takes its WCET; the instance must end by its deadline. Kernels made to
leave out the instance's own WCET, half the mandatory work or the partial instance fail
it under DM, and hang on the overload guard under EDF. The survivors pass: they
under-count within what still fits.

Two things kept that check short, found on 2026-10-07 from a question of the user's, about
instances ending early. Every instance of a task took the same time, so an end never came
earlier than the one before; and nothing compared the kernel's decisions with its test.
The kernel tests an optional instance once, when it first reaches the head, and drops it
for good: in `firmonce`, an instance dropped at 100 would pass the same test at 121, the
task before it having ended in 1 tick of its 50, and is never tested again. That is no
fault of safety, and the test pins it as the kernel has it. Since then the host's trace
takes a time for each instance (`D task instance takes`), drawn apart for the second run
of each (m,k)-firm set, and `tools/differential.py` holds the kernel to its test, written
from `IsTaskSchedulable`'s header rather than its code. The kernel's count of mandatory
instances rounds up, ceil(n m / k) over n whole periods, where an even pattern may hold
fewer; the reference rounds as the kernel does, a rule taken from its code after the
pattern alone had admitted instances the kernel dropped, two of them read in the trace to
that rounding. An optional instance that
starts must pass the test at its start or at an end before, where it may have been
admitted, a release at that instant preempting it. An instance dropped must fail it where
the trace shows when it was decided: at the first point the kernel considered optional
instances while it waited, if it was the only one undecided there. The first version took
for such a point an end where an instance admitted before, then preempted, was still due,
and failed the kernel twice in 2,000 sets; read in the trace, the kernel was right. On
1,000 sets of each build, of the 2,794 optional instances dropped under EDF, 2,231 are so
checked; 2,141 could have run, every instance taking its WCET and keeping its deadline,
and 331 would have passed the test at a later point the kernel scheduled from; under DM,
1,789, 1,441 and 224 of 2,072. Testing a dropped instance again would win the last number.
A tighter test could win most of the one before, but not by leaving out the instances due
after the optional one's deadline, as tried the same day (see "Hypotheses that were
wrong"): it has to hold the deadlines of the instances it delays.

The test that does, under EDF, is the processor demand criterion from the instant of
decision: the instances released from then on are feasible without the optional one, and
with it every deadline from its own on must have the instant, its WCET and the mandatory
instances due by that deadline before it, until the processor would first be free, from
where the schedule is the one without it. A prototype in `tools/differential.py`
(`demand_test`), judged against the schedule as it was, admitted 2,140 of the 2,141 drops
that could have run, none that could not, and refused none of the 23,481 instances the
kernel admitted; the one left was due past the end of the run, which the simulation does
not see. As admitting an instance changes what follows, `tools/firm_admission.py` runs
the whole schedule again under each test, on 2,000 sets, every instance taking its WCET,
then a time of its own: with the kernel's test 75.4 % of the optional instances run, with
the demand test 81.3 %, and with no test at all, admitting whatever fits by itself, 81.3 %
as well, missing 279 deadlines; the other two miss none. Its cost is the walk of the
instances released in that busy stretch: one in the mean, 75 at most over 5,000 sets of
`tools/differential.py`, where the kernel's test walks the tasks once. Not in the kernel:
a busy stretch has no bound but the load, and a kernel would have to stop the walk at a
bound of its own, refusing past it, which stays safe.

It went into the kernel the same evening, with the user's agreement, as `DemandFits`,
under EDF when no event-driven task has bandwidth reserved, the old count staying for the
rest and for DM: the busy stretch as the fixed point of the work released before it, then
the demand at the optional instance's deadline and at each mandatory one's up to the end
of the stretch, the mandatory instances of an interval counted as ceil(j m / k) over the
pattern's numbers, a formula checked against the pattern for every m and k up to 40.
`OS_FIRM_DEMAND_BOUND`, 16, was chosen on `tools/firm_admission.py`: on 1,000 sets, 8
ran 81.4 % of the optional instances, 16 and more 81.8 %, as with no bound. The reference
of `tools/differential.py` follows it, bound included; on 5,000 sets the kernel admitted
none it refuses and dropped none it admits where the instant is sure, and kept every
deadline; under EDF the drops fell from 2,794 to 856 on the first 1,000, and
`tools/firm_admission.py` runs 81.1 % of the optional instances, from 75.4 %. The Pico's
images under DM are the same byte for byte. Two host tests reach what the random sets do not: a
refusal at the optional instance's own deadline, with an event-driven task of WCET 0,
which alone reserves no bandwidth, waiting in the arrival queue (`firmdemand`), and a
period past 2^30 in the busy stretch (`firmdemandlong`). What a decision costs was measured
on the Pico the same night, at 125 MHz, by `BenchAdmissionPico` (`make KERNEL=SOFT
bench-admission`, `tools/admission_cost.py`): the eight tasks of the costliest set
`tools/firm_admission.py`'s simulation found among 1,500, each instance spinning a
pseudo-random quarter of its WCET to all of it, each test timed in cycles by SysTick.
The count it replaced, on the same tasks: 16.6 µs in the mean, 33.9 at most, over 20,000
tests, and the board ran on. `DemandFits`: 78 µs in the mean and 419 at most over its
first 42 tests, interrupts masked around each, after which the kernel's overload guard
stopped the board, an instance still running at its next release; interrupts left as
the kernel has them, 67.7 µs and 420 over 634 tests, then the guard again, a mandatory
instance not started by its next release. The test takes the instant it is called for
the instant the optional instance would start, and its own time, up to 420 µs where the
shortest period is 283, is counted nowhere; the count's pessimism covered its 34 µs.
The RP2040's core divides in software, and each count of an interval divides several
times. With the user's agreement the kernel went back to the count the same night,
under EDF as under DM, the reference of `tools/differential.py`, `docs/api.md` and the
host tests with it; `DemandFits` stays in the history, its prototype in
`tools/differential.py` and `tools/firm_admission.py` to count what a test that counted
its own cost might win. The bench, now timing the count, stays. On the same tasks the count took 13.8 µs in the
mean and 31.0 at most over 20,000 tests, interrupts masked, and 13.9 and 31.7 left
unmasked, the board running on both times; the 16.6 µs of the run before went through
the share reserved for event-driven tasks, which the bench no longer reserves.
Of its 60 mutants that compiled, the random sets killed 40. Read, 7 of the 20 left were
faults no set had met, killed by host tests. Four, at the edges, were found again by
simulating the schedule under the fault (`tools/firm_admission.py`'s simulation, the
kernel's test written in Python) and confirmed with the kernel so made: a demand equal to a
deadline refused, at the optional instance's own or a later one, an instance released
at the very end of an interval or a period before it counted (`firmdemandset`, three
sets whose count of instances run over 6,000 ticks changes). Three were read: a period
past 2^30 left out of the stretch, which admitted an instance that delayed another past
its deadline (`firmdemandlongdue`), and, twice, a task of WCET 0 divided by, which on this
Mac's arm64 reads 0 and refuses, and traps under UndefinedBehaviorSanitizer
(`firmdemand`). An event-driven
task read as a periodic one, and an instance released at the end of the stretch counted
against the bound, died with them. 3 are declared equivalent (the demand at the end of the
stretch, at most the stretch by its fixed point; the optional instance's deadline checked
twice; a WCET of 1 divided by itself). 8 stay, counted: 5 change only how long the fixed
point may take before the walk of the stretch refuses past the bound, which no test
times; 2 are overflow guards at 2^30; and one checks the deadlines past the stretch,
which hold only as long as the mandatory instances can be scheduled at all, what the
kernel assumes and does not check. 49 of 60, 86 % of the 57 not declared; the two
mutants of its call both die.
Run again on the soft kernel's 152 survivors at 300 sets a build, the reference killed 4:
a mandatory arrival one tick before the deadline left out (151), a test that admits at
the deadline itself (213), an optional instance put at the head of their queue rather
than by its deadline (364), and,
by a drop the test refuses, a partial instance counted whole (169). Of the survivors left
in the periodic part of the test, 8 were read and change nothing (a scan that goes past
the deadline adding 0, ties where both branches add the WCET), and are declared with the
guard of the test, 107, which `IsTaskSchedulable` makes redundant; 3 more, overflow guards
weakened or skipped for a WCET of 1, would differ only near 2^30 ticks of work, and stay
counted. The rest are in the event-driven part, which the reference leaves out. The
kernel scores 79.6 % (576 of 724), 80.6 % of the 715 not declared.

A quarter of the power-aware kernel's survivors were in
the choice of speed (`GetProcessorSpeed`, DRA's simulation, DM_SLACK's slack): a mutant
that picks a faster speed than it needs keeps every deadline, and nothing held the
policies to the speeds their specification gives.

Since 2026-10-05 `tools/speed_reference.py` does. It computes the speed of each dispatch
from the ZottaOS manual's chapter 6, written before the kernel's code was read, and
`tools/differential.py` fails a speed other than its own. Run against the kernel, it
first disagreed on most builds, and each disagreement was read in the kernel before
anything was changed: three were the reference's own mistakes (the releases past the end
of the run, which the kernel knows; the work received, which the kernel keeps in whole
ticks as the manual's figure does in integers; and when an end decides, before the
release at the same instant); the rest were the kernel departing from the manual, each
towards a faster speed, none towards a missed deadline. The kernel set the speed only
when the task to run changed, where the manual does at every timer interrupt, so that a
release that did not preempt reclaimed nothing; and DM_SLACK took its slack only for an
instance not alone, where DR_OTE takes the slower of its two. Both were ZottaOS's, and
both were corrected the same day, the reference agreeing again. Two departures stay,
written in the reference: its EDF* breaks ties the other way from the paper, as its
header says, and DM_SLACK keeps one slack, the last left, where the manual keeps one per
task. The reference agreed with the kernel on 1,000 task sets of each of the five
builds, before and after the corrections, on the Mac and in the CI's image before. Of
the 171 mutants of the code that chose a speed before the corrections, 100 were killed
without the reference, 109 with it, and 112 at the 300 task sets a build the CI runs
(`tools/mutants.py --sets`); after them, 104 of the 164 of the same code. The survivors
left were mostly code of the event-driven tasks under DRA, which the reference then
left out, the manual not saying where they stand in its simulation, and comparisons
whose edge gives the same speed. Under OTE and DM_SLACK the reference takes event-driven tasks
in too since the same evening, their next release bounding the stretch, and under
DM_SLACK the slack an event-driven instance leaves: of the 17 mutants of the code that
bounds the stretch, the 6 left are such comparisons. Two of DM_SLACK's survivors were
first written as behaviours no test reads; read again on 2026-10-06, neither is. Leaving out the
`CompilerBarrier()` before the SC that hands a slack over changes nothing GCC emits for
the RP2040, the only target that builds DM_SLACK: `DMSlackCalculateSlack` disassembles
the same with and without it, which keeps it as a guard against a compiler that would
sink the three stores past the call. And a task given its own slack back, `>=` for `>`
on the priorities, never meets one: a task keeps every deadline running its remaining
WCET at the fastest speed from any point, so that its end plus the slack it leaves is
at most its deadline, and the slack, run out with the time elapsed, is gone by its next
release, at least a deadline after the one before.

On 2026-10-06 the reference took in the event-driven tasks under DRA and DR_OTE, where
the manual is silent, as the kernel has them: an instance in the simulation by the
server's deadline, and the server's share of the time it has nothing pending taken as
excess, counted and rounded at each update of the simulation, at the instants the
kernel makes them. With its instances alone in the simulation, the reference disagreed,
the kernel always slower; 10,000 task sets under the two builds, checked for deadlines
only, missed none, before the excess was written in. With it, DRA agreed on 3,300 sets.
DR_OTE disagreed once more, the kernel faster: an event-driven task that might be
released at once sent it to the fastest speed, DRA's time left out, where OTE alone
should have lost its time, as DM_SLACK has it since 80cc6dc. Corrected with the user's
agreement, it agreed on 3,000 sets. A kernel made to count no excess fails its
first task sets under both builds.

## What the README claims, and what checks it

That lesson, a claim no test reads is not checked, was applied to the README on
2026-10-04: each claim it makes about behaviour, set against what reads it in the
behaviour, a test, a model, the board or an instrument.

| Claim | What reads it | Found |
|---|---|---|
| Scheduling by EDF or DM | the host test's view of the task control block; `tools/differential.py`, each trace checked against the algorithm of its build | holds |
| No periodic tick: the timer interrupts at a release only | nothing on the host; on the U5, the idle task's 454,487 Stop 2 in three hours, which a tick would have cut short | to add: the trace's timer interrupts set against the releases. Done on 2026-10-05: `tools/differential.py` reads every interrupt of the comparator, and a host port made to interrupt every 500 ticks fails its first task set |
| The power-aware kernel slows down only as far as every deadline holds | `tools/differential.py` under its five builds, with kernels made to run too slow; the host test's speeds | holds |
| It slows down as far as DRA, OTE and DM_SLACK compute | nothing until 2026-10-05: a kernel faster than its policy kept every deadline. Since, `tools/speed_reference.py`, from the ZottaOS manual, each speed of `tools/differential.py`'s traces, and since 2026-10-06 DRA's and DR_OTE's with event-driven tasks | **did not quite**: a release that did not preempt reclaimed nothing, and DM_SLACK left its slack out of a task alone. Both corrected the same day (80cc6dc). DR_OTE left DRA out when an event-driven task could be released at once, corrected on 2026-10-06 |
| Overload drops chosen (m,k)-firm instances, not deadlines at random | the host test's `firm` runs, `firmoverload*` | holds |
| The queues take no lock | `docs/architecture.md` | **false since 2026-09-25**: `OSSuspendSynchronousTask` masks interrupts around the enqueue of a task on its event. The README now says so |
| The queue between the cores went from fifteen barriers to six | the model `fifo_mp.py` | **stale**: seven since 2026-09-29 |
| The models found four bugs | `docs/method.md`, "Hypotheses that were wrong" | **stale**: six, two of them in the queue between the cores |
| Two boards checked at each commit | `tools/board_ci.md` | **stale**: three, the Pico 2 since 2026-09-28 |
| 87 to 90 % of each kernel's lines run | `tools/coverage.py` | **stale**: every line runs or is excluded with its reason, 90.5 % of the branches |
| How much current Stop 2 saves is not measured | `docs/stm32u5.md` | **stale**: measured on 2026-10-03 with a PPK2 |
| Whether DVFS saves energy is not settled | `docs/roadmap.md`, items 3 and 4 | **stale**: settled against it on the U5, some 10 % at best on the RP2350; open on the RP2040 |
| The program shown is a whole program for the Pico | since 2026-10-04 the Pico's Makefile builds it from the page, and the Renode suite checks that its LED blinks at 1 Hz | **false**: it never routed GPIO 25 to the SIO nor made it an output, and on the chip the LED would not have lit. The page now shows the whole program |
| Responses stay under the bound of the analysis | `tools/response_times.py` on the traces in `docs/data` | holds, and since 2026-10-04 under EDF too |
| The figures: 4,716 bytes, 3.2 µs, +28 ppm, 108,000 activations | each dated, with its source | measurements of a day, not claims about every build |

Six claims had gone stale as the work moved on, two were false, and two were read by
nothing. The README was corrected the same day, and its program is built and run from
the page since. The check of the tick was added on 2026-10-05.

## What this changes in the repository

The CI runs the kernel, under Renode and on the host, besides building it. That is how
the `-O2` defect and the scheduling one above were found. A measurement is written with
its date and its deviation (the +28 ppm of the frequency counter, `rp2040.md`). A result
that did not reproduce is said so. A commit message gives its reasoning, wrong turns
included.

## Audits, and what they leave unproved

### The line-by-line audit

The new code, the RP2040 port and the Cortex-M layer, has been audited line by line.
That turned up four defects: a race between `OSEnqueueUART` and its interrupt, a
zero-sized transmission that emptied 64 KB onto the port, missing header dependencies,
and stale comments.

The kernel inherited from ZottaOS went through the same audit on 2026-09-25. Each finding
was reproduced by a host test that failed before its fix (`test/host/README.md`):

- a slot buffer lost a slot to a reader preempting its writer, and read fields
  `OSMalloc` had not cleared;
- an event-driven task ending at its deadline was inserted twice in the ready queue;
- an event-driven task waiting past a wrap was sorted before periodic tasks due earlier;
- deadlines were left unshifted at the wrap in the soft kernel's optional instances, in
  the power-aware kernel under deadline-monotonic scheduling, and in DRA's simulation
  queue;
- a DM_SLACK slack was given twice;
- a task past its WCET was slowed to the slowest speed;
- the soft kernel's test of optional instances overflowed;
- the kernel accepted creations it cannot count with, which it now refuses.

In the compiled code of the Cortex-M0+, three of the five emulated load-linked read the
value before setting the reservation, so an interrupt in between went unseen. A compiler
barrier now keeps the order.

The RP2350 and STM32U5 ports went through the same audit on 2026-09-29, by two agents
reading one port each, every finding then checked by hand against the code. Two defects
were each reproduced under Renode by a test that fails on the code before:

- the STM32U5's idle task in Stop 2 slept past the 2^30 wrap when the next arrival was
  an event-driven task's beyond it, and the kernel's clock lost the overshoot, up to 2 s
  on the board, with nothing counted late (`Stop2EventWrapU5`, `stm32u5.md`);
- `OSEnqueueUART` primed the transmission at task level with only the UART's interrupt
  masked, so that a task preempting another inside it emptied the same buffer from where
  the other stood: 179 lines of some 18,600 came out broken in 0.3 s on the Pico 2, 60
  of some 400 on the Pico, whose senders then all but stopped (`UARTSendersPico2`,
  `UARTSendersPico`). The RP2040 had it too, from the port the RP2350's was taken from;
  the first audit had closed the race of the call with its own interrupt, not with
  another caller. On the Pico 2 itself, on 2026-09-30, 18,106 lines in 20 s came out
  whole (`tools/pico2_uart.py`).

The smaller findings were fixed the same day without a test of their own, each read
against the code and the whole suites run again after: the U585's vector table trapped
as reserved five interrupts it has (SAES, AES, PKA, OTFDEC1 and OTFDEC2), taken from the
U575's; the flash prefetch was on, the condition of erratum 2.2.26, and served nothing
to images in SRAM; `OSInitUART` said TRUE with its queue unallocated, and
`OSInitTimerEvent`, which said nothing, wrote past an empty block for no node; the Pico
ports released blocks from reset by a read-modify-write of `RESETS`, which could put
back in reset what core 1 had just released; `OSGetStop2Counts` unmasked the interrupts
whatever the caller's state. The comment on the booster's clock was wrong too: it takes
PLL1's source before the divider M, 16 MHz from the HSE, not 4.

One finding was left for later: without the HSE, PLL1 took the MSIS at 3.998 MHz, under
the 4 MHz floor of its input. Since 2026-09-30 it takes the MSIS of range 2, 16.0017
MHz; an independent review of that change found two faults the same day, both fixed.
The NUCLEO-U575ZI-Q, taken for a board without the HSE, has one (the hypotheses above):
the path first ran on a chip as `SleepNoHSEU5` on the UNO Q on 2026-10-02 (`stm32u5.md`,
"Without the HSE").

### Two fixes that were wrong

The endurance test (`SoakPico`, `tools/soak.py`) showed two of those fixes wrong the
same day, once an interrupt and phases of high load had joined it.

A slot buffer's status, set after its slot rather than before, let a reader take the new
slot early while the status still said unread from the slot before, and take it again
once the writer had marked it unread. On the board, 37 slots were read twice in 79 s,
none torn. No order of a status kept apart from the slot is right. Each slot now carries
its number, and a reader that takes each slot once compares it with the last it took.

An event-driven task signalled while it suspended itself was assumed to be the task the
timer handler had interrupted, and the handler finished it in its place. When a task of
higher priority had preempted it and signalled it instead, the context of that task was
discarded. The soft kernel hung under Renode, and the host test segfaulted under
deadline-monotonic scheduling. Interrupts are now masked from the enqueue of the
suspending task until it leaves the ready queue, so the handler never sees such a task.
Both defects have a host test that fails on the code before (`test/host/README.md`).

### Races and limits closed after the audit

The audit left races open and listed limits of the design, seven in all. They were
closed on 2026-09-25 and 26. Each was reached on the host, except strict aliasing, which
is turned off.

The counter can wrap while the timer handler runs, after the handler has found no
overflow. The host reaches that window with a hook (`wrapinside`). The handler then reads
a time from after the wrap while its arrivals are not yet shifted, and releases nothing.
It serves them once it finds the overflow flag raised in the meantime, which it now tests
again before it returns. The test fails with that second test taken out.

In the power-aware kernel, a task ending between two interrupts left the next task at
its own speed. The flag that told the handler to set that speed was raised before the
task became a zombie. A first interrupt, which found the task still running, cleared it.
A second one, once the task was a zombie, took the next task for one that had been
running. The handler now reads `_OSNoSaveContext`, which only the context switch
clears. The host takes the timer interrupt at the kernels' compiler barriers
(`endinside`), and with a barrier in the old window the kernel before the fix fails it
(`test/host/README.md`).

Under DM_SLACK, a task ending read the time before its reservation. An interrupt that
shifted the clock at a wrap in between left the task a time from before the wrap against
a last update from after it. The slack, left for the task ending after an idle time,
then grew by the 2^30 of the shift at the next arrival. The time is now read inside the
reservation. The host runs a task set in which a task ends at the last tick before the
wrap, and takes the interrupt at each of its time reads and barriers (`timewrap`). The
kernel before the fix misses a deadline there.

An optional instance can still be ready at its next arrival, never started. The host
reaches that case with tasks that take time, the mandatory instances keeping the
processor busy across its period (`firmwait`). The kernel takes the instance out of the
ready queue as it should, under both algorithms. An optional instance already started
at that point is still an overload the kernel stops on (`DEBUG_MODE`), by design. The
schedulability test lets it start only if it ends in time, so only a task running past
its declared WCET brings it, or work the test leaves out. Under EDF the test left out
the event-driven tasks when no share of the processor was declared for them, although
each declared its WCET and workload. An optional instance started, an event delayed it,
and the kernel stopped on that guard at its next arrival (`firmeventwait`). The test now
reserves at least the share each event-driven task takes, its WCET over its workload.

The indices of the wait-free queue had 16 bits. An operation completed by the one
preempting it could still move an index it had read, once 65,000 operations during that
preemption had brought the index back to the same value, and the queue lost an item.
The host reaches this by running those operations inside the LL of a dequeue
(`test_ipc`). With 32 bits, as the queue between the cores already had, it takes 2^32
(2026-09-26). Under Renode, the preemptions of the FIFO example no longer fell inside the
queue's operations once the code had grown longer, so the example now varies the length
of its loop. Strict aliasing, which GCC does exploit here, is turned off (below).

### Scale

The execution tests exercise three or four tasks, the host test ten. Nothing here
establishes how the scheduler behaves with thirty. The 2³⁰ wrap of the kernel clock,
about eighteen minutes away on hardware, is crossed on the host, under Renode, and on
the UNO Q's STM32U5 by its endurance test (2026-09-26), four times in its first hour and
a quarter.

### Between the cores

The RP2350 port was audited line by line on 2026-09-29 (above). Its emulated platform
acknowledges its clocks blindly (`emulation.md`); a Pico 2 has run it since 2026-09-28,
its clocks checked and its outputs timed on a frequency counter (`architecture.md`).

Between the cores, the models cover the orders of access the architecture allows. That
the compiled code keeps the order they need is checked by `tools/check_order.py`, in the
CI, on every build of the Pico and the Pico 2, and of the STM32U5 since its port
(2026-09-25), on every path through the buffers' functions.
`tools/check_order_mutants.sh` shows the check failing without any one of the barriers.
There are nine in the two buffers' source since the status of a buffer, which the models
leave out, is set after its slot is handed over (2026-09-25). The queue between the
cores had no such check until 2026-09-30: only its source and its model named its seven
barriers. The check now follows the queue through its LL and SC, which are calls, and
each barrier removed from the source is caught, each by the pair of accesses it stands
between. An independent review found two holes in it the same day, both closed: a path
went on past a return GCC writes as `ldmia.w sp!, {..., pc}`, which the check took for
an ordinary load, and the pairs let the E5 of the next round stand for the E10 that must
follow the LL of the place, so that a queue without E10, or with it before the LL,
passed. Changes to the algorithm that keep its order, as a test turned around at E11,
are not the check's to find.

The check cannot see a reordering by the processor that the architecture does not
allow, which is the models' premise. Nor can it see code the compiler might emit for
another version or optimisation level until it runs there. Without the barriers' memory
clobber, GCC 16.2 happened to keep the same order at -O2 (2026-09-25). The clobber is a
guarantee, not a fix to an observed fault.

### Between the cores, on a processor that reorders

The Pico 2 keeps its accesses in order: `LitmusPico2.c` found none reordered. On the
board, then, the queue between the cores shows that it runs, not that its seven barriers
are needed. `test/litmus` (2026-10-04) compiles `Escapement_CoreQueue.c` unchanged for an
Apple M3 Pro, its LL and SC made LDXR and STXR, its DMB a `DMB ISH`. That processor does
use the freedom Armv8-M gives Normal memory. Two threads first run the three classic
litmus tests, then each enqueues its own numbered items and dequeues in turns, under a
check of each item's contents, of each producer's order and, at the end, of every item
dequeued once. Each mutant then leaves one barrier out of the processor while keeping it
in the compiler.

| Run, 2026-10-04 | Result |
|---|---|
| SB, MP, LB without a DMB, 19,999,744 instances each | 12,233,728 SB and 26,428 MP reordered; LB never |
| The same with a DMB | none |
| The queue, two places, its seven barriers, 4 runs of 1 to 20 million items each way | 0 errors |
| Without the barrier after E9 | caught in 5 runs of 5: an enqueuer's items out of order |
| Without the barrier after D9 | caught in 1 run of 5, with four pairs of threads running |
| Without the one after E5, E15, D5, D6 or D15 | caught in none of 2 runs each |

Two barriers are now shown needed on a real processor, not only in the model. The five
the bench does not catch are no less needed: `fifo_mp.py` finds each one's run against
the architecture, and this processor does not happen to take it. The bench stands for a
processor that reorders, not for a Cortex-M, none of which is known to reorder as far.

The kernel's 3-slot buffer went on the same bench the same day (`test/litmus/slots.c`).
`EscapementHard.c` is compiled as it stands, through the host port of `test/host` built
with `HOST_LITMUS`: its barriers are a `DMB ISH`, its LL and SC an LDXRB and an STXRB.
A writer writes items of 16 bytes, a number and a check word, and a reader copies the
latest out. A slot written while it is read, or named before it is filled, comes out
torn or short, and the numbers read must never go backwards.

| Run, 2026-10-04 | Result |
|---|---|
| The buffer, its five barriers, 4 runs of 5 to 20 million items | 0 errors, some 64 million reads in 5 s |
| Without the barrier before `Latest` is written | caught in 5 runs of 5: a slot read short |
| Without the one before the reader's LL of `Reading` | caught in 5 runs of 5: a torn slot |
| Without the one after `Latest`, the one before `Status`, or the one before `Reading = 3` | caught in none of 5 runs each |

The two caught are the two the model gives the plainest reason for: the slot filled
before it is named, and the reader's request seen before it reads the slot.

The 4-slot buffer went on it next (`slots.c -4`), with no LL or SC: each side writes
only words of its own.

| Run, 2026-10-04 | Result |
|---|---|
| The buffer, its five barriers, 4 runs of 5 to 20 million items | 0 errors, some 56 million reads in 4 s |
| Without the barrier before the writer names its slot in `Index` | caught in 5 runs of 5: a slot read short |
| Without the one before it names the pair in `Latest` | caught in 5 runs of 5: a slot read short |
| Without the one before the reader takes the slot from `Index` | caught in 5 runs of 5: a slot read short |
| Without the one before `Status`, or the one before the reader names its pair in `Reading` | caught in none of 5 runs each |

Of the three structures, the 4-slot buffer shows the most of its barriers needed on
this processor, three of five, against two of five for the 3-slot buffer and two of
seven for the queue. The barrier before `Status`, common to both buffers, is never
caught: it matters only for the first item, which the bench reads among millions.

The first version of the bench had a fault of its own. In place of the barrier it left
out, it counted the passes with an atomic add. Between an LDXR and its STXR, that
exclusive access cleared the reservation, so every SC failed and the mutant without the
barrier after E9 looped for ten minutes. The bench looked like it had found a livelock
in the queue. Its mutants now add no access to memory, and a watchdog stops a thread
that makes no progress for 10 s.

### Store order on one core

The same holds on one core for the stores a task makes that the timer interrupt may
find half done. The processor keeps their order; the compiler need not, and did not.
Besides `OSEndTask` (above), the soft kernel's `ScheduleNextTask` promotes an optional
instance in three stages. It marks the instance `STATE_ACTIVATE`, moves it to the head
of the ready queue in three steps, and sets it `STATE_INIT`. The interrupt completes a
promotion it finds marked. GCC saw the mark overwritten and dropped it, on the
Cortex-M0+, the M33 and the M4 alike. A promotion interrupted there left the task both
at the head of the queue and in the list of optional instances. No test had shown it.

Compiler barriers now order these stores, those of the two drops in the same function,
and the clearing of `SetActiveTaskRemainingTime` in the power-aware kernel (a flag since
removed, above). `tools/check_order.py` checks the compiled order of each on every path,
on every build of the Pico, the Pico 2 and the F4 (2026-09-25). The F4 port was removed
on 2026-09-26, and the STM32U5's builds are checked in its place. Run on the kernels of
the day before, the check reports exactly the faults found by hand: `OSEndTask` on the
M33 and the M4, and the promotion on all three. The mutants script shows each rule
failing on a source whose order is turned around. An audit of the ports found no other
such sequence, apart from one latent in the UART, which is now ordered as well.

An independent review of the kernels found one more on 2026-09-30, in the power-aware
kernel under DRA and DR_OTE. Two compare-and-stores that a task makes as it ends keep
their operands and the step they reached in static variables, so that the timer
interrupt can complete a pair the task left half done. GCC kept the step in a register
and wrote only its last value: the disassembly held no store of step 0 or 1, and an
interrupt between the two stores found no pair pending, leaving the simulation of DRA
behind and its choice of speed too low. The variables are now volatile, a compiler
barrier follows each store, and the step moves to the one after the step done, not by
one from what it holds, since a task resuming after the interrupt completed its pair
took it past its end. The host test takes an interrupt at each of those barriers.

The same review found the soft kernel admitting under EDF an optional instance that
could not end in time. With an event-driven task whose server deadline lay far ahead,
`IsTaskSchedulable` subtracted from the work the instance and the mandatory ones need,
where events due later take nothing from it. The instance started, was still in the
ready queue at its next arrival, and was inserted again: the queue looped. The host
test `firmdiscount` reproduces it and fails on the code before.

### What else -O2 could take

What else -O2 could take was checked the same day. The host tests pass at -O2 under the
whole of UndefinedBehaviorSanitizer, with Clang on the Mac, and the CI now runs them so
with GCC. Every inline assembly statement that must keep its place among memory
accesses now declares it: `CLREX`, the `WFI` of the idle task, the `SEV` and `WFE` of
the launch of core 1. The 88 images of every build came out byte for byte the same, so
this too is a guarantee rather than a fix.

The board machine ran `tools/check_order.py` on what its own GCC 16.2 built, while the
CI's compiler was 14.2. Since 2026-09-26 the CI builds with 16.2 and also checks the
order of every variant under 14.2. The bench, now an Arduino UNO Q with no GCC 16, runs
the CI's images.

The code breaks the strict-aliasing rule, and GCC uses it. Built with
`-fno-strict-aliasing`, all 88 images change, among them functions of the kernels,
which access the same memory as `TCB`, `ETCB` and the port's `MinimalTCB`. The one
difference read, in `OSEndTask`, is a value reused instead of read again, correct either
way. No fault has been traced to aliasing. Every firmware is now built with
`-fno-strict-aliasing` anyway, as insurance against what another version of GCC could
draw from the rule the code breaks. On the board that cost 68 bytes, and 3.4 to 3.5 µs
per round on average, 11 to 12 µs at worst, over two runs of each.

Built with `-Wextra -Wnull-dereference -Warray-bounds=2`, no variant gave any of the
warnings the optimiser computes. A comparison of signedness, though, led to the timer
events of the STM32 port. On its 32-bit timers the counter and the comparator were read
through pointers that were not volatile. GCC could therefore reuse the counter read in
the loop and the comparator just written, and take an event whose time had passed for
one to come, 71 minutes later. GCC 16.2 happened to read them again. The eleven accesses
are volatile now, as those of the 16-bit path already were.
