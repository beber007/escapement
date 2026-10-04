# The scheduler on the host

The three kernels also build for the machine that runs the tests. This page lists what
those tests check, the defects they have caught, and what they cannot see.

```sh
make -C test/host run
make -C test/host run OPT=-O2 SANITIZE=address,undefined BUILD=build-O2
```

The kernels are compiled as they ship. Only the target layer is simulated
(`host_port.c`), and time is a variable. That gives what no board gives: task sets
larger than an example carries, the 2^30 wrap of the kernel clock without waiting
eighteen minutes, and the parts of the kernel no example runs.

AddressSanitizer reports a read past a block, whatever the heap holds next to it.
Signed overflow, a shift out of range of its type and division by zero are made errors
too, so that they fail on every host. Left alone, a division by zero traps on x86 and
yields 0 on arm64. The CI runs the tests a second time at -O2, the level of the
firmware, under the whole of UndefinedBehaviorSanitizer, because the optimiser takes
more from undefined behaviour there.

There are nine builds: the hard and the soft kernel under EDF and DM, the power-aware
kernel under EDF and DM with OTE, and the power-aware kernel under DRA, DR_OTE and
DM_SLACK. Each build runs `test_scheduler` in every mode below, the `firm` modes with the
soft kernel only. `test_ipc` runs once per kernel. A 10 s alarm reports a kernel that
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
| `firmwait` | tasks taking time, the mandatory instances keeping the processor busy across a whole period of an optional one, which is still in the ready queue, never started, when its task arrives again: the kernel takes it out before inserting the next, every mandatory instance runs, none twice, and the ready queue stays whole |
| `firmeventwait` | an optional instance started under EDF, then delayed by an event-driven task declaring its WCET and workload but no share of the processor, signalled as an interrupt handler would at chosen times: the schedulability test must count it, and no deadline is missed |
| `eventrelease` | an event-driven task signalled a workload after its previous signal, its first instance preempted by a periodic task of higher priority: each instance ends within its workload of its signal, the next release counted from the previous one, not from the last time the task was elected |
| `firmeventahead` | soft kernel under DM only: an optional instance tested while an event-driven task of higher priority cannot be released again yet; the test counts its interference from that release, and the last instance that does not fit whole by its WCET. The instance fits by 10 ticks, and ends at 1990 under the worst signals; counted from now, or the remainder whole, it would be dropped |
| `firmeventqueued` | soft kernel: an optional instance tested at its arrival while an event-driven task, signalled again too soon, waits in the arrival queue; the test steps over its smaller control block, which read as a periodic task's is read past its end (AddressSanitizer) |
| `firmwrapmandatory` | soft kernel: an optional instance tested just before the wrap, its deadline beyond it, while another task's next mandatory instance arrives beyond the wrap too; the instance fits by 499 ticks, and is dropped if that arrival is not brought back by 2^30 |
| `simstale` | a task ending early, then arriving behind a task that runs on: under DRA and DR_OTE its previous instance is still in the simulation queue, which the insertion takes out; the queue is checked after each timer event of the timed runs. The path is taken, but a kernel that leaves the previous entry linked is not caught: it drops the entries in between, and a task the simulation lost runs at the fastest speed, which costs energy and no deadline |
| `firmoverload`, `firmoverloadinstances`, `firmoverloadshare`, `firmoverloadsum` | soft kernel: optional instances tested while more than 2^30 ticks of work are declared before their deadline, by single instances, whole instances, or, under EDF, the share of the event-driven tasks at 200 and 128 / 256; each guard of the 32-bit sum is reached, and each taken out shows in UndefinedBehaviorSanitizer but one, which the check after it makes redundant |
| `signals` | three signals of an event before its task runs: it runs twice, the third finding the second kept; an event no task was created for is signalled without effect |
| `notask` | the kernel started with no task created sets itself up and elects the idle task |
| `trace <ticks>` | tasks read on the standard input, one a line: `P wcet period deadline takes` a periodic task, `E takes workload` an event-driven one, `S time task` a signal of its event, and, soft kernel only, `F wcet period deadline takes m k` an (m,k)-firm task; run for that long, prints who ran from when, for how long and at what speed, when each instance ended, and when the timer interrupted, for `tools/differential.py`, which draws schedulable task sets at random and checks every trace against EDF or deadline-monotonic scheduling |
| `createbounds` | tasks the kernel must accept at the edges of what it refuses: a period of one whole turn of 2^30, a deadline of one tick, a deadline past the remainder in a period of a turn or more, a workload of one tick |
| `longperiod` | a task of period 2^30 + 1000, which counts its turns apart, over three wraps: released four times, each on time |
| `twosignals` | two events signalled at the same instant before the timer handler runs: both tasks run, once each |
| `eventspacing` | an event-driven task of workload 300 signalled at 50, 150 and 400: released at once at its first signal, then from the arrival queue at 350 and 650, a workload after each release |
| `minspeed` | a light load with `OSSetMinimalProcessorSpeed`: the power-aware kernel never goes below it, which four of its five builds would otherwise do; two tasks released together with equal deadlines, which EDF* breaks by arrival and address |
| `endinside`, `endinsidebusy` | `early` and `busy` with the soft timer interrupt taken at one compiler barrier of the kernels in two, at random (`HostCompilerBarrierHook`), where a task ending leaves its stores in the order the handler relies on; the interrupt completes first, as on the target, what `FinalizeContextSwitchPreparation` completes. The time does not move there, so who ran when and how fast must be as in the run without those interrupts, which a child process makes first |
| `timewrap`, `timewrapbusy`, `timewrapidle` | `early`, `busy`, and two tasks arriving together after an idle time, started at the phase of the counter where one of the first sixteen instances ends at the last tick before the wrap; the interrupt of the wrap is taken at each of the first four time reads and barriers of that end in turn (`HostTimeReadHook`, `HostCompilerBarrierHook`), where the kernel may hold a time from before the shift. Every check of the timed run must hold. Each run is a child process |
| `test_ipc` | Each operation of the FIFO queue and of the queue between the cores, interrupted at each of its LLs by another, which must complete it or move Tail or Head on for it. Every creation of a queue or buffer refused, not crashing, when memory runs out at each of its allocations. The FIFO queue past the wrap of its indices, started below it; refusing a node when full; a dequeue preempted while the operations preempting it bring its index back to the value it read, as 16 bits did after 65,000 of them. Both slot buffers through their states, and a reader coming in the middle of a write, at the writer's first barrier or at its last with the slot before left unread, over blocks filled with 0xA5 as SRAM is rather than zeros. The queue between the cores of the RP2350 (`Escapement_CoreQueue.c`) in order, full, empty and round its array, its SCs, the one that advances Tail or Head among them, made to fail on purpose. |

