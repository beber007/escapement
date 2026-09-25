# The scheduler on the host

```sh
make -C test/host run
make -C test/host run OPT=-O2 SANITIZE=address,undefined BUILD=build-O2
```

The three kernels are compiled as they ship; only the target layer is simulated
(`host_port.c`), and time is a variable. That buys what no board gives: task sets larger
than an example carries, the 2^30 wrap of the kernel clock without waiting eighteen
minutes, and the parts of the kernel no example runs. AddressSanitizer reports a read
past a block, whatever the heap holds next to it. Signed overflow, a shift that leaves
the type and division by zero are made errors too, so that they fail on every host: left
alone, a division by zero traps on x86 and yields 0 on arm64. The CI runs the tests a
second time at -O2, the level of the firmware, under the whole of
UndefinedBehaviorSanitizer: the optimiser takes more from undefined behaviour there.

Nine builds: the hard and the soft kernel under EDF and DM, the power-aware kernel under
EDF and DM with OTE, and under DRA, DR_OTE and DM_SLACK. Each runs `test_scheduler` in
every mode below and `test_ipc` once per kernel; a 10 s alarm reports a kernel that
loops.

| Run | What it checks |
|---|---|
| `test_scheduler` | ten periodic tasks over 200,000 ticks: every task activated as often as its period calls for, no deadline missed, tasks released together run in priority order |
| `create` | what the kernel refuses to create: a period of 0 or of 65535 turns of 2^30, a remainder outside [0, 2^30), a deadline of 0, past the period or of 2^30, an event-driven task without its event or with a workload outside [1, 2^30), more tasks than a byte counts, and for the soft kernel m of 0 or above k |
| `priority` | an event-driven task created before periodic tasks of shorter deadline runs after them |
| `wrap` | the clock jumped from event to event over three wraps, an arrival served late just short of each, tasks left in the ready queue across it; deleting any one of the three time shifts of the arrival and ready queues makes it fail — one of them is reached only through that late arrival, and one makes the kernel loop forever, which the alarm reports |
| `wrapsim` | the same, an instance ending just before a wrap: under DRA and DR_OTE its entry in the simulation queue crosses it, and its deadline must shift too |
| `wrapinside` | the same, the counter wrapping inside the timer handler once it has found no overflow and before it reads the time (`HostOverflowCheckHook`): the arrival due just before the wrap must still be served, within its deadline, at each of three wraps |
| `wrapevents` | the same, an event-driven task signalling itself and waiting in the arrival queue beyond each wrap, where periodic tasks count turns of 2^30 apart |
| `events` | event-driven tasks woken by periodic tasks, by themselves and by a buffer slot filling up |
| `suspend` | an event-driven task ends exactly at its deadline with a signal pending, the soft timer interrupt taken at once inside `OSSuspendSynchronousTask`, the tasks it elects running on top of it |
| `signalinside` | an interrupt at each of the LLs of `OSSuspendSynchronousTask` in turn signals the task suspending itself, or wakes a task of higher priority that preempts it and signals it; interrupts masked hold it until they are unmasked, as on the target |
| `lull` | a task of 400 s wakes an event-driven task, with nothing in between: the reclaiming policies account for the aperiodic bandwidth over the whole interval at once, which overflowed 32 bits until 2026-09-25 |
| `busy`, `early`, `slack` | tasks that take time, instances ending at or before their WCET or leaving time to others: the speed the power-aware kernel picks decides whether deadlines hold |
| `expiry` | the time a task left unused runs out while the processor idles; the task set was found by searching random ones for a deadline that a kernel whose slack never runs out misses |
| `reclaim` | that time slows down a task that is not the last of its busy period |
| `reuse` | a task slowed down on that time is preempted by one that may use it too, under DM_SLACK: what the first used is gone |
| `overrun` | a task takes six times its WCET: past it, the power-aware kernel runs it at the fastest speed |
| `firm` | (m,k)-firm tasks under a declared overload of 220 %, soft kernel only |
| `firmwrap`, `firmlong` | optional instances across the wrap, and one of 2^23 ticks, whose schedulability test left 32 bits |
| `firmevents` | (m,k)-firm tasks beside an event-driven task, whose workload the soft kernel computes under EDF; each task reads its place in its pattern (`OSGetTaskInstance`) |
| `minspeed` | a light load with `OSSetMinimalProcessorSpeed`: the power-aware kernel never goes below it, which four of its five policies would otherwise do; two tasks released together with equal deadlines, which EDF* breaks by arrival and address |
| `endinside`, `endinsidebusy` | `early` and `busy` with the soft timer interrupt taken at one compiler barrier of the kernels in two, at random (`HostCompilerBarrierHook`), where a task ending leaves its stores in the order the handler relies on; the interrupt completes first, as on the target, what `FinalizeContextSwitchPreparation` completes. The time does not move there, so who ran when and how fast must be as in the run without those interrupts, which a child process makes first |
| `timewrap`, `timewrapbusy`, `timewrapidle` | `early`, `busy`, and two tasks arriving together after an idle time, started at the phase of the counter where one of the first sixteen instances ends at the last tick before the wrap; the interrupt of the wrap is taken at each of the first four time reads and barriers of that end in turn (`HostTimeReadHook`, `HostCompilerBarrierHook`), where the kernel may hold a time from before the shift. Every check of the timed run must hold. Each run is a child process |
| `test_ipc` | each operation of the FIFO queue, and of the queue between the cores, interrupted at each of its LLs by another, which must complete it or move Tail or Head on for it; every creation of a queue or buffer refused, not crashing, when memory runs out at each of its allocations; the FIFO queue past the wrap of its indices and refusing a node when full, both slot buffers through their states and a reader coming in the middle of a write, at the writer's first barrier or at its last with the slot before left unread, over blocks filled with 0xA5 as SRAM is rather than zeros, the queue between the cores of the RP2350 (`Escapement_CoreQueue.c`) in order, full, empty and round its array, its SCs made to fail — the one that advances Tail or Head among them —, store-conditionals made to fail on purpose |

