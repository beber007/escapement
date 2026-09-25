# Verifying AI-assisted development

This project is written with an AI assistant. That changes what the repository
has to prove: not that code was produced, but that what was produced is correct.

The working rule is one line: **the AI proposes, the instrument decides.**
Nothing here is considered established because it was asserted confidently — by
the assistant or by anyone else. It is established when something outside the
claim confirms it.

## Six independent levels

| Level | Means | Catches |
|---|---|---|
| Compilation | GitHub Actions on every push, with a toolchain other than the developer's | code that does not build, and anything the local compiler forgives that the one in CI does not |
| The scheduler alone | the kernel built for the host, with time as a variable, in CI | a kernel that does not run the algorithm it claims, the 2^30 wrap of its clock, which the board reaches only after eighteen minutes, and the parts of the kernel no example exercises: event-driven tasks, FIFO queue, slot buffers |
| Every interleaving | small models of the slot buffers and of the FIFO queue, explored exhaustively in CI (`test/model`) | what no test that runs one path at a time can reach: an interrupt at the one instruction where it matters |
| Replayable execution | Renode, replayed by `renode-test` in CI | a kernel that builds but does not schedule |
| Internal state on hardware | OpenOCD and SWD | an emulator that models the hardware wrongly |
| Independent instrument | frequency counter of a Bus Pirate v4 | everything above at once — it trusts no software from this repository |

The levels are ordered by how much they cost and by how little they assume. The
sixth exists because the fifth still runs through a debugger, which turned out
to matter: see the `TIMER_DBGPAUSE` investigation in `rp2040.md`.

## Hypotheses that were wrong

Kept on purpose. A record that shows only the conclusions teaches nothing about
how they were reached, and an assistant that is confidently wrong is a fact one
has to design around.

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
| The FIFO queue of the kernels is wait-free on one core, its announced operation completed by whoever preempts it | An exhaustive model of the queue (`test/model/fifo.py`), run by run checked for linearizability | A dequeue that signals an event and finds the queue empty leaves the signal, then gets preempted before marking itself done. The first helper to see the signal left without marking it either; once a waiting task had taken the signal, a second helper, preempted since before, found the slot empty and put a signal back. One event, two tasks woken. The dequeue now marks itself done when it finds its signal |
| A full FIFO queue refusing a node would corrupt its tail, having no capacity test like the one in Evéquoz's paper — stated by the assistant from reading the code | The host test, strengthened to check the queue after the refusal | The enqueue only ever writes into an empty slot, a guard the reading had missed; the queue refuses cleanly |
| DRA, DR_OTE and DM_SLACK compile, and only running them is left — the roadmap, from reading the build | Building the power-aware kernel with each of them, on the target and on the host (2026-09-24) | None compiled. DRA and DR_OTE called an interrupt intrinsic of the MSP430 and gave the task control block a third link where the context switch reads its fields; DM_SLACK read a clock variable that no longer exists. Once built, the host test found four defects: the speed computation multiplied a time by a ratio and overflowed past 21 s, which OTE shares; DRA shifted a time that nothing updates without event-driven tasks and overflowed at the third wraparound; DRA looked for the running task in its simulation queue and read through a null link when the task had outlived its WCET there; and DM_SLACK gave the time a task left unused to tasks of higher priority, which never counted it in their response time — a hand-built task set shows the first task missing its deadline, and the time now goes to tasks of lower priority, as the kernel's own comment said |
| The kernel schedules by earliest deadline first — the first line of the README, and the reason the project is interesting | Running the scheduler on the host, where a mirror of the task control block read a pointer where a deadline belonged | Every example shipped was scheduling deadline-monotonic. `EscapementHard.h` set the algorithm itself, with no `#ifndef`, and no configuration file overrode it |

One more, of a different kind: an early rebranding pass deleted a comment
terminator in 32 files, and the first attempt to repair them corrupted 57
healthy ones. It was undone with `git checkout` and redone from an exact list.
Working in a repository where every step is committed is what made that cheap.

## What `-O2` exposed

A test that said yes for the wrong reason, recorded in full.

Adding `-O2` to the Makefiles built cleanly, passed both Renode suites locally
three times over, and the board reported the per-activation cost falling from
7.0 to 3.2 µs. Everything said go. The CI then failed the first assertion of the
stm32f4 suite, and widening the tester window changed nothing — because the
window was not the problem.

Downloading the ELF the CI had built and running it under the *local* emulator
reproduced the failure at once: the binary is at fault, not the runner. Tracing
its outputs showed the kernel raising one output at 19 µs and then stopping
dead. The program counter sat in `HardFaultException`, and `CFSR` read
`0x00040000` — `INVPC`, an invalid exception return. The context switch, which
builds its own exception frame by hand, does something the architecture only
tolerates as long as the compiler leaves the surrounding code alone.

The defect itself is a missing barrier. Pending PendSV does not take it: the
write has to reach the NVIC and the processor has to observe the pending state
before it runs what follows. `OSEndTask` depends on never returning — a task
starts with `0xFFFFFFF9` in `LR`, an `EXC_RETURN` value, so returning from
thread mode faults. At `-O0` the epilogue was long enough for the exception to
arrive first; optimised, `bx lr` sits one instruction after the store. `dsb`
and `isb` close it, the CI agrees, and the build is at `-O2`.

