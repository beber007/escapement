# The scheduler on the host

```sh
make -C test/host run
```

The three kernels are compiled as they ship; only the target layer is simulated
(`host_port.c`), and time is a variable. That buys what no board gives: task sets larger
than an example carries, the 2^30 wrap of the kernel clock without waiting eighteen
minutes, and the parts of the kernel no example runs. AddressSanitizer reports a read
past a block, whatever the heap holds next to it. Signed overflow and division by zero
are made errors too, so that they fail on every host: left alone, a division by zero
traps on x86 and yields 0 on arm64.

Nine builds: the hard and the soft kernel under EDF and DM, the power-aware kernel under
EDF and DM with OTE, and under DRA, DR_OTE and DM_SLACK. Each runs `test_scheduler` in
every mode below and `test_ipc` once per kernel; a 10 s alarm reports a kernel that
loops.

| Run | What it checks |
|---|---|
| `test_scheduler` | ten periodic tasks over 200,000 ticks: every task activated as often as its period calls for, no deadline missed, tasks released together run in priority order |
| `wrap` | the clock jumped from event to event over three wraps, an arrival served late just short of each, tasks left in the ready queue across it; deleting any one of the three time shifts of the arrival and ready queues makes it fail — one of them is reached only through that late arrival, and one makes the kernel loop forever, which the alarm reports |
| `events` | event-driven tasks woken by periodic tasks, by themselves and by a buffer slot filling up |
| `busy`, `early`, `slack` | tasks that take time, instances ending at or before their WCET or leaving time to others: the speed the power-aware kernel picks decides whether deadlines hold |
| `expiry` | the time a task left unused runs out while the processor idles; the task set was found by searching random ones for a deadline that a kernel whose slack never runs out misses |
| `reclaim` | that time slows down a task that is not the last of its busy period |
| `firm` | (m,k)-firm tasks under a declared overload of 220 %, soft kernel only |
| `test_ipc` | the FIFO queue past the wrap of its indices and refusing a node when full, both slot buffers through their states, store-conditionals made to fail on purpose |

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
power-aware one in its shipped configuration, one task extension (2026-09-22). What
remains is mostly the paths a preempted operation takes, which one thread cannot reach:
those are for the models of `test/model`. Tasks run on no stack of their own and the
test calls the elected task itself, so the context switch is not tested here — the
defect of the Cortex-M0 context switch that lost the idle task's type showed only once
its assembler ran, under Renode. And the kernel's own code and each change of speed take
no time here: the test checks the policy, not its cost on a processor.