Under the power-aware kernel every run also checks the speeds asked for: always one of
the operating points of the RP2040; where slowing down is possible, some below the
fastest as well as at it; with event-driven tasks in the set, or under DRA when tasks
take their WCET, the fastest only, since a waiting event-driven task can be woken at any
time. On the task set and the wrap the speed changed over four thousand times when the
power-aware kernel joined the test (2026-09-22). A kernel that never slows down fails.

## What it has caught

The kernel shipped scheduling deadline-monotonic while every page said EDF, and the
soft and power-aware headers forced it too; the idle task was read past its block; DRA,
DR_OTE and DM_SLACK did not compile, then showed four defects once built. Those stories
are in `docs/method.md`. Two more, of the soft kernel: under EDF it computed the
workload of an event-driven task as `(wcet << 8) / aperiodicUtilization`, ignoring the
one passed, and the examples passed 0 for both — a division by zero, 0 on a Cortex-M
and a crash on x86; under DM it shifted at every wrap a deadline it never set, until
the value overflowed. A workload given is now taken as is, a creation with neither is
refused, the deadline is set for every instance, and the sanitizer flags fail on the
old code.

The atomics of the host keep a reservation, as the target does: an LL sets it, its SC
consumes it, and code a test runs between them (`HostLLHook`), as an interrupt would,
makes the SC fail. `_OSDisableInterrupts` masks that code as it masks an interrupt: a
hook that finds interrupts masked holds its interrupt until they are unmasked
(`HostUnmaskHook`), and so is a soft timer interrupt raised meanwhile. `OSMalloc` can be given a budget of allocations (`HostMallocBudget`)
and fill its blocks with a byte instead of zeros (`HostMallocFill`), as SRAM is.