Two things are worth keeping from this. A local build passing is a statement about one
version of one compiler — the toolchain in the CI, not the version on the developer's
machine, was the only thing between this defect and a repository claiming to be measured
and verified. And the first diagnosis was wrong: the registers being written through
non-volatile pointers, it concluded that the compiler had dropped the store, and the
disassembly of the CI binary showed it there all along. The `volatile` was added anyway,
because the code had no right to that store being kept, but it was not the bug.

## The repository was not scheduling the way it said

It took until the scheduler ran on a host to find it.

The README leads with earliest-deadline-first scheduling, and it is what makes this kernel
worth looking at next to a fixed-priority one. `EscapementHard.h` nevertheless selected
deadline-monotonic itself, in a plain `#define` with no `#ifndef` around it, so an
application could not choose: no example overrode it, and none of them could have.

Nothing exposed it. The examples scheduled correctly, the emulation tests passed, the
periods were right to the part per hundred thousand on a frequency counter — all of it is
just as true under deadline-monotonic. It surfaced only because a host build read the
deadline field of the elected task and got a pointer back: under deadline-monotonic the
kernel inserts a priority byte and drops the two deadline fields, which moves everything
after them.

The algorithm is now chosen in `Escapement_Config.h`, the header only supplies the
fallback, and every example selects earliest-deadline-first. All three emulation suites
still pass, and the host test additionally checks that no deadline is missed — a check
that could not even be written while the field it reads was not there.

## What this changes in the repository

- Every example is built in CI, and the kernel is **run** there, not merely
  compiled.
- Hardware measurements are recorded with their deviation and their
  interpretation, not as bare numbers.
- The documentation states what has *not* been verified as clearly as what has.
- Commit messages carry the reasoning, including the reasoning that turned out
  to be mistaken.

## What it does not prove

The new code — the RP2040 port and the Cortex-M layer — has been audited line by
line, which turned up four defects: a race between `OSEnqueueUART` and its
interrupt, a zero-sized transmission that emptied 64 KB onto the port, missing
header dependencies, and stale comments. The kernel inherited from 2016 went through
the same audit on 2026-09-25, each finding reproduced by a host test that failed before
its fix (`test/host/README.md`): a slot buffer that lost a slot to a reader preempting
its writer, and read fields `OSMalloc` had not cleared; an event-driven task ending at
its deadline inserted twice in the ready queue; an event-driven task waiting past a
wrap sorted before periodic tasks due earlier; deadlines left unshifted at the wrap in
the soft kernel's optional instances, and in the power-aware kernel under
deadline-monotonic scheduling and in DRA's simulation queue; a DM_SLACK slack given
twice; a task past its WCET slowed to the slowest speed; overflows in the soft kernel's
test of optional instances; and creations the kernel cannot count with, now refused.
In the compiled code of the Cortex-M0+, three of the five emulated load-linked read the
value before setting the reservation, so that an interrupt in between went unseen: a
compiler barrier now keeps the order.

Two of those fixes were wrong, and the endurance test (`SoakPico`, `tools/soak.sh`)
showed it the same day, once an interrupt and phases of high load had joined it. A
slot buffer's status, set after its slot rather than before, let a reader take the new
slot early, the status still saying unread from the slot before, and again once the
writer had said it unread: 37 slots read twice in 79 s on the board, none torn. No
order of a status apart from the slot is right; each slot now carries its number, and a
reader taking each slot once compares it with the last it took. And an event-driven task
signaled while it suspended itself was assumed to be the task the timer handler had
interrupted, which it finished in its place: preempted instead by a task of higher
priority that signaled it, it had the context of that task discarded, and the soft
kernel hung under Renode, the host test segfaulted under deadline-monotonic
scheduling. Interrupts are now masked from the enqueue of the suspending task to its
leaving the ready queue, and the handler never sees such a task. Both have a host test
that fails on the code before (`test/host/README.md`). **Left open** are races the host cannot reach and
limits of the design: indices of the wait-free queue that come back to the same value after 65,000 operations
during one preemption. Six points once on that list are closed (2026-09-25). The counter
wrapping while the timer handler runs, once it has found no overflow, is reached on the
host by a hook in that window (`wrapinside`): the handler, reading a time from after the
wrap with its arrivals not yet shifted, releases nothing, and serves them once it finds
the flag of the overflow raised meanwhile, which it tests again before it returns; the
test fails with that test taken out. In the power-aware kernel, a task ending between
two interrupts left the next at its speed: the flag that told the handler to set that
speed, raised before the task became a zombie, was cleared by an interrupt that found
the task still running, and the second, once the task was a zombie, took the next for
one that had been running. The handler now reads `_OSNoSaveContext`, which only the
context switch clears, and the host takes the timer interrupt at the kernels' compiler
barriers (`endinside`); with a barrier in the old window, the kernel before fails it
(`test/host/README.md`). Under DM_SLACK, a task ending read the time before its
reservation: an interrupt that shifted the clock at a wrap in between left it a time of
before the wrap against a last update after it, and the slack, left for the task ending
after an idle time, grew by the 2^30 of the shift at the next arrival. The time is now
read inside the reservation; the host runs a task set with a task ending at the last
tick before the wrap, the interrupt taken at each of its time reads and barriers
(`timewrap`), and the kernel before misses a deadline there. An optional instance still
ready at its next arrival, never started, is reached on the host by tasks that take
time, the mandatory instances busy across its period (`firmwait`): the kernel takes it
out of the ready queue as it should, under both algorithms. One already started is
still an overload the kernel stops on (`DEBUG_MODE`), by design: the schedulability
test lets it start only if it ends in time, so only a task running past its declared
WCET brings it, or work the test leaves out. Under EDF the test left out the event-
driven tasks when no share of the processor was declared for them, although each
declared its WCET and workload: an optional instance started, an event delayed it, and
the kernel stopped on that guard at its next arrival (`firmeventwait`). It now reserves
at least the share each event-driven task takes, its WCET over its workload. And strict aliasing, which
GCC does exploit here, is turned off (below).