Under the power-aware kernel every run also checks the speeds asked for. They must
always be operating points of the RP2040. Where slowing down is possible, some must be
below the fastest as well as at it. With event-driven tasks in the set, or under DRA
when tasks take their WCET, only the fastest is allowed, since a waiting event-driven
task can be woken at any time. On the task set and the wrap, the speed changed over four
thousand times when the power-aware kernel joined the test (2026-09-22). A kernel that
never slows down fails.

## What it has caught

Several of the defects it caught are told in `docs/method.md`. The kernel shipped
scheduling deadline-monotonic while every page said EDF, and the soft and power-aware
headers forced it too. The idle task was read past its block. DRA, DR_OTE and DM_SLACK
did not compile, then showed four defects once built.

Two more concern the soft kernel. Under EDF it computed the workload of an event-driven
task as `(wcet << 8) / aperiodicUtilization`, ignoring the one passed, and the examples
passed 0 for both. That is a division by zero, which gives 0 on a Cortex-M and a crash on
x86. Under DM it shifted at every wrap a deadline it never set, until the value
overflowed. A workload given is now taken as is, a creation with neither is refused, and
the deadline is set for every instance. The sanitizer flags fail on the old code.

### How the host stands in for the target

The atomics of the host keep a reservation, as the target does. An LL sets it and its
SC consumes it. Code a test runs between them (`HostLLHook`), as an interrupt would,
makes the SC fail. `_OSDisableInterrupts` masks that code as it masks an interrupt: a
hook that finds interrupts masked holds its interrupt until they are unmasked
(`HostUnmaskHook`), and so does a soft timer interrupt raised meanwhile. `OSMalloc` can
be given a budget of allocations (`HostMallocBudget`), and can fill its blocks with a
byte instead of zeros (`HostMallocFill`), as SRAM is.

### The audit of the inherited kernel

