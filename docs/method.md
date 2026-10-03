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
| Compilation | GitHub Actions on each push to `main` and each pull request, with the developer's GCC 16.2 and, for every variant, the compiled order and the Pico 2 suite, the GCC 14.2 of the stable distributions (since 2026-09-26; 14.2 alone before) | code that does not build, and anything one compiler forgives that the other does not |
| The scheduler alone | the kernel built for the host, with time as a variable, in CI | a kernel that does not run the algorithm it claims, the 2^30 wrap of its clock, which the board reaches only after eighteen minutes, and the parts of the kernel no example exercises: event-driven tasks, FIFO queue, slot buffers |
| Every interleaving | small models of the slot buffers and of the FIFO queue, explored exhaustively in CI (`test/model`) | what no test that runs one path at a time can reach: an interrupt at the one instruction where it matters |
| Replayable execution | Renode, replayed by `renode-test` in CI | a kernel that builds but does not schedule |
| Internal state on hardware | OpenOCD and SWD | an emulator that models the hardware wrongly |
| Independent instrument | frequency counter of a Bus Pirate v4 | everything above at once — it trusts no software from this repository |

The levels are ordered by how much they cost and by how little they assume. The sixth
exists because the fifth still runs through a debugger, and that turned out to matter
(the `TIMER_DBGPAUSE` investigation in `rp2040.md`). Beside these levels, the CI runs
static analysis (cppcheck over each port, GCC's `-fanalyzer`) and checks the compiled
order of the accesses the lock-free code depends on (below).

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
| The kernel schedules by earliest deadline first — the first line of the README, and what sets the project apart | Running the scheduler on the host, where a mirror of the task control block read a pointer where a deadline belonged | Every example shipped was scheduling deadline-monotonic. `EscapementHard.h` set the algorithm itself, with no `#ifndef`, and no configuration file overrode it |

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

## What this changes in the repository

The CI runs the kernel, under Renode and on the host, besides building it. That is how
the `-O2` defect and the scheduling one above were found. A measurement is written with
its date and its deviation (the +28 ppm of the frequency counter, `rp2040.md`). A result
that did not reproduce is said so. A commit message gives its reasoning, wrong turns
included.

## What it does not prove

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
whatever the caller's state. Left for later: the MSIS locked on the LSE feeds PLL1 at
3.998 MHz, under the 4 MHz RM0456 gives as the floor of both the VCO's input and the
booster's clock, on a board without the HSE, which the NUCLEO-U575ZI-Q was then taken
for. The comment on the booster's
clock, read against RM0456 the same day, was wrong: the booster takes the source of
PLL1 before its divider M, 16 MHz from the HSE, not 4, still within its 4 to 16 MHz.
Done on 2026-09-30: PLL1 takes the MSIS of range 2, 16.0017 MHz, whose booster clock
(8.0009) and VCO input (5.3339) stay within their ranges even at the few % the MSI may be
off before it locks again after a wake-up. An independent review of that change found
the same day that it had missed the SRAM's wait state in voltage range 4 above 16 MHz,
and read the datasheet's 1 % as a bound during the lock where it is its end; both
fixed. A Renode test checks what the port writes, and
fails on the code before. The NUCLEO was said to have run it since without error: it
runs on its HSE (the hypotheses above). The path first ran on a chip as `SleepNoHSEU5`
on the UNO Q on 2026-10-02 (`stm32u5.md`).

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
There are nine since the status of a buffer, which the models leave out, is set after
its slot is handed over (2026-09-25). The queue between the cores had no such check
until 2026-09-30: only its source and its model named its seven barriers. The check now
follows the queue through its LL and SC, which are calls, and each barrier removed from
the source is caught, each by the pair of accesses it stands between. An independent
review found two holes in it the same day, both closed: a path went on past a return GCC
writes as `ldmia.w sp!, {..., pc}`, which the check took for an ordinary load, and the
pairs let the E5 of the next round stand for the E10 that must follow the LL of the
place, so that a queue without E10, or with it before the LL, passed. Changes to the
algorithm that keep its order, as a test turned around at E11, are not the check's to
find.

The check cannot see a reordering by the processor that the architecture does not
allow, which is the models' premise. Nor can it see code the compiler might emit for
another version or optimisation level until it runs there. Without the barriers' memory
clobber, GCC 16.2 happened to keep the same order at -O2 (2026-09-25). The clobber is a
guarantee, not a fix to an observed fault.

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