The execution tests exercise three or four tasks, the host test ten. Nothing here
establishes how the scheduler behaves with thirty. The 2³⁰ wrap of its clock, about
eighteen minutes away on hardware, is crossed on the host and under Renode, never on
a board.

The RP2350 port has not run on a board, and nothing of it has been audited line by
line; its clocks are acknowledged blindly by the emulated platform (`emulation.md`).
Between the cores, the models cover the order of accesses the architecture allows;
that the compiled code keeps it, `tools/check_order.py` checks in the CI on every build
of the Pico and the Pico 2, on every path through the buffers' functions, and
`tools/check_order_mutants.sh` shows it failing without any one of the barriers — nine
since the status of a buffer, which the models leave out, is set after its slot is
handed over (2026-09-25). What
the check cannot see is a reordering by the processor that the architecture does not
allow — the models' premise — nor code the compiler might emit for another version or
level of optimisation until it runs there. Without the barriers' memory clobber, GCC
16.2 happened to keep the same order at -O2 (2026-09-25): the clobber is a guarantee,
not a fix to an observed fault.

On one core, the same holds of the stores a task makes that the timer interrupt may
find half done: the processor keeps their order, the compiler need not, and did. Besides
`OSEndTask` (above), the soft kernel's `ScheduleNextTask` promotes an optional instance
by marking it `STATE_ACTIVATE`, moving it to the head of the ready queue in three steps,
and setting it `STATE_INIT`; the interrupt completes a promotion it finds marked. GCC
saw the mark overwritten and dropped it, on the Cortex-M0+, the M33 and the M4 alike:
a promotion interrupted there left the task at the head and in the list of optional
instances at once. No test had shown it. Compiler barriers now order these stores, those of the
two drops in the same function, and the clearing of `SetActiveTaskRemainingTime` in the
power-aware kernel (a flag since removed, above), and `tools/check_order.py` checks the compiled order of each on every
path, on every build of the Pico, the Pico 2 and the F4 (2026-09-25). Run on the kernels
of the day before, it reports exactly the faults found by hand: `OSEndTask` on the M33
and the M4, the promotion on all three; the mutants script shows each rule failing on a
source whose order is turned around. A port audit found no other such sequence, one
latent in the UART, now ordered as well.

What else -O2 could take was checked the same day. The host tests pass at -O2 under the
whole of UndefinedBehaviorSanitizer, with Clang on the Mac, and the CI now runs them so
with GCC. Every inline assembly statement that must keep its place among memory
accesses declares it (`CLREX`, the `WFI` of the idle task, the `SEV` and `WFE` of the
launch of core 1): the 88 images of every build came out byte for byte the same, a
guarantee rather than a fix. The board machine runs `tools/check_order.py` on what its
own GCC 16.2 builds, the CI's being 14.2. Strict aliasing is used: built with
`-fno-strict-aliasing`, all 88 images change, in functions of the kernels among others,
which access the same memory as `TCB`, `ETCB` and the port's `MinimalTCB`. The one
difference read, in `OSEndTask`, is a value reused instead of read again, correct either
way; no fault has been traced to aliasing. Every firmware is now built with
`-fno-strict-aliasing`, as insurance against what another version of GCC could draw
from the rule the code breaks: on the board it cost 68 bytes and 3.4 to 3.5 µs per
round on average, 11 to 12 µs at worst, over two runs of each. Built with `-Wextra -Wnull-dereference -Warray-bounds=2`, every
variant gave no warning of those the optimiser computes, but a comparison of signedness
led to the timer events of the STM32 port: on its 32-bit timers the counter and the
comparator were read through pointers that were not volatile, so that GCC could reuse
the counter read in the loop and the comparator just written, and take an event whose
time had passed for one to come, 71 minutes later. GCC 16.2 happened to read them again;
the eleven accesses are volatile now, as the 16-bit path's already were.