The audit of the inherited kernel (2026-09-25) added the runs `create` to `firmlong`
and the reader in the middle of a write; each failed on the kernel before its fix — a
hang, a corrupted queue, a missed deadline or a sanitizer report. A slot buffer said a
slot was new before handing it over, so a reader preempting the writer took the slot it
had read, and the new one was lost; an event-driven task ending at its deadline was
inserted in the ready queue while still in it; an event-driven task waiting beyond a
wrap sorted before periodic tasks due earlier; a DM_SLACK slack was given twice; a task
past its WCET resumed at the slowest speed. The sanitizer now also checks shifts.

Two of those fixes were wrong, which the endurance test found on the board and under
Renode the same day (`docs/method.md`): the status of a slot buffer set after the slot
let a reader take the same slot twice, and an event-driven task preempted while it
suspended itself had the context of the task preempting it discarded. The reader at the
writer's last barrier and `signalinside`, with a task of higher priority, fail on them;
the first under every kernel, the second with a segmentation fault under
deadline-monotonic scheduling.

`endinside` closed a race the audit had left open (2026-09-25). The power-aware kernel
raised a flag of its own before a task ending became a zombie, for the handler to set
the speed of the next task, and the handler cleared it: an interrupt that found the task
still running cleared it, and a second one, once the task was a zombie, took the next
task for one that had been running, charged it the time of the task ending and left it
at that task's speed. The handler now reads `_OSNoSaveContext` instead, which only the
context switch clears. With a barrier where the flag was set and the zombie not yet
marked, the kernel before the fix fails in each of the five power-aware builds, at both
loads but for DRA with tasks taking their WCET, where it has nothing to reclaim; the
kernel after it passes so, and fails once the handler ignores `_OSNoSaveContext`. So that tasks here are ended
the way the target ends them, the test now also clears `_OSNoSaveContext` where the
context switch would, and takes each soft timer interrupt through what
`FinalizeContextSwitchPreparation` does.

`timewrap` closed another (2026-09-25). Under DM_SLACK a task ending read the time
before taking the reservation that makes its slack one unit: the wrap shifted the time
of the last update in between, the task stored a time from before it, and at the next
arrival after an idle time the slack grew by 2^30. The time is now read inside the
reservation. The kernel before misses a deadline under `timewrapidle`, where a task of
lower priority than the one ending is slowed down on that slack; the others pass on it,
the slack going there to no task that may use it.

Faults were also planted in the kernels by hand, to see the test fail. When it was
written (5018fdb, 2026-09-21), 14 of 15 faults planted in the hard kernel failed a
check; the one that did not, event-driven deadlines no longer following one another, has
no effect when tasks run in zero time, and the runs where tasks take time have no
event-driven task. In the soft kernel, ignoring the interference of other tasks in the
schedulability test of optional instances is not caught, for the same reason. `expiry`
and `reclaim` were added when a slack that never ran out, and a DM_SLACK that reclaimed
nothing, passed every other run.

## What it cannot see

Line coverage, measured when each kernel joined the test — the Makefile has no target
for it — was 90 % of the hard kernel (2026-09-21, up from 24 % before the runs of
events, queue and buffers), 87 % of the soft one (2026-09-21) and 90 % of the
power-aware one in its shipped configuration, one task extension (2026-09-22). Over the
nine builds together, with clang's `--coverage`, it was 92 %, 89 % and 92.5 %, and 85.5 %
of the queue between the cores, before the preempted operations, the allocations that
fail, `firmevents` and `minspeed`; 96.9 %, 95.1 %, 96.2 % and 100 % after (2026-09-25).
What is left of the wait-free queue is an operation that finds its work done while
helping another, which takes two interruptions nested. The host interrupts an operation
only at an LL, one thread at a time: every interleaving is for the models of
`test/model`. Tasks run on no stack of their own and the
test calls the elected task itself, so the context switch is not tested here — the
defect of the Cortex-M0 context switch that lost the idle task's type showed only once
its assembler ran, under Renode. And the kernel's own code and each change of speed take
no time here: the test checks the policy, not its cost on a processor.