The audit of 2026-09-25 added the runs from `create` to `firmlong`, and the reader in
the middle of a write. Each failed on the kernel before its fix, with a hang, a
corrupted queue, a missed deadline or a sanitizer report; the findings are listed in
`docs/method.md`, "The line-by-line audit". Two of those fixes were wrong ("Two fixes
that were wrong" there): the reader at the writer's last barrier fails on the first,
under every kernel, and `signalinside`, with a task of higher priority, on the second,
with a segmentation fault under deadline-monotonic scheduling.

### Races closed after the audit

Each run below reached a race or a path no run had taken, on 2026-09-25; the story of
each is in `docs/method.md`, "Races and limits closed after the audit".

- `endinside`: the power-aware kernel left the next task at the speed of a task ending
  between two interrupts. The kernel before the fix fails in each of the five
  power-aware builds, at both loads, but DRA with tasks taking their WCET, where it has
  nothing to reclaim; the kernel after it fails once the handler ignores
  `_OSNoSaveContext`. So that tasks end here as on the target, the test clears
  `_OSNoSaveContext` where the context switch would, and takes each soft timer
  interrupt through what `FinalizeContextSwitchPreparation` does.
- `firmwait`: an optional instance still ready at its next arrival, which instances
  running in no time never leave. The kernel takes it out correctly; with that removal
  taken out, the run hangs.
- `firmeventwait`: under EDF, with no share declared for the event-driven tasks, the
  soft kernel's test of an optional instance left them out. The run hangs on the
  overload guard before the fix.
- `timewrap`: under DM_SLACK, a task ending read the time before its reservation, and
  the slack grew by 2^30 after the wrap. The kernel before the fix misses a deadline
  under `timewrapidle`; the other runs pass on it, their slack going to no task that may
  use it.

`eventrelease` found a defect inherited from ZottaOS (2026-10-04), written for a branch
coverage showed untaken. The soft kernel set an event-driven task's earliest next release
each time it elected the task, not once where the task is released: under DM an instance
signalled 300 ticks after the previous one, its workload, ended 419 ticks after its
signal, where the analysis bounds its response at 210. The kernel before the fix fails
`eventrelease` and `firmeventahead`.

### Faults planted, then mutants

When the test was written (5018fdb, 2026-09-21), 14 of 15 faults planted by hand in the
hard kernel failed a check; `expiry` and `reclaim` were added when a slack that never ran
out, and a DM_SLACK that reclaimed nothing, passed every other run. `tools/mutants.py`
now plants them by the thousand (`docs/method.md`, "Mutants").

Three things of the harness changed on 2026-10-04, each after a mutant of the hard
kernel survived every run:

- `OSMalloc` fills its blocks with 0xA5, as the target's SRAM is not zeros: a field a
  creation leaves unset no longer reads 0.
- `RunAcross` holds a task elected before the wrap over it only when its deadline lies
  beyond the wrap, so that the deadline sits in the ready queue while the kernel shifts
  the times. It held one whenever the wrap was its next interrupt, and a task of a
  period past 2^30, released on time, ran a whole turn late (`longperiod`). Held only
  within the last `LATENCY` ticks, as first corrected, none sat in the ready queue
  across the wrap, and the coverage check found the shift of its deadlines run by no
  test.
- Every task that ends, by `OSEndTask` or `OSSuspendSynchronousTask`, must leave its
  context unsaved (`_OSNoSaveContext`) and ask for a context switch, as the timer
  handler must, which the host's port only counts: a run that sees one without fails.

## Coverage

Coverage is a check (`python3 tools/coverage.py`, in CI since 2026-10-04): every line of
the three kernels and of the queue between the cores that some build compiles must run in
one of them, or say where it stands why it cannot, with `COVERAGE-LINE` or
`COVERAGE-OFF` ... `COVERAGE-ON` in a comment. An excluded line that runs fails as well.
Branches must not fall below `coverage-floor`. The test binaries are read one by one: a
view merged over the nine builds had left out lines of DM_SLACK, two of them never run.
On 2026-10-04 the check found 68 lines no run took, and 86.5 % of the branches taken.
Tests were written for the logic among them, which found the defect of `eventrelease`;
dead code was removed (`Initialize` could not fail, and OTE, DR_OTE and DM_SLACK had a
branch for an empty arrival queue, which the active periodic task never leaves empty);
the soft and power-aware kernels now refuse a WCET past the deadline. 37 lines remain
excluded: the assertions of `DEBUG_MODE`, the helpers of the wait-free queue that find
their work done, and DRA's update interrupted by another. 90.5 % of the branches ran
that day, and 91.15 % on 2026-10-05, after the tests the mutants called for (`method.md`).

Earlier figures, by line only: 90 % of the hard kernel (2026-09-21, up from 24 % before
the runs of events, queue and buffers), 87 % of the soft one, 90 % of the power-aware one
(2026-09-22); over the nine builds merged, 96.9 %, 95.1 % and 96.2 % of the three
kernels and 100 % of the queue between the cores (2026-09-25).

## What it cannot see

What is left of the wait-free queue is an operation that finds its work done while
helping another, which takes two nested interruptions. The host interrupts an operation
only at an LL, one thread at a time. Every interleaving is left to the models of
`test/model`.

Tasks run on no stack of their own, and the test calls the elected task itself, so the
context switch is not tested here. The defect of the Cortex-M0 context switch that lost
the idle task's type showed only once its assembler ran, under Renode. The kernel's own
code and each change of speed take no time here either: the test checks the policy, not
its cost on a processor.
