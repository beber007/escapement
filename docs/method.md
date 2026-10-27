# Verifying AI-assisted development

This project is written with an AI assistant. So the repository must show that what the
assistant produced is correct. This page lists the levels of checks. Then it records the
hypotheses that proved wrong and the defects found, each with its date.

The rule is: **the AI proposes, the instrument decides.** Nothing here counts as
established because someone states it with confidence, whether the assistant or anyone
else. It is established once something outside the claim confirms it.

## Six independent levels

| Level | Means | Catches |
|---|---|---|
| Compilation | GitHub Actions, on each push to `main` and each pull request: GCC 16.2, GCC 14.2 (since 2026-09-26) and clang | code that does not build; code that one compiler accepts and another does not |
| The scheduler alone | the kernel built for the host, with time as a variable, in CI | a kernel that does not run the algorithm it claims; the wrap of its clock at 2^30, which the board reaches only after eighteen minutes; the parts of the kernel that no example uses: event-driven tasks, the FIFO queue, the slot buffers |
| Every interleaving | small models of the slot buffers and of the FIFO queue, explored in full in CI (`test/model`) | what a test that runs one path at a time cannot reach, such as an interrupt at the one instruction where it matters |
| Replayable execution | Renode, replayed by `renode-test` in CI | a kernel that builds but does not schedule |
| Internal state on hardware | OpenOCD and SWD | an emulator that models the hardware wrongly |
| Independent instrument | frequency counter of a Bus Pirate v4 | all of the above at once: it trusts no software from this repository |

Each level costs more than the one before, and assumes less. The sixth exists because
the fifth still goes through a debugger, and that turned out to matter: see the
`TIMER_DBGPAUSE` investigation in `rp2040.md`. The CI also runs these checks:

- static analysis: cppcheck on each port, GCC's `-fanalyzer` and clang
  (`tools/clang_check.sh`). Since 2026-10-13, CodeQL also reads what clang compiles
  there (`codeql.yml`);
- the compiled order of the memory accesses that the lock-free code depends on (see
  below);
- coverage, since 2026-10-04 (`tools/coverage.py`). A commit fails if the host test
  leaves a line of a kernel unrun, unless the line is excluded with its reason;
- a differential test, since 2026-10-04 (`tools/differential.py`). The kernels run
  random task sets on the host. Each trace is checked against the algorithm its build
  claims:
  - at every instant, the task that runs is a valid choice under EDF or under
    deadline-monotonic scheduling;
  - the processor never idles while work waits;
  - every instance ends once its work is done, and by its deadline;
  - under EDF, each response stays within the bound of Spuri's response time analysis
    (`tools/response_times.py`).

Kernels made wrong on purpose fail the differential test within the first few task sets
of each build. They are: an order reversed (EDF's or DM's), arrivals sorted the wrong
way, a speed one step too low, and always the slowest speed.

## What this changes in the repository

The CI does not only build the kernel. It runs it too, under Renode and on the host.
That is how the `-O2` defect and the scheduling defect below were found. A measurement
is written with its date and its deviation, such as the +28 ppm of the frequency counter
(`rp2040.md`). When a result did not reproduce, the docs say so. A commit message gives
its reasoning, wrong turns included.

## Mutants: how strong the tests are

Coverage says that a line runs. It does not say that a test would notice if the line
were wrong. `tools/mutants.py` puts one small fault at a time into a kernel's source: a
comparison turned round, `+` for `-`, `&&` for `||`, 0 for 1, or a one-line statement
left out. Then it runs the host test and `tools/differential.py` on the builds of that
kernel. A mutant is killed when a test fails. It survives when no test fails.

`tools/mutants.py` leaves some lines out: those that no host build compiles, such as the code of a
variant the host does not build. A mutant there changes nothing that the host runs.

The last full run was on c0ff963, the candidate for release 0.1, on 2026-10-11. It used
300 task sets per build:

| Kernel | Mutants killed | Of those not declared equivalent |
|---|---:|---:|
| Hard | 87.6 % (424 of 484) | 97.0 % of 437 |
| Soft | 84.0 % (609 of 725) | 96.7 % of 630 |
| Power-aware | 83.0 % (664 of 800) | 96.2 % of 690 |

Every survivor was read. Each one then had one of four outcomes:

- killed by a test;
- declared equivalent, with its reason, in `test/host/equivalent-mutants.jsonl`;
- caught on the ARM builds by `tools/check_order.py`;
- written down as a behaviour that no test reads.

Since then, the mutants of the kernel lines a pull request changes are run before it is
merged (`tools/mutants.py --only`), as `CLAUDE.md` asks. The section "The mutant
campaigns, by date", at the end of this page, tells how the scores got there.

## What the README claims, and what checks it

The mutants taught a lesson: a claim that no test reads is not checked. On 2026-10-04,
this lesson was applied to the README. Each claim it makes about behaviour was set
against what reads it: a test, a model, the board or an instrument.

| Claim | What reads it | Found |
|---|---|---|
| Scheduling by EDF or DM | the host test's view of the task control block; `tools/differential.py`, which checks each trace against the algorithm of its build | holds |
| No periodic tick: the timer interrupts at a release only | nothing on the host. On the U5, in three hours, the idle task entered Stop 2 454,487 times; a tick would have cut those sleeps short | to add: the trace's timer interrupts set against the releases. Done on 2026-10-05: `tools/differential.py` reads every interrupt of the comparator. A host port made to interrupt every 500 ticks fails its first task set |
| The power-aware kernel slows down only as far as every deadline holds | `tools/differential.py` under its five builds, with kernels made to run too slow; the host test's speeds | holds |
| It slows down as far as DRA, OTE and DM_SLACK compute | nothing until 2026-10-05: a kernel faster than its policy still kept every deadline. Since then, `tools/speed_reference.py`, written from the ZottaOS manual, checks each speed in the traces of `tools/differential.py`. Since 2026-10-06 it also checks the speeds of DRA and DR_OTE with event-driven tasks | **not quite**: a release that did not preempt reclaimed nothing, and DM_SLACK left out its slack for a task alone. Both were corrected the same day (80cc6dc). DR_OTE left DRA out when an event-driven task could be released at once; corrected on 2026-10-06 |
| Overload drops chosen (m,k)-firm instances, not deadlines at random | the host test's `firm` runs, `firmoverload*` | holds |
| The queues take no lock | `docs/architecture.md` | **false since 2026-09-25**: `OSSuspendSynchronousTask` masks interrupts around the enqueue of a task on its event. The README now says so |
| The queue between the cores went from fifteen barriers to six | the model `fifo_mp.py` | **stale**: seven since 2026-09-29 |
| The models found four bugs | `docs/method.md`, "Hypotheses that were wrong" | **stale**: six, two of them in the queue between the cores |
| Two boards checked at each commit | `tools/board_ci.md` | **stale**: three, the Pico 2 since 2026-09-28 |
| 87 to 90 % of each kernel's lines run | `tools/coverage.py` | **stale**: every line runs or is excluded with its reason; 90.5 % of the branches |
| How much current Stop 2 saves is not measured | `docs/stm32u5.md` | **stale**: measured on 2026-10-03 with a PPK2 |
| Whether DVFS saves energy is not settled | issues #23 and #5 | **stale**: settled against DVFS on the U5; some 10 % at best on the RP2350. Open on the RP2040, then settled on 2026-10-10: up to some 5 % in WFI, nothing in SLEEP |
| The program shown is a whole program for the Pico | since 2026-10-04, the Pico's Makefile builds it from the page, and the Renode suite checks that its LED blinks at 1 Hz | **false**: it never routed GPIO 25 to the SIO and never made it an output. On the chip, the LED would not have lit. The page now shows the whole program |
| Responses stay under the bound of the analysis | `tools/response_times.py` on the traces in `docs/data` | holds, and under EDF too since 2026-10-04 |
| How it schedules: EDF keeps (50, 25) and (80, 35), DM leaves the second unfinished at 80; the (m,k)-firm set at 110 % keeps every deadline, optional instances dropped | since 2026-10-18, `tools/host_figures.py --check` in the CI. It runs the kernel on the host, checks each statement on the trace, and compares the figures with what the runs draw | holds |
| The figures: 4,716 bytes, 3.2 µs, +28 ppm, 108,000 activations | each one dated, with its source | measurements of a day, not claims about every build |

Six claims had gone stale as the work moved on. Two were false. Two were read by
nothing. The README was corrected the same day. Since then, its program is built and run
from the page. The check of the tick was added on 2026-10-05.

The claims were checked again on 2026-10-10, before the first release (roadmap, task
8). Every claim about behaviour holds on that code:

- the speeds match the policies at 20,000 task sets under DRA and DR_OTE, once the
  reference served each signal in a pass of its own (task 7);
- the bench read the round on 2e30970: 3.5 µs, and 12 µs at worst.

Some items were stale:

- the STM32U3's row said "Renode only", but the board had been on the bench since
  2026-10-09;
- the NUCLEO-U575ZI-Q's row did not say that it runs the soft kernel's endurance test,
  and from its flash;
- the README counted three ports, where there are four;
- it gave 91.15 % of the branches, where there are 91.78 %;
- the table lacked the STM32U3's page.

All were corrected the same day (6a4f138). That commit also made `api.md` say that the
API is not yet stable. Where the README mentions the optional instance, 7416533 wrote
down its single test and its count, the same day.

## Hypotheses that were wrong

The wrong hypotheses are kept here, each with what tested it. The assistant can be
wrong with confidence. So the record must show how each conclusion was reached.

| Stated | What refuted it | What was actually true |
|---|---|---|
| The Renode timer model is conformant. Then, once suspected, that it was the culprit, with no evidence either way | A reproducer of two register writes, outside any kernel | Two real defects in the model: the `UG` callback ignored the value written, and `CC1G`–`CC4G` were not implemented |
| `TIMER_DBGPAUSE` is not why the kernel starts late | The binary under test was stale: no `Makefile` tracked the header dependencies | It was the cause. The build system was fixed as a result |
| The narrow pulses of the 20 and 60 ms tasks defeat the frequency counter | Eight readings in a row, stable to one part in a hundred thousand | The instrument was fine. The earlier failure came from the firmware |
| The kernel's cost per activation is what trips the overload guard | Measurement: 7.0 µs per round, 0.7 % of the processor | The core was still running on the crystal. The guard was right |
| Rewriting history to drop deleted blobs would shrink `.git` from 12 MB to 7 MB | The repository, measured after the attempt | No gain at all that day, and the operation was reverted. Measured again later on a copy, the same idea took 12 MB down to 7. So the first measurement was of something else. What it was has not been found |
| Including the dependency files at the top of a `Makefile` is harmless | `make` stopped building anything but one object file | The first rule that `make` reads becomes the default goal, and a `.d` file provides one |
| Building at `-O2` was a matter of adding the flag: it built, both emulation suites passed, and the board measured 3.2 µs per round instead of 7.0 | The CI, on its own toolchain | The kernel HardFaults there. Local success only proved that one compiler version was forgiving |
| Now that the hard kernel honours the algorithm an application selects, the soft and the power-aware kernels do too | The soft kernel run on the host: the deadline of every mandatory instance read 0 | Both headers set deadline-monotonic themselves, as `EscapementHard.h` had. The soft kernel had never run under EDF since the takeover. The power-aware example was configured for EDF, but ran deadline-monotonic |
| Building the soft kernel under EDF only needs the header fixed | The `_Static_assert` on the task control block, which failed | The port tested the algorithm before any kernel header had defined its names. To the preprocessor, an undefined name is 0: `EDF == DEADLINE_MONOTONIC_SCHEDULING` read `0 == 0`. The context switch would then have read the task's entry point four bytes off. The names now live in `Escapement_Modes.h`, which is included first |
| The power-aware kernel stalls on the board. Once looked at closely, so does the hard one | A trace written by the firmware itself, and read without stopping a core | The hard kernel stalled only while the debugger stopped core 0 to read where it was. Time ran on, the probe missed its deadline, and the overload guard fired. The power-aware kernel did stall on its own, from a timer interrupt that the previous image had left pending. This was a defect of the port. Neither Renode nor a start from power-up could show it |
| The host test of the soft kernel is deterministic: time is a variable, and nothing else varies | The same code, green on `main`, failed the next CI run with a signed overflow | The kernel read the period of the idle task, which has none. It read it from whatever the heap held past the task's block. AddressSanitizer now reports it on every run |
| The four Pico builds passed their suite, so the Cortex-M0 context switch was sound | The power-aware kernel on the Pico read and wrote past the end of the RAM from its timer handler. This showed as a warning in the Renode log, not as a failed test | The context switch reduced the state of every starting task to its running bit with an `ANDS`. The Cortex-M3 variant tests into another register instead. The idle task lost `TASKTYPE_BLOCKING`. The power-aware kernel then took it for a periodic task with work left, and went 44 bytes into a 20-byte block. The hard and soft kernels consult that flag on the idle task only to sort arrivals ahead of it. They then read a field the idle task does not have, which happened to do no harm in the examples. The defect would also have hurt an event-driven task, and no Pico example has one. The suite now fails on any access past the RAM |
| The Cortex-M0 context switch saves what a task needs: R4-R7, as the ZottaOS port for it said. R8-R11 are out of reach of Thumb-1 code | `IPCPico` under Renode. It failed under the soft kernel and deadline-monotonic scheduling once both fixes that the endurance test called for were in. It passed as soon as anything observed it. A watchpoint set only past 35.98 ms caught the Filler writing a slot's number into a node of the FIFO queue (2026-09-25) | R8-R11 are preserved across calls, as R4-R7 are, and GCC uses them in Thumb-1 code. A task that preempts another and ends is wiped from the stack, with whatever it left in them. The Filler resumed inside `OSWriteBuffer` with the preempting task's R8 as the index of its latest slot. Its next slot pointed nine slots past the three. The fixes had only led GCC to keep a value in R8 across a point where a task can be preempted. The handler now saves R8-R11 through R4-R7. `IPCPico` checks them around a delay, while the tasks that preempt it leave them changed. It fails on the old handler under the hard, the soft and the power-aware kernels |
| `SoakPico2` stopped under Renode on the kernel's overload check, only with core 1 running. The assistant stated that Renode's exclusives were the cause, and changed the test to play the RP2350's monitor, as the tests between the cores already do | The soft kernel under deadline-monotonic scheduling still stopped in 4 runs of 4, with the monitor played. Then watchpoints on the state of the task that missed its deadline, on `_OSNoSaveContext` and on `_OSActiveTask` (2026-09-25) | `OSEndTask` marks the task a zombie, then says that its context is not to be saved. Nothing kept GCC from storing the flag first. It did so for the Cortex-M33 of the Pico 2 and the M4 of the F4, under the hard and the soft kernels. For the M0+ it happened to keep the order. An interrupt in between found a running task that was not a zombie. It left the task in the ready queue, and wiped its stack in the switch it made. The kernel still saw the task as running. Later the task was resumed with the context of the task it had preempted, here the idle task, and it stopped the kernel at its next arrival. Core 1 only moved the interrupts onto that instruction. A compiler barrier now separates the two stores in all three kernels. The monitor is out of the test, and the test passes without it: 6 runs of 6 alone under the hard kernel, and the suite twice under each of the four builds of the Pico 2 |
| The 3-slot buffer is Chen and Burns' mechanism, with an LL/SC pair where they use compare-and-swap. So it is as sound as theirs | An exhaustive model of the reader and the writer (`test/model/threeslot.py`), with the LL/SC pair emulated as on the Cortex-M0+ | A compare-and-swap fails only when the value changed. A store-conditional also fails when an interrupt merely came between it and its load-linked. The reader then left `Reading` at 3 and read slot 3 of three. The reader now tries again. The host test makes store-conditionals fail on purpose to check it |
| The 3-slot buffer, with its reader retrying, is as sound between two cores as on one | The model taken to two cores, each with its own reservation, which a store of the other core to `Reading` clears (2026-09-24) | The writer tried its store-conditional once. On one core that was harmless: the interrupt that makes it fail also clears the reader's reservation. Between two cores, the reader's reservation survives. Its store-conditional then hands it a `Latest` read before the writer published. The writer picks the slot that is neither `Reading` nor the new `Latest`, and so writes the slot being read. The writer now tries again, in all three kernels. The model also fails with monitors local to each core. This is why the RP2350 needs `ACTLR.EXTEXCLALL` |
| The slot buffers need nothing more between two cores than on one: the 4-slot buffer carried 320,000 reads between the cores of the Pico with none torn, and the models of both held on two cores | The same models, with each core free to perform its loads and stores out of program order, as Armv6-M and Armv8-M allow for Normal memory (2026-09-25) | Nothing in the kernels ordered a slot's bytes before its announcement, nor the reader's announcement before its copy. Each buffer needs four barriers. The model catches the removal of any one: a read then mixes two records. The kernels now have them: a `DMB` on the RP2040 and the RP2350. The board showed no torn read without them. That proves only that its cores did not reorder those accesses in that test. The architecture does not promise it |
| The FIFO queue of Evéquoz's Figure 3 can be used between the two cores of the RP2350 as it stands: it is lock-free, and built on LL/SC, which the chip has | An exhaustive model of two cores running it (`test/model/fifo_mp.py`), with the RP2350's monitor and SCs that may fail for no reason (2026-09-25) | Figure 3 assumes the LL/SC of the paper's Figure 2. There, an SC fails only when another thread's SC succeeded on the same word. So an enqueuer whose one SC on Tail fails may assume that Tail moved on. The paper warns that real LL/SC give less, and offers another algorithm for them (its Section 5 and Figure 5). On the RP2350, an SC also fails when the other core wrote anywhere in the granule, or for no visible reason. Tail then stays behind an enqueue that returned. A dequeue that follows answers that the queue is empty while it holds the item. The same goes for Head. The model shows Figure 3 holding under the paper's LL/SC and failing under the chip's. `Escapement_CoreQueue.c` keeps Figure 3, and tries that SC again while the LL still finds the old index |
| A DMB between any two accesses of the queue between the cores to different words, fifteen in all, is more than it needs. A model of weakly ordered cores would say which to keep | `explore_weak` in `test/model/fifo_mp.py`, each core performing its accesses in any order Armv8-M allows. It ran at first with three of them pending at most. Then, on a machine of 30 GB, it ran with as many as the longest run of accesses between two barriers (2026-09-26) | Five are needed, and each is caught when left out. Without the one after D5, a dequeue answers that the queue is empty while it holds an item. Without any one of the other four, a dequeuer reads an item's contents before its enqueuer wrote them. The narrow window could say nothing about the ten others. It held without the barrier at the entry of Enqueue, although the architecture lets the publishing SC overtake the store there unless another DMB stops it. The window as wide as the runs between barriers, which no access crosses, showed nine of them superfluous, one at a time. It showed the five, with the one before a dequeue returns, to be enough. Each scenario took 25 minutes and 16 GB at most. Without that last barrier, one case outgrew 26 GB. The queue keeps six, within the model's bounds: two places, two operations per core, two rounds of each loop. On 2026-09-29, with each state kept as a 16-byte fingerprint, the five held without the sixth too, at a longest window of 9 |
| The queue between the cores may count on an SC's write being seen before the writes after it: the model had always assumed so | The model letting a store pass an SC ahead of it (2026-09-29, prompted by an audit of the port). In it, each core's reservation follows program order, and only a write to its Location ends it. This follows the architecture as read (DDI0553B.y, B7.2.3: a control dependency starts from a read; atomic-ordered-before orders an SC after its own LL alone) | Wrong. With the six barriers, Tail was seen advanced before the item it counts was in its place. A dequeuer found the place empty and helped Head past it, and the item was lost, in all three scenarios. The RP2350's monitor covers the granule, yet it still let one run through. A DMB after E15 and one after D15 close it, and each is needed. The eight hold at their full windows, 5, 6 and 4. Without the one before a dequeue returns, the seven left hold too, at 5, 6 and 7. So the queue keeps seven, each needed. The chip likely never shows it, since its SC waits for the bus's answer. Neither the board nor Renode could have shown it. The architecture allows it, and that is what the model is for. The 3-slot buffer's model made the same assumption, and holds without it: after its SC, each core reads Reading again, which waits for the SC, and its next stores take their slot from that read |
| The RP2350's exclusive monitor, played under Renode by hooks on the LL/SC functions, is played right: each hook runs whole | `FIFOCoresPico2`, with each core a producer and a consumer of the same queues, at a slice of 1 us (2026-09-26) | 1,004 records were taken of 1,000 written, and the sums of their counters were off. The queue's model holds with two of each. With the played monitor under a lock, every record came through once. The hooks of the two cores can run at once, and two SCs on one granule had both found their reservation |
| The FIFO queue of the kernels is wait-free on one core: whoever preempts an announced operation completes it | An exhaustive model of the queue (`test/model/fifo.py`), each run checked for linearizability | A dequeue that signals an event and finds the queue empty leaves the signal. Then it gets preempted before marking itself done. The first helper to see the signal left without marking it done either. A waiting task then took the signal. A second helper, preempted since before, found the slot empty and put a signal back. One event woke two tasks. The dequeue now marks itself done when it finds its signal |
| A full FIFO queue that refuses a node would corrupt its tail, since it has no capacity test like the one in Evéquoz's paper. Stated by the assistant from reading the code | The host test, strengthened to check the queue after the refusal | The enqueue only ever writes into an empty slot. The reading had missed that guard. The queue refuses cleanly |
| DRA, DR_OTE and DM_SLACK compile, and only running them is left. The roadmap said so, from reading the build | Building the power-aware kernel with each of them, on the target and on the host (2026-09-24) | None compiled. DRA and DR_OTE called an interrupt intrinsic of the MSP430. They also gave the task control block a third link, where the context switch reads its fields. DM_SLACK read a clock variable that no longer exists. Once they built, the host test found four defects. First, the speed computation multiplied a time by a ratio, and overflowed past 21 s; OTE shares it. Second, DRA shifted a time that nothing updates without event-driven tasks, and overflowed at the third wraparound. Third, DRA looked for the running task in its simulation queue, and read through a null link when the task had outlived its WCET there. Fourth, DM_SLACK gave the time a task left unused to tasks of higher priority, which never counted it in their response time. A hand-built task set shows the first task missing its deadline. The time now goes to tasks of lower priority, as the kernel's own comment said |
| `SleepU5`, loaded on the NUCLEO-U575ZI-Q with no error from OpenOCD, was running and failing on its own | Halting the core after the load: it was in its HardFault handler before the image started (2026-10-02) | JP2 was off. It carries the ST-LINK's reset to the MCU. So the "reset halt" reset nothing, the image was started inside the handler, and the loader reported success |
| `SleepU5` hung in Stop 2 on the NUCLEO because of the board: its revision X silicon, or its supply, VDD at 1.8 V on the LDO. Stated by the assistant after the first trials | The same hang on the SMPS and at 3.3 V. Then a second agent, started afresh, compared the two boards' loaders down to their OpenOCD scripts (2026-10-02) | The loader. Debian's OpenOCD sets DBG_STOP and DBG_STANDBY at each connection. The UNO Q's own OpenOCD clears them. The port cleared DBG_STOP alone, so the chip entered Stop 2 with DBG_STANDBY set, which holds off the reset. With both cleared, it ran |
| The NUCLEO-U575ZI-Q has no HSE, so its endurance runs since 2026-09-30 checked PLL1 on the MSIS of range 2. The assistant took this from its reading of the board's user manual (UM2861, 6.7) | Its RCC, read over SWD while its run was redeployed. A wake-up of some 60 ticks of LPTIM1, near the 64 that the port waits for an HSE, prompted the read (2026-10-02) | HSE ready, and PLL1 on it, M = 4: its crystal X3 is fitted. The manual leaves the HSE to the variant. The runs checked the HSE path. The MSIS of range 2 has run under Renode only |
| On PLL1 from the MSIS, at 160.017 MHz, the TIM2 of `SleepNoHSEU5` would run 106.7 ppm ahead of LPTIM1. Computed by the assistant, and made the bound of the board check | The check itself, red on its first run (2026-10-02): -4.1 ppm | TIM2 is set from LPTIM1 at every wake-up from Stop 2, and the image spends most of its time there. PLL1's rate shows only while the image is awake. The computation was right about the clock, and wrong about what TIM2 counts. The path itself ran without a fault |
| The PPK2 read nothing on the NUCLEO, or the debugger could not reach the MCU through it. The assistant and an agent gave three causes in turn: a voltage drop in its shunt, then VOUT on the board's rail, then a fault of its range switching that its firmware once had (2026-10-03) | The firmware, found up to date. The rail, found at 3.31 V with the PPK2 at 3.5 V. Then photos of the wiring | VIN and VOUT were the wrong way round on JP5. The MCU ran through the body diode of the PPK2's switch, and the PPK2 read 0. Turned round, as an ampere meter, it measured. The debugger still does not reach the MCU through it, and this is left unexplained: the image is put in the flash with JP5 fitted |
| The host test covered 97 % of the kernels' lines, as clang's coverage merged over the nine builds said (2026-10-04) | Each test binary read on its own, then a test written for each branch never taken | The merged view had left out lines compiled in one build only. Two of them, in DM_SLACK, had never run. In all, 68 lines had never run. A test written for one of them found a defect inherited from ZottaOS. The soft kernel set an event-driven task's next release again each time it elected it. Under deadline-monotonic scheduling, an instance ended 419 ticks after its signal. Its deadline was 300, and its response by analysis 210 |
| CBMC could prove the kernels' FIFO queue as compiled, with `goto-instrument --isr` placing an interrupt before each shared access, nested as on one core (2026-10-04) | A first run succeeded in half a second, while an assertion that the interrupt never preempted the operation held too. Then the same run with the interrupt in place | The interrupt routine had been dropped as unused before it was instrumented, so nothing ran inside the operation. With it in place, one operation and one interrupt did not conclude in 20 minutes and 3.6 GB. The proof stays with `test/model/fifo.py`, a model written anew, and the compiled order with `tools/check_order.py` |
| The kernel schedules by earliest deadline first: the first line of the README, and what sets the project apart | The scheduler run on the host, where a mirror of the task control block read a pointer where a deadline belonged | Every example shipped scheduled deadline-monotonic. `EscapementHard.h` set the algorithm itself, with no `#ifndef`, and no configuration file overrode it |
| On an overflow, the RP2040's MPU region over the 1 KB below the stack would lock up the core, since the HardFault's own stacking faults in the region. Written by the assistant on 2026-10-04, and in the code's comment | `StackGuardPico` on the Pico (2026-10-05): DHCSR's S_LOCKUP at 0, the core in its HardFault handler | HFNMIENA at 0 turns the MPU off at the HardFault's priority. The HardFault's stacking lands in the region, the handler runs, and the globals stay untouched |
| `Sentinel` was written over under Renode in the deadline-monotonic build, and Renode was the cause: its stacking of the fault walked down past the region. Stated by the assistant, since the EDF build kept `Sentinel` intact and Renode takes a MemManage that ARMv6-M does not have | The disassembly of `Recurse` | GCC had put two levels of it in one frame of 1,536 bytes, which stepped over the 1 KB region. The test now keeps it out of line. A frame of 1 KB or more is a limit of the guard, written in its comment |
| Under EDF, the soft kernel's test of an optional instance need count only the mandatory instances due by its deadline, since the others wait for it. Most of the instances it drops would then run. Proposed by the assistant on 2026-10-07, and accepted | The kernel changed so, under `tools/differential.py`. A task set whose instances all take their WCET hung on the overload guard: a mandatory instance ended at 2,412, its deadline 2,384 | The instances that wait for it must still keep their own deadlines. Counting every instance released before its deadline protected them. `worst_case_end` simulated only up to the optional instance's end, and so counted as safe some drops that made other instances miss. It now runs until the processor would first be free. It fails the kernel so changed at 1,703, before the hang. The change was not kept |
| The test of an optional instance by processor demand is exact, and keeps every deadline when every instance takes its WCET. Stated by the assistant on 2026-10-07, and ported into the kernel with the user's agreement | `BenchAdmissionPico` on the Pico, the same night. The kernel's overload guard stopped the board after 42 tests, then after 634 with interrupts unmasked. The count that this test replaced ran 20,000 tests | Exact only for a kernel that costs nothing. The test itself took up to 420 µs at 125 MHz, from the instant it assumed the instance would start, and nothing counted that time. The host, the simulation and the reference all had the kernel's code take no time |
| The host test runs a task as the target does. Taken for granted since the host test was written, and by the assistant again on 2026-10-12 for the overruns | A set of (m,k)-firm tasks that overran, drawn by `differential.py` in the review of the overrun handling (2026-10-12). The soft kernel saw an optional instance that had run for 300 ticks still in state INIT, and dropped it while it ran | The target's context switch sets STATE_RUNNING on a task it starts (`Escapement_CortexMx_a.S`). The host called the task's code and never set it. So every test of STATE_RUNNING in the soft kernel took another path on the host than on the target. test/host's Dispatch() now sets it. No earlier result changed: the difference matters only to an instance preempted or late. The same review found two more ways a fault got through: no set that overran held (m,k)-firm tasks, and a task lost from the ready queue looked, in an overload, like one whose releases are skipped (CheckQueues) |

One more mistake was of another kind. An early rebranding pass deleted a comment
terminator in 32 files. The first attempt to repair them corrupted 57 healthy ones. It
was undone with `git checkout`, then redone from an exact list. Every step was
committed, so this cost little.

## What `-O2` exposed

Adding `-O2` to the Makefiles built cleanly. Both Renode suites passed locally, three
times over. The board reported the cost per activation falling from 7.0 to 3.2 µs. Then
the CI failed the first assertion of the stm32f4 suite. Widening the tester's window
changed nothing, so the window was not the cause.

The ELF that the CI had built failed at once under the *local* emulator too. So the
binary was at fault, not the runner. Its outputs showed the kernel raising one output at
19 µs, then stopping dead. The program counter sat in `HardFaultException`. `CFSR` read
`0x00040000`, which is `INVPC`: an invalid exception return. The context switch builds
its own exception frame by hand. The architecture tolerates this only as long as the
compiler leaves the code around it alone.

The defect was a missing barrier. Setting PendSV pending does not take the exception at
once. The write has to reach the NVIC, and the processor has to see the pending state
before it runs what follows. `OSEndTask` relies on never returning. A task starts with
`0xFFFFFFF9` in `LR`, an `EXC_RETURN` value, so a return from thread mode faults. At
`-O0`, the epilogue was long enough for the exception to arrive first. Optimised, `bx lr`
sits one instruction after the store. A `dsb` and an `isb` close the window. The CI
agrees, and the build is at `-O2`.

A local build that passes says something about one version of one compiler. Here, the
CI's toolchain, not the one on the developer's machine, was the only thing between this
defect and a repository that claimed to be measured and verified. The first diagnosis
was wrong, too. The registers were written through non-volatile pointers, so it
concluded that the compiler had dropped the store. The disassembly of the CI binary
showed the store there all along. The `volatile` was added anyway, since the code had no
right to count on that store being kept. But it was not the bug.

## The repository was not scheduling the way it said

The README put earliest-deadline-first scheduling first. EDF is what sets this kernel
apart from a fixed-priority one. Yet `EscapementHard.h` selected deadline-monotonic
scheduling itself, in a plain `#define` with no `#ifndef` around it. No example
overrode it, and none could have.

The examples scheduled correctly, and the emulation tests passed. On a frequency
counter, the periods were right to one part in a hundred thousand. All of that is just
as true under deadline-monotonic scheduling. The problem showed once the scheduler ran
on the host. A build there read the deadline field of the elected task, and got a
pointer back. Under deadline-monotonic scheduling, the kernel inserts a priority byte
and drops the two deadline fields. That moves every field after them.

The algorithm is now chosen in `Escapement_Config.h`. The header only supplies the
fallback, and every example selects EDF. All three emulation suites still pass. The
host test also checks that no deadline is missed. That check could not be written while
the field it reads did not exist.

## Audits, and what they leave unproved

### The line-by-line audit

The new code, the RP2040 port and the Cortex-M layer, has been audited line by line.
The audit found four defects:

- a race between `OSEnqueueUART` and its interrupt;
- a transmission of zero bytes that emptied 64 KB onto the port;
- missing header dependencies;
- stale comments.

The kernel inherited from ZottaOS went through the same audit on 2026-09-25. Each
finding was reproduced by a host test that failed before its fix (`test/host/README.md`):

- a slot buffer lost a slot to a reader that preempted its writer, and read fields that
  `OSMalloc` had not cleared;
- an event-driven task that ended at its deadline was inserted twice in the ready queue;
- an event-driven task that waited past a wrap was sorted before periodic tasks due
  earlier;
- at the wrap, some deadlines were not shifted: in the soft kernel's optional instances,
  in the power-aware kernel under deadline-monotonic scheduling, and in DRA's simulation
  queue;
- a DM_SLACK slack was given twice;
- a task past its WCET was slowed to the slowest speed;
- the soft kernel's test of optional instances overflowed;
- the kernel accepted creations it cannot count with, and now refuses them.

In the compiled code of the Cortex-M0+, three of the five emulated load-linked read the
value before they set the reservation. So an interrupt in between went unseen. A
compiler barrier now keeps the order.

The RP2350 and STM32U5 ports went through the same audit on 2026-09-29. Two agents read
one port each. Every finding was then checked by hand against the code. Two defects were
each reproduced under Renode by a test that fails on the code from before:

- On the STM32U5, the idle task in Stop 2 slept past the 2^30 wrap when the next
  arrival was an event-driven task's, beyond the wrap. The kernel's clock lost the
  overshoot, up to 2 s on the board, and nothing was counted late (`Stop2EventWrapU5`,
  `stm32u5.md`).
- `OSEnqueueUART` started the transmission at task level with only the UART's interrupt
  masked. A task that preempted another inside it emptied the same buffer from where the
  other stood. On the Pico 2, 179 lines of some 18,600 came out broken in 0.3 s. On the
  Pico, 60 of some 400 did, and its senders then all but stopped (`UARTSendersPico2`,
  `UARTSendersPico`). The RP2040 had the defect too, from the port that the RP2350's was
  taken from. The first audit had closed the race of the call with its own interrupt,
  not with another caller. On the Pico 2 itself, on 2026-09-30, 18,106 lines in 20 s
  came out whole (`tools/pico2_uart.py`).

The smaller findings were fixed the same day without a test of their own. Each was read
against the code, and the whole suites were run again after:

- The U585's vector table trapped as reserved five interrupts that it has (SAES, AES,
  PKA, OTFDEC1 and OTFDEC2). The table had been taken from the U575's.
- The flash prefetch was on. That is the condition of erratum 2.2.26, and the prefetch
  served nothing to images in SRAM.
- `OSInitUART` returned TRUE with its queue unallocated. `OSInitTimerEvent` returned
  nothing, and wrote past an empty block when asked for no node.
- The Pico ports took blocks out of reset by a read-modify-write of `RESETS`. That
  could put back in reset what core 1 had just taken out.
- `OSGetStop2Counts` unmasked the interrupts whatever the caller's state.

The comment on the booster's clock was wrong too. The booster takes PLL1's source before
the divider M: 16 MHz from the HSE, not 4.

One finding was left for later. Without the HSE, PLL1 took the MSIS at 3.998 MHz, under
the 4 MHz floor of its input. Since 2026-09-30 it takes the MSIS of range 2, at 16.0017
MHz. An independent review of that change found two faults the same day; both were
fixed. The NUCLEO-U575ZI-Q was thought to have no HSE, but it has one (see the
hypotheses above). The path first ran on a chip on 2026-10-02, as `SleepNoHSEU5` on
the UNO Q (`stm32u5.md`, "Without the HSE").

### The review before release 0.1

This was release 0.1's task 12, on 2026-10-11. Five agents on Claude Fable 5.1 read the
code at fef9d5e. Each was given one part, and none saw the others' conclusions. Each
finding was then read against the code or the data before it counted.

**The documentation against the code and the measurements.** Nineteen findings, all
confirmed. None was a defect of the code. They were claims that later work had made
false, above all where the documentation lagged the STM32U3's arrival:

- The STM32U3's page still said that the board had not come, and that nothing on it was
  measured.
- The RP2040's page said that its current was not measured.
- The README counted three boards checked at each commit. There are four since
  2026-10-10.
- `architecture.md` had no STM32U3 among its targets.
- `build.md` gave the power-aware kernel to the Pico alone, and wrote only
  `SleepU5Flash` into a flash.
- Two places said that the Pico's second week of endurance was still running. It was
  stopped on 2026-10-10.
- The suites' counts of tests were those of 2026-10-05.

Two figures had no record behind them. The relock of PLL_SYS was 53 µs in
`power-aware.md`, where the run recorded in `rp2040.md` gives 55 at most. The lines
of the RP2040 port were miscounted by two. Each is corrected where it stood, to the
figure measured. One item is left as it is, because it is dated: the sizes in the README
and in `build.md`, from bf447b8. They have grown since. The `TaskLEDPico` of fef9d5e
takes 4,896 / 8 / 2,264 bytes with GCC 16.2, with the MPU's guard in its `bss`. Release
item 4 measures them again on the candidate.

**The RP2040 and RP2350 ports.** Six findings. Five were confirmed against the code and
fixed. None was reproduced on a board:

- `OSInitSleepGate` armed alarm 3 with the timer still in reset, after a power-on or a
  watchdog reset. `_OSInitializeTimer` takes the timer out of reset, but it runs only
  with `OSStartMultitasking`. The images on the bench never met this, since each was
  loaded after one that had taken the timer out of reset. The gate now takes the timer
  out of reset itself, on both ports.
- When a previous image had left alarm 2 armed, the idle task's sleep took it for armed,
  with its time then 0. So no sleep was long enough to stop PLL_SYS, until the alarm fired. Now
  alarm 2 counts only when an event manager is on it, on both ports.
- The RP2040's clock set-up wrote 0 to the whole of CLK_SYS_CTRL. This turned the
  auxiliary mux to the PLL, still in reset, at the moment the glitchless mux left it.
  The RP2350's set-up had been fixed on 2026-09-30, and the RP2040's had not, though
  shared ports must share such a fix.
- Three comments said what the code no longer does. `make SLEEP_GATE=1
  SLEEP_SPEED=0` built an idle task that ignores its sleep speed. The Makefile now
  refuses the pair.

Left open: the RP2040's idle task moves the auxiliary mux off PLL_SYS before it stops
it. Without that, the chip faulted when it woke from SLEEP. The RP2350's idle task
leaves the mux, and works. Both are measured on their boards. Why the RP2040 faulted is
not understood.

**The STM32U3 and STM32U5 ports.** Every register, address and bit that the STM32U3 port
writes was read against RM0487 rev. 3, and so were the power-aware driver's sequences.
None was wrong. Seven smaller findings were confirmed against the code:

- `make`, then `make KERNEL=PA` in the same directory, linked the hard kernel's objects
  into images named for the other kernel. The dependencies of `-MMD` do not see the
  flags. This was reproduced on the U3, and every port's Makefile had it. Each Makefile
  now keeps the flags of its last build, and removes the objects when they change. They
  are removed rather than dated: macOS's make 3.81 dates to the second, and an object
  of the same second passed for new.
- The U3's set-up left the LSE off if it found LSEON cleared and LSERDY not yet fallen,
  a state that lasts six cycles of the LSE. It now starts the LSE again, and writes its
  drive only once both read 0.
- `OSMSILocked` reported the PLL mode enabled, not locked. An LSE lost for good
  after a lock also went uncounted at every wake-up. Both now report what happened.
- `OSSetStop2Wake` took a margin as long as the shortest sleep, which wrapped the
  sleep's length: the chip woke 1.83 s late. The margin is now held under
  `OS_STOP2_MIN_US`, on both ports.
- A comment of the U3's said that the MSIK was unused. The Makefile did not say that
  an idle task with Stop 2 ignores `SLEEP_SPEED`.

Left open: the U5's idle task enters Stop 2 with USART1 enabled. RM0487 asks the U3 not
to do this, and RM0456 asks the same of the U5. Yet the U5 has run so for days. A fix
would be checked on its board.

**The three kernels.** Their shared FIFO and slot-buffer code is still identical in all
three. Five findings, three of them fixed:

- Under the power-aware kernel, LastRemainingWorkUpdate was shifted at every wrap, and
  brought forward only while a task ran. So were the simulation's time and its zombies
  under DRA and DR_OTE. After two or three wraps with nothing to run, they overflowed
  INT32. Under DRA, the simulation, stuck in the past, could grant a task slack it did
  not have. `TestLongIdle` runs a task with a period of three turns, across seven wraps.
  Before the fix, it stopped on the overflow at `EscapementHardPA.c:693`. Now the timer
  handler brings these times to the present when it hands the processor to the idle task, as
  DM_SLACK already did for LastRemainingWorkUpdate. The simulation consumes its work
  interval by interval. So advancing it more often changes nothing it computes, and
  `differential.py` checks this against the reference of the policies.
- `OSMalloc` rounded its size up within the 16 bits of its parameter. So 65,533 to
  65,535 bytes came to 0, and the block handed out was the one before. It now rounds in
  32 bits.
- Under DM, the soft kernel's test of an optional instance bounded the periodic tasks'
  sum at 2^30, but not the event-driven tasks' terms. Two of those could overflow it.
  The event-driven terms are now bounded as the periodic sum is
  (`TestFirmHeavyEvents`, which takes the sum past that bound).

Two are left open, both inherited from ZottaOS. Under EDF, the server deadline of the
event-driven tasks grows by each workload, without a bound. Two workloads near 2^30,
minutes each, could take it past INT32. Under EDF\*, an event-driven task ready
across a wrap keeps its release time unshifted until its next release. This changes only
a tie between two equal absolute deadlines.

**The flash images and the tools that write and check them.** The second stage, its
CRC, the RP2350's IMAGE_DEF and the four layouts in flash were read, and had no fault.
Seven findings:

- The power-aware check of the STM32U3, added the same day, left the board's long run
  reading the board during the check. The run's reader would have taken the checked
  image for a restart, and loaded its own image over it in the middle of the check. The
  check of cc1a4ac ran with the run stopped by hand, and passed. `BOARD_SOAK_SERVICE=none`
  now stops the run.
- `tools/board_ci.sh` read GitHub's answers through a pipe into jq. The pipe's status
  was jq's, which is 0 on an empty input, and it stood for the request's. So a request
  that failed marked the commit as checked, with the status `error`, instead of being
  tried again. The answer is now kept first.
- `tools/pico_flash.sh` and `pico2_flash.sh` always ended with 0. A write that failed
  left the previous image running, for the next tool to read. They now end with the
  verification's outcome.
- Three comments said that the images run from SRAM, where `FLASH=1` puts them in the
  flash. `board_ci.md` said that a check takes 20 minutes, where the last took 28.

Left open:

- In the RP2040's flash image, libgcc's division stays in the flash. The kernel's queue
  in SRAM calls it through a veneer, the first call through a cold cache.
- `tools/soak.py` loads a Pico image as an SRAM image, whichever it is. It is used by
  hand only.
- `tools/nucleo_flash.sh` writes again when its verification fails for any reason, a
  read error included. At worst that costs one counted writing.

**What checking the fixes found.** The STM32U3 starts its LSE once, at start. The fix to
that start moved the code by a few bytes, and the Renode suite then hung in
`SleepWrapU3`: the chip asleep in Stop 2 for good, LPTIM1 counting, its compare at 0.
The wake-up had fallen on a count of 0, once in 65,536. Renode's model of LPTIM1 refuses
a compare of 0 ("Compare value cannot be 0"). The chip matches it as the counter passes
0 at each reload (RM0487, LPTIMx_ISR, CC1IF, p. 1925). The U5 uses the same driver, and
has entered Stop 2 some 150,000 times an hour on its boards without hanging. The suites
had passed only because of where the wake-ups happened to fall. The platforms of both
STM32 ports now take a fixed copy of the model
(`emulation/renode/Escapement_STM32L0_LpTimer.cs`), as they took one of the timer's. The
driver stays as the boards ran it.

**The mutants on the candidate.** Run whole on c0ff963, at 300 task sets a build:

- Hard: 87.6 % (424 of 484), and 97.0 % of the 437 not declared equivalent, as before
  the review.
- Soft: 84.0 % (609 of 725), and 96.7 % of the 630 not declared equivalent. Its one new survivor
  is the bound that the review put after the share for events
  (`totalWork >= 0x3FFFFFFF`). It is declared, as its two twins are.
- Power-aware: 83.0 % (664 of 800), and 96.2 % of the 690 not declared equivalent.

One declaration was wrong, and `tools/mutants.py` said so. DM_SLACK's slack, left
negative once run out, was read on 2026-10-10 as harmless. In fact it overflows its 32
bits after two wraps idle, and `TestLongIdle` kills it. The declaration is gone.

Two survivors are new. Both were killed before the review. Both have been hidden since
by the update that the review added to the handler when idle, which brings DRA's
simulation to the present at each pass:

- The pair of the simulation left pending by an interrupt and not completed on entry
  (117). `endinside` caught it. Now the handler's first update completes that pair in
  place of its own.
- The time of the last excess left unshifted at the wraparound (151). Set 116 of
  `pa_drote` caught it.

Each now costs one update of the simulation computed from a time behind, until the next
update: less excess and less slack, a faster speed, never a later end. They stay written
as behaviours no test reads.

### Two fixes that were wrong

The endurance test (`SoakPico`, `tools/soak.py`) showed that two of those fixes were
wrong. It did so the same day, once an interrupt and phases of high load had joined it.

The first fix set a slot buffer's status after its slot, rather than before. A reader
could then take the new slot early, while the status still said unread from the slot
before. It could take it again once the writer had marked it unread. On the board, 37
slots were read twice in 79 s, none torn. No order is right for a status kept apart from
the slot. Each slot now carries its number. A reader that takes each slot once compares
that number with the last one it took.

The second fix assumed that an event-driven task, signalled while it suspended itself,
was the task the timer handler had interrupted. The handler then finished its suspension
in its place. But a task of higher priority could have preempted it and signalled it
instead. The context of that higher-priority task was then discarded. The soft kernel
hung under Renode, and the host test segfaulted under deadline-monotonic scheduling.
Interrupts are now masked from the enqueue of the suspending task until it leaves the
ready queue, so the handler never sees such a task. Both defects have a host test that
fails on the code from before (`test/host/README.md`).

### Races and limits closed after the audit

The audit left races open and listed limits of the design, seven in all. They were
closed on 2026-09-25 and 26. The host reached each of them, except strict aliasing,
which is turned off.

The counter can wrap while the timer handler runs, after the handler has found no
overflow. The host reaches that window with a hook (`wrapinside`). The handler then
reads a time from after the wrap, while its arrivals are not yet shifted, and releases
nothing. It serves them once it finds that the overflow flag was raised in the meantime.
It now tests that flag again before it returns. The test fails when that second test is
taken out.

In the power-aware kernel, a task that ended between two interrupts left the next task
at the speed of the task that ended. The flag that told the handler to set the next
task's speed was raised before the task became a zombie. A first interrupt found the
task still running, and cleared the flag. A second interrupt, once the task was a
zombie, took the next task for one that had been running. The handler now reads
`_OSNoSaveContext`, which only the context switch clears. The host takes the timer
interrupt at the kernels' compiler barriers (`endinside`). With a barrier in the old
window, the kernel from before the fix fails that test (`test/host/README.md`).

Under DM_SLACK, a task that ended read the time before its reservation. An interrupt in
between could shift the clock at a wrap. The task was then left with a time from before
the wrap, against a last update from after it. The slack, left for the task that ended
after an idle time, then grew by the 2^30 of the shift at the next arrival. The time is
now read inside the reservation. The host runs a task set in which a task ends at the
last tick before the wrap. It takes the interrupt at each time read and each barrier of
that task's end (`timewrap`). The kernel from before the fix misses a deadline there.

An optional instance can still be ready at its next arrival, never started. The host
reaches that case with tasks that take time: the mandatory instances keep the processor
busy across its period (`firmwait`). The kernel takes the instance out of the ready
queue, as it should, under both algorithms.

An optional instance that has already started at that point is still an overload. The
kernel stops on it (`DEBUG_MODE`), by design. The schedulability test lets an instance
start only if it ends in time. Only a task that runs past its declared WCET can then
bring this case, or work that the test leaves out. Under EDF, the test left out the
event-driven tasks when no share of the processor was declared for them, although each
declared its WCET and workload. An optional instance started, an event delayed it, and
the kernel stopped on that guard at its next arrival (`firmeventwait`). The test now
reserves at least the share that each event-driven task takes: its WCET over its
workload.

The indices of the wait-free queue had 16 bits. An operation can be completed by another
that preempts it. That operation could still move an index it had read, once 65,000
operations during the preemption had brought the index back to the same value. The queue
then lost an item. The host reaches this by running those operations inside the LL of a
dequeue (`test_ipc`). With 32 bits, as the queue between the cores already had, it takes
2^32 operations (2026-09-26). Under Renode, once the code had grown longer, the
preemptions of the FIFO example no longer fell inside the queue's operations. The
example now varies the length of its loop for that reason. Strict aliasing, which GCC
does exploit here, is turned off (below).

### Scale

The execution tests run three or four tasks, and the host test ten. Nothing here shows
how the scheduler behaves with thirty. The kernel clock wraps at 2³⁰, about eighteen
minutes away on hardware. That wrap is crossed on the host, under Renode, and on the UNO
Q's STM32U5 by its endurance test (2026-09-26): four times in its first hour and a
quarter.

### Between the cores

The RP2350 port was audited line by line on 2026-09-29 (above). Its emulated platform
acknowledges its clocks blindly (`emulation.md`). A Pico 2 has run the port since
2026-09-28, with its clocks checked and its outputs timed on a frequency counter
(`architecture.md`).

Between the cores, the models cover the orders of access that the architecture allows.
`tools/check_order.py` checks that the compiled code keeps the order the models need. It
runs in the CI, on every path through the buffers' functions, on every build of:

- the Pico and the Pico 2;
- the STM32U5, since its port (2026-09-25);
- the STM32U3, since its port (2026-10-06).

`tools/check_order_mutants.sh` shows the check failing without any one of the barriers.
The two buffers' source has had nine of them since 2026-09-25, when the status of a
buffer came to be set after its slot is handed over. The models leave that status out.

The queue between the cores had no such check until 2026-09-30. Only its source and its
model named its seven barriers. The check now follows the queue through its LL and SC,
which are calls. Each barrier removed from the source is caught by the pair of accesses
it stands between. An independent review found two holes in the check the same day, and
both were closed:

- A path went on past a return that GCC writes as `ldmia.w sp!, {..., pc}`. The check
  took that return for an ordinary load.
- The pairs let the E5 of the next round stand for the E10 that must follow the LL of
  the place. A queue without E10, or with E10 before the LL, therefore passed.

Changes to the algorithm that keep its order, such as a test turned around at E11, are
not for the check to find.

The check cannot see a reordering by the processor that the architecture does not allow:
that is the models' premise. Nor can it see the code that the compiler might emit for
another version or optimisation level, until it runs there. Without the barriers' memory
clobber, GCC 16.2 happened to keep the same order at -O2 (2026-09-25). The clobber is a
guarantee, not a fix to an observed fault.

### Between the cores, on a processor that reorders

The Pico 2 keeps its accesses in order: `LitmusPico2.c` found none reordered. On the
board, then, the queue between the cores shows that it runs, not that its seven barriers
are needed.

`test/litmus` (2026-10-04) compiles `Escapement_CoreQueue.c` unchanged for an Apple M3
Pro. Its LL and SC become LDXR and STXR, and its DMB a `DMB ISH`. That processor does
use the freedom that Armv8-M gives Normal memory. Two threads first run the three
classic litmus tests. Then each thread enqueues its own numbered items, and dequeues in
turns. Throughout, a check covers each item's contents and each producer's order. At the
end, it checks that every item was dequeued once. Each mutant then leaves one barrier
out of the processor, while keeping it in the compiler.

| Run, 2026-10-04 | Result |
|---|---|
| SB, MP, LB without a DMB, 19,999,744 instances each | 12,233,728 SB and 26,428 MP reordered; LB never |
| The same with a DMB | none |
| The queue, two places, its seven barriers, 4 runs of 1 to 20 million items each way | 0 errors |
| Without the barrier after E9 | caught in 5 runs of 5: an enqueuer's items out of order |
| Without the barrier after D9 | caught in 1 run of 5, with four pairs of threads running |
| Without the one after E5, E15, D5, D6 or D15 | caught in none of 2 runs each |

Two barriers are now shown to be needed on a real processor, not only in the model. The
five that the bench does not catch are no less needed. For each one, `fifo_mp.py` finds
a run that the architecture allows and that fails without it. This processor does not
happen to take that run. The bench stands for a processor that reorders, not for a
Cortex-M. No Cortex-M is known to reorder as far.

The kernel's 3-slot buffer went on the same bench the same day (`test/litmus/slots.c`).
`EscapementHard.c` is compiled as it stands, through the host port of `test/host` built
with `HOST_LITMUS`. Its barriers become a `DMB ISH`, and its LL and SC an LDXRB and an
STXRB. A writer writes items of 16 bytes, each with a number and a check word. A reader
copies the latest item out. A slot written while it is read, or named before it is
filled, comes out torn or short. The numbers read must never go backwards.

| Run, 2026-10-04 | Result |
|---|---|
| The buffer, its five barriers, 4 runs of 5 to 20 million items | 0 errors, some 64 million reads in 5 s |
| Without the barrier before `Latest` is written | caught in 5 runs of 5: a slot read short |
| Without the one before the reader's LL of `Reading` | caught in 5 runs of 5: a torn slot |
| Without the one after `Latest`, the one before `Status`, or the one before `Reading = 3` | caught in none of 5 runs each |

The two caught are the two for which the model gives the plainest reason: the slot must
be filled before it is named, and the reader's request must be seen before it reads the
slot.

The 4-slot buffer went on the bench next (`slots.c -4`). It has no LL or SC: each side
writes only words of its own.

| Run, 2026-10-04 | Result |
|---|---|
| The buffer, its five barriers, 4 runs of 5 to 20 million items | 0 errors, some 56 million reads in 4 s |
| Without the barrier before the writer names its slot in `Index` | caught in 5 runs of 5: a slot read short |
| Without the one before it names the pair in `Latest` | caught in 5 runs of 5: a slot read short |
| Without the one before the reader takes the slot from `Index` | caught in 5 runs of 5: a slot read short |
| Without the one before `Status`, or the one before the reader names its pair in `Reading` | caught in none of 5 runs each |

Of the three structures, the 4-slot buffer shows the most of its barriers needed on this
processor: three of five. The 3-slot buffer shows two of five, and the queue two of
seven. The barrier before `Status`, common to both buffers, is never caught. It matters
only for the first item, and the bench reads that item among millions.

The first version of the bench had a fault of its own. In place of the barrier it left
out, it counted the passes with an atomic add. That exclusive access, between an LDXR
and its STXR, cleared the reservation. Every SC then failed, and the mutant without the
barrier after E9 looped for ten minutes. The bench looked as if it had found a livelock
in the queue. Its mutants now add no access to memory, and a watchdog stops a thread
that makes no progress for 10 s.

### Store order on one core

The same holds on one core, for the stores a task makes that the timer interrupt may
find half done. The processor keeps their order. The compiler need not, and did not.

Besides `OSEndTask` (above), the soft kernel's `ScheduleNextTask` promotes an optional
instance in three stages. It marks the instance `STATE_ACTIVATE`, moves it to the head
of the ready queue in three steps, and sets it `STATE_INIT`. The interrupt completes a
promotion that it finds marked. GCC saw the mark overwritten and dropped it, on the
Cortex-M0+, the M33 and the M4 alike. A promotion interrupted there left the task both
at the head of the queue and in the list of optional instances. No test had shown it.

Compiler barriers now order these stores. They also order those of the two drops in the
same function, and the clearing of `SetActiveTaskRemainingTime` in the power-aware
kernel (a flag since removed, above). `tools/check_order.py` checks the compiled order
of each one on every path, on every build of the Pico, the Pico 2 and the F4
(2026-09-25). The F4 port was removed on 2026-09-26, and the STM32U5's builds are
checked in its place. Run on the kernels of the day before, the check reports exactly
the faults found by hand: `OSEndTask` on the M33 and the M4, and the promotion on all
three. The mutants script shows each rule failing on a source whose order is turned
around. An audit of the ports found no other such sequence, apart from one latent in the
UART, which is now ordered as well.

An independent review of the kernels found one more on 2026-09-30, in the power-aware
kernel under DRA and DR_OTE. A task that ends makes two compare-and-stores. They keep
their operands, and the step they reached, in static variables, so that the timer
interrupt can complete a pair that the task left half done. GCC kept the step in a
register, and wrote only its last value: the disassembly held no store of step 0 or 1.
An interrupt between the two stores found no pair pending. The simulation of DRA
then fell behind, and chose too low a speed. The variables are now volatile, and a
compiler barrier follows each store. The step is now set to the one after the step done,
instead of moving up by one from what it holds. Moving up by one had taken the step past
its end when a task resumed after the interrupt had completed its pair. The host test
takes an interrupt at each of those barriers.

The same review found that the soft kernel, under EDF, admitted an optional instance
that could not end in time. With an event-driven task whose server deadline lay far
ahead, `IsTaskSchedulable` subtracted the events' term from the work that the instance
and the mandatory instances need. Events due later, though, take nothing from that work.
The instance started, was still in the ready queue at its next arrival, and was inserted
again: the queue looped. The host test `firmdiscount` reproduces it, and fails on the
code from before.

### What else -O2 could take

What else -O2 could take was checked the same day. The host tests pass at -O2 under the
whole of UndefinedBehaviorSanitizer, with Clang on the Mac. The CI now does the same
with GCC. Every inline assembly statement that must keep its place among memory accesses
now declares it: `CLREX`, the `WFI` of the idle task, the `SEV` and `WFE` of the launch
of core 1. The 88 images of every build came out byte for byte the same, so these
declarations too are a guarantee rather than a fix.

The board machine ran `tools/check_order.py` on what its own GCC 16.2 built, while the
CI's compiler was 14.2. Since 2026-09-26, the CI builds with 16.2, and also checks the
order of every variant under 14.2. The bench is now an Arduino UNO Q with no GCC 16. It
runs the CI's images.

The code breaks the strict-aliasing rule, and GCC uses that rule. Built with
`-fno-strict-aliasing`, all 88 images change. Among them are functions of the kernels,
which access the same memory as `TCB`, `ETCB` and the port's `MinimalTCB`. The one
difference read, in `OSEndTask`, is a value reused instead of read again. It is correct
either way. No fault has been traced to aliasing. Every firmware is now built with
`-fno-strict-aliasing` anyway. This insures against what another version of GCC could
draw from the rule that the code breaks. On the board, that cost 68 bytes. Over two runs
of each, the mean round went from 3.4 to 3.5 µs, and the worst from 11 to 12 µs.

Built with `-Wextra -Wnull-dereference -Warray-bounds=2`, no variant gave any of the
warnings that the optimiser computes. A comparison of signedness, though, led to the
timer events of the STM32 port. On its 32-bit timers, the counter and the comparator
were read through pointers that were not volatile. GCC could therefore reuse the counter
read in the loop, and the comparator just written. It could then take an event whose
time had passed for one to come, 71 minutes later. GCC 16.2 happened to read them again.
The eleven accesses are now volatile, as those of the 16-bit path already were.

## The mutant campaigns, by date

### 2026-10-04 and 05: the first run

The first run, on 2026-10-04, killed these shares of the mutants that compiled:

- hard kernel: 355 of 484, 73.3 %;
- soft kernel: 487 of 718, 67.8 %;
- power-aware kernel: 536 of 798, 67.2 %.

The survivors of the hard kernel were read one by one. They fell in five kinds:

| Kind | Hard | What they were |
|---|---:|---|
| Equivalent: nothing a run can see changes | ~20 | ties broken the other way, a field set to the 0 it already held, a bound moved where no value reaches it |
| The target's only | ~30 | interrupts masked, compiler and memory barriers, the timer started, PendSV pended: the host's port does nothing there, and `check_order.py`, Renode and the boards are what read them |
| Hidden by the host | ~30 | every field a creation sets: the host's `OSMalloc` handed out zeroed blocks, where the target's SRAM holds what the last image left, so a field left unset read 0 and passed |
| The helpers of the wait-free queue | 7 | an operation finding its work done, which two nested interrupts reach: `test/model/fifo.py` explores them |
| Behaviour no test read | ~30 | below |

The last kind was the work. Beyond tests to write, it found three things:

- The host test's blocks are now filled with 0xA5, since SRAM is not zeros. No field
  that the kernels set turned out forgotten, but leaving one out now fails.
- `RunAcross` is the loop that jumps the host's clock from event to event. It ran a task
  whose period is past 2^30 a whole turn late, and no test had such a task. It is fixed,
  and `longperiod` tests it (`test/host/README.md`).
- No build compiled the variant of `EnqueueRescheduleQueue` for a port without
  `NonMaskableSoftwareTimer`: the three ports and the host defined it. The variant was
  ZottaOS's. It was removed on 2026-10-05, and the 97 images of the examples stayed the
  same byte for byte.

Tests were added for the rest:

- the edges of what a creation accepts (`createbounds`);
- a period past 2^30, run over three wraps (`longperiod`);
- two events signalled at once (`twosignals`);
- an event-driven task started at its signal, and spaced by its workload from the
  arrival queue (`eventspacing`);
- the edges of the slot buffers;
- the harness, which checks that a task that ends leaves its context unsaved and asks
  for a switch, as the timer handler must; the host's port only counts these requests.

`tools/differential.py` now also draws event-driven tasks: a workload apart under DM,
and a total bandwidth server under EDF. It also draws (m,k)-firm sets that overload
unless optional instances are dropped. Each task takes its WCET, so an optional instance
wrongly admitted misses its deadline.

Run again on the survivors, the scores rose:

- hard kernel: 79.5 % (385 of 484);
- soft kernel: 74.2 % (533 of 718);
- power-aware kernel: 71.8 % (573 of 798).

### 2026-10-05 and 06: the speeds held to a reference

A quarter of the power-aware kernel's survivors were in the choice of speed
(`GetProcessorSpeed`, DRA's simulation, DM_SLACK's slack). A mutant that picks a faster
speed than it needs keeps every deadline. Nothing held the policies to the speeds that
their specification gives.

Since 2026-10-05, `tools/speed_reference.py` does. It computes the speed of each
dispatch from chapter 6 of the ZottaOS manual. It was written before the kernel's code
was read. `tools/differential.py` fails a speed other than the reference's.

Run against the kernel, the reference first disagreed on most builds. Each disagreement
was read in the kernel before anything was changed. Three were the reference's own
mistakes:

- the releases past the end of the run, which the kernel knows;
- the work received, which the kernel keeps in whole ticks, as the manual's figure does
  in integers;
- when an end decides, before the release at the same instant.

The rest were the kernel departing from the manual. Each went towards a faster speed,
and none towards a missed deadline:

- The kernel set the speed only when the task to run changed, where the manual does it
  at every timer interrupt. A release that did not preempt thus reclaimed nothing.
- DM_SLACK took its slack only for an instance that was not alone, where DR_OTE takes
  the slower of its two speeds.

Both were ZottaOS's. Both were corrected the same day, and the reference agreed again.
Two departures stay, written in the reference. Its EDF* breaks ties the other way from
the paper, as its header says. DM_SLACK keeps one slack, the last left, where the manual
keeps one per task.

The reference agreed with the kernel on 1,000 task sets of each of the five builds,
before and after the corrections, on the Mac and in the CI's image before. Of the 171
mutants of the code that chose a speed before the corrections:

- 100 were killed without the reference;
- 109 with it;
- 112 at the CI's 300 task sets a build (`tools/mutants.py --sets`).

After the corrections, 104 of the 164 mutants of the same code were killed. The
survivors left were mostly of two kinds. Some were code of the event-driven tasks under
DRA, which the reference then left out, since the manual does not say where they stand
in its simulation. The others were comparisons whose edge gives the same speed.

Since the same evening, the reference also takes in event-driven tasks under OTE and
DM_SLACK: their next release bounds the stretch. Under DM_SLACK, it takes in the slack
that an event-driven instance leaves too. Of the 17 mutants of the code that bounds the
stretch, the 6 left are such comparisons.

Two of DM_SLACK's survivors were first written as behaviours no test reads. Read again
on 2026-10-06, neither is:

- Leaving out the `CompilerBarrier()` before the SC that hands a slack over changes
  nothing that GCC emits for the RP2040, the only target that builds DM_SLACK.
  `DMSlackCalculateSlack` disassembles the same with and without it. The barrier stays
  as a guard against a compiler that would sink the three stores past the call.
- A task given its own slack back (`>=` for `>` on the priorities) never meets one. A
  task keeps every deadline when it runs its remaining WCET at the fastest speed from
  any point. Its end, plus the slack it leaves, is then at most its deadline. The slack
  runs out with the time elapsed, and is gone by the task's next release, at least a
  deadline after the one before.

On 2026-10-06, the reference took in the event-driven tasks under DRA and DR_OTE, where
the manual is silent. It took them as the kernel has them:

- an instance in the simulation, by the server's deadline;
- the server's share of the time it has nothing pending, taken as excess.

The excess is counted and rounded at each update of the simulation, at the instants the
kernel makes them.

With the instances in the simulation but no excess yet, the reference disagreed, the
kernel always slower. Before the excess was written in, 10,000 task sets under the two
builds, checked for deadlines only, missed none. With the excess, DRA agreed on 3,300
sets.

DR_OTE disagreed once more, the kernel faster. An event-driven task that might be
released at once sent it to the fastest speed, with DRA's time left out. Only OTE should
have lost its time, as DM_SLACK has it since 80cc6dc. Corrected with the user's
agreement, DR_OTE agreed on 3,000 sets. A kernel made to count no excess fails its first
task sets under both builds.

### 2026-10-06: the survivors read, and the wraparound

On 2026-10-06 the mutants were run whole, at the 300 task sets a build that the CI runs,
rather than 100. The soft kernel scored 74.2 % again (533 of 718), and the hard one 79.5
% (385 of 484).

The hard kernel's score had first been written 80.4 % (389 of 484), and the run of
2026-10-06 seemed to lose four mutants. Its survivors were run again at f7622b7, the
source of that day. They gave 30 killed, not 34, and left the same 99 survivors as the
run of 2026-10-06. The 389 was miscounted, and nothing was lost. Under the hard kernel's
builds, 300 task sets cannot kill fewer mutants than 100: the first 100 are the same
sets, drawn from the same seed.

The power-aware kernel was run whole the same evening, at 300 task sets: 76.9 % (610 of
793), up from 71.8 %. Of the mutants that both runs hold, 85 survived on 2026-10-04 and
are killed now. The tests and the speed reference added since, and the larger runs,
killed them together; the runs do not tell which did. 12 of the 85 are in
`DRASimUpdateElapseTime`, the excess of the event-driven tasks that the reference took
in that day. The 183 left are mostly in the timer's handler (28), the choice of speed
(21) and the initialisation of the slot buffers (15).

The 21 in the choice of speed, `GetProcessorSpeed`, were read one by one that evening.
None is a behaviour left unchecked:

- One is the task given its own slack back, read above.
- Fourteen move the edge of a comparison where both sides give the same speed: a bound
  set to the value it is compared with, or a time left equal to the work. Only the
  fastest speed does that work in that time, at the operating points of the RP2040 and
  the host, 24 and 102 in 256ths.
- Two read DRA's excess with the opposite sign. `DRASimUpdateElapseTime`, called before
  each choice, has already spent it. A build made to trap on a non-zero excess there ran
  4,000 task sets under DRA and DR_OTE, and the host test, without trapping.
- Two give DM_SLACK a slack of 0 or 1 tick, which the fastest speed absorbs at those
  points.
- One starts the slack's bound at 1 rather than 0, which is the same again.
- The last runs a task with no work left on record at the slowest speed. That task ends
  at the instant it is dispatched, and no trace shows it run.

The same evening, the random task sets were taken across the wraparound. The host test's
trace now starts at any phase of the counter. Half the sets of `tools/differential.py`
now start just short of 2^30. The trace still counts time from the start, so every check
and the speed reference read the traces unchanged. Until then, every set had run within
its first 6,000 ticks, far from the wraparound. Only the hand-written tests `timewrap`,
`firmwrap` and `longperiod` reached it. The hard kernels held. The others did not, in
three ways:

- **OTE lost its time before each wraparound.** The power-aware kernel read the next
  arrival from its low part alone. An arrival past 2^30 then seemed past, and the last
  instance before each wraparound ran at the fastest speed. That was faster than needed,
  with no deadline at risk. `GetNextArrival` now counts the wraparound in.
- **A task that ended at the very tick of the wraparound read a time 2^30 short.** The
  counter had wrapped, but the timer's handler had not yet shifted the kernel's times.
  DM_SLACK credited the slack that the task left with 2^30, and the handler then took
  that from the work of the next task. The soft kernel saw every deadline 2^30 ahead. It
  admitted an optional instance that could not end in time, and the host test's
  scheduler never returned. Slowed down on 2^30 of slack, tasks of lower priority could
  miss their deadlines. The window is the few cycles between the counter's wrap and the
  handler, every eighteen minutes on the RP2040. A time read by a task is now taken past
  2^30 when it lies half the range of the counter before the last time the kernel set
  (`GetTaskTime`, in both kernels). `timewrap` and `firmwrapend` end a task at that
  tick, and the second fails without the correction.
- **The reference was wrong twice, and the kernel right.** First, the handler zeroes the
  server's deadline, and the time the excess was counted to, when it shifts them below
  zero. This loses the excess counted before the wraparound, as the kernel's comment
  says. Second, a task whose WCET the simulation has used up, dispatched again at the
  wraparound, gets no time from DRA, as the comment of `GetDRASlackTime` says. The
  reference now takes both in.

Both corrections of the kernels were made with the user's agreement.

Run again across the wraparound:

- the power-aware kernel's mutants scored 77.5 % (621 of 801);
- the hard kernel's survivors lost 3 more, in the code that shifts its times (388 of
  484, 80.2 %);
- the soft kernel's survivors lost 4 of the 179 found again, all in the code that
  carries the arrivals of its mandatory instances past the wraparound.

Since then, a mutant that was read and shown equivalent is declared in
`test/host/equivalent-mutants.jsonl`, with its reason. A declared mutant is found by
what it changes, as `--survivors` finds survivors. The declared mutant still runs. If
one declared equivalent is killed, it is reported, since the declaration is then wrong.
The score is given twice: of all the valid mutants, and of those not declared. The
power-aware kernel's 26 read that evening are the 21 of the choice of speed and 5 of
DM_SLACK's slack. They give 80.1 % of the 775 others. No mutant is declared that has not
been read, and none for being hard to kill. The two mutants of the excess changed a term
that is always 0 there. That term was then taken out of `GetProcessorSpeed`, with the
user's agreement, and its mutants with it: 24 remain declared.

The hard kernel's 96 survivors were read the same evening. Each was settled, where it
could be, by a build of the mutant, a model given the fault, or a crafted task set:

- 47 change nothing that the kernel promises: stores to zeroed sentinels, or stores
  overwritten before any read; ties that the algorithm leaves open; compiler barriers
  whose ARM code is the same without them; assertions of DEBUG_MODE. 46 of them are
  declared, giving 88.6 % of the 438 others, from 80.2 % of all. One was left out: the
  mutant that shifts all the deadline-monotonic priorities by one. Its argument near 255
  tasks is not tight enough.
- 5 more the host cannot see, and `tools/check_order.py` kills them on the ARM builds.
- The 44 left are behaviours no test reads. 17 of them are visible only on a board,
  where any example would catch most (the timer never started, the idle task left
  masked).

Of these 44, those that the host can reach, ranked by what they would cost:

- an event-driven task elected while still a zombie, its context then discarded;
- queue indices taken from leftover SRAM, which the host's uniform fill of 0xA5 hides;
- an event released late across the wraparound;
- the 3-slot writer taking the slot being read;
- a signal dropped until the next interrupt.

The reading also found a fault of the host test, not of the kernel. A deadline armed
past the wraparound made `HostTicksToNextEvent` skip the interrupt of the wrap. The
random task sets, whose signals are far apart, never met it.

Tests were then written for the survivors that the host can reach. Each kill was checked
by running the mutant again:

- A task that the kernel elects is never a zombie, except while it ends (212).
- The host's port asks for what a board needs at start: the timer initialised, then
  started with interrupts masked; the handler, only once it is; the start asking for a
  context switch and unmasking; the idle task sleeping; the comparator's flag raised at
  the deadline armed. Eighteen mutants seen only on a board die so (12 to 19, 89, 222,
  241 to 248).
- Memory from OSMalloc is filled with a small count in each word, rather than one byte.
  Under that fill, two forgotten indices no longer read equal, and a forgotten size no
  longer reads huge (338, 340, 348, 370). It hid two others: a forgotten narrow field
  read the 0 of a small count's upper bytes as if set (46, 385). No fill sees all. The
  IPC tests run under both, and 0xA5 stays the default.
- Queues of 2 to 4 nodes are taken across the wrap of their indices from 12 phases (250
  to 254, 330, 331), and a slot of one byte is read by reference (460).

Two mutants of the 3-slot buffer's table, 413 and 416, have the writer take the slot
being read. They still survive a reference held from every state. The models and the
litmus tests catch them; `mutants.py` does not.

Since the same night, `tools/differential.py` also draws an event-driven task signalled
at the edges: a signal just before the wraparound and a second within its workload, a
signal at the very tick of the wrap, or one at time 0. Its first run failed on most
builds. Read with the trace, no failure was the kernel's:

- Its model gave the server's deadlines in the order of the signals. The kernel gives
  them in the order of the releases: a signal before its task's last deadline is
  released at that deadline, after later signals of other tasks.
- It counted signals that the kernel coalesces: one kept pending per event, the rest
  dropped (TestSignals). Sets that may coalesce are now left out.
- A ninth task, past the host's eight, was dropped unseen. The host now refuses it
  aloud.

2,000 sets a build agree.

### 2026-10-07: 9d9157c, and the test of optional instances

Run whole on 9d9157c, at 300 task sets a build, the hard kernel's mutants scored 86.4 %
(418 of 484), 95.4 % of the 438 not declared; the power-aware kernel's 81.5 % (651 of
799), 84.0 % of the 775 not declared; the soft kernel's 79.0 % (572 of 724). The run also
showed a fault of `tools/mutants.py`: a test that ran past its time limit was reported
killed, but only `make` was stopped, and three soft mutants (217, 218, 247) kept spinning
an hour and forty minutes after the campaign, their parent gone. Each step now runs in a
process group of its own, killed whole on the limit.

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
its own cost might win. The bench, now timing the count, stays. On the same tasks the
count took 13.8 µs in the
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

### 2026-10-10: every survivor read

Eight of the hard kernel's survivors had never been read (0, 127, 130, 206, 225, 226,
236, 459 of the run on 9d9157c); they were on 2026-10-10, each against a build of the
mutant, and six are killed since by a test that fails on it:

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
alike, 98, matched no survivor; they are ranked among the whole source's since. The
same fault chose the mutants to run: given a file of part of them, as its own
`-again.jsonl` is, `--survivors` ranked them alike among that part, and ran the soft
kernel's `totalWork += tmp` left out at line 619, killed long before, for the same left
out at line 646. A record whose number still names the same mutant is now taken by it.

The soft kernel's 152 survivors of 9d9157c were run again on the same day: the tests
added for the hard kernel killed 10, its code being shared, and the 142 left were read,
9 of them declared before. Each was set against its twin in the hard kernel, found by
the function it lies in and its rank there, and the reason of a twin declared taken
over only where the code is the same and the reason holds in the soft kernel's frame:
of 19 barriers, 9 are caught by `tools/check_order.py` on the ARM builds, among them
the one after a task's zombie state in `OSEndTask`, whose twin in the hard kernel keeps
its order without it; 5 leave the memory accesses, the calls and the barriers in the
same order on the eight ARM builds, compared one by one, and are declared; the others
move loads, register reuse or an inlining decision, which `check_order.py` does not
follow, and are written below. Then:

- 26 are killed by tests written for them: a deadline of 0 with a WCET of 0 (30); the
  soft kernel's edges of creation (`createevents`): a first arrival of 65,535 turns, the
  127 tasks a byte of DM priorities holds, an event-driven task with no utilization, a
  workload computed past 2^30, the shares of 1/256 and 255/256, under DM a WCET equal to
  its workload (49, 52, 378 to 381, 391, 401); the first instance numbered 0 for a k of
  255 and of 200, and mandatory with blocks of zeros (63, 65, 66); a first instance a
  turn and a remainder away (87, 468); a mandatory instance at the last tick before the
  counter wraps, which a bound of `>=` read a turn later, the test of another task's
  optional instance leaving it out (294, `firmatwrap`); and under DM the test's count of
  the event-driven tasks of higher priority, the earliest release and every workload
  after, the last in part above and below its WCET, an instance that would end late
  dropped and one that fits run (180 to 197, `firmeventsdm`); under EDF the share
  reserved for the events counted from their server's last deadline (202,
  `firmeventsedf`). The host's timed tasks now count the ends past the deadline of the
  instance the kernel elected, taken from the next arrival it had set then, which holds
  whatever was dropped and across the wrap of the counter.
- 78 are declared, the reason of each in `test/host/equivalent-mutants.jsonl`: ties and
  bounds no value reaches, guards against an overflow that WCET <= deadline <= period
  already rules out, a pre-test that `IsTaskSchedulable` makes again strictly, the idle
  task's priority that nothing reads, the next mandatory arrival of a task found at a
  shift with no turn left, which the same pass releases and recomputes before any test
  reads it, and the twins.
- 20 are behaviours no test reads. Barriers the compiler moves loads or registers
  across, or inlines differently without, which `check_order.py` does not follow, read
  by hand on the RP2040 build with no store crossing (111, 122, 124, 508, 540, 542,
  544); the wait-free queue's helpers finding their work done, which `test/model/fifo.py`
  explores (528, 530, 566, 568), and two entries of the 3-slot table the models and the
  litmus tests catch (655, 658), as in the hard kernel; the 4 bytes of the start's
  allocation (463); `STATE_DROP` left unset, an optimisation by its own comment, read
  again only by a re-entry between the mark and the removal (130); a reserve of 1/256
  for an event-driven task of WCET 0 (387); and under EDF the share reserved for events
  computed short, a utilization of 1/256 left out or a part of a tick (199, 203, 204,
  206): an optional instance is tested once the events due before it have run, and
  those released after get a later deadline than its own, so that it ends in time; what
  the share protects is the events' own deadlines, which no test sets dense enough to
  reach.

Run again with those tests, the 142 gave the 26 kills and 87 survivors declared: the
soft kernel's mutants score 84.0 % (608 of 724), 95.4 % of the 637 not declared.

The power-aware kernel's 148 survivors of 9d9157c, run again the same day, gave 5 kills;
the 143 left, 24 declared before, were read as the soft kernel's were, its only target,
the RP2040, compiled under each of its five builds for the barriers. Those it could not
settle by reading ran again at 2,000 task sets a build instead of 300 (`--sets`), which
killed two: EDF*'s arrival times left unshifted in the ready queue at the wraparound,
which broke a tie the wrong way in set 1200 of `pa_dra`. `tools/differential.py` checks
that set at every run since, beside its 300 (`KEPT`). Then:

- 2 more are killed by tests: a deadline of 0 with a WCET of 0 (39), and an event-driven
  task whose WCET is its workload, which the power-aware kernel takes (287); 4 barriers
  of the slot buffers are caught by `tools/check_order.py` (810 to 816);
- 84 are declared: the twins of the hard kernel's declared ones whose code and frame are
  the same; barriers that leave the memory accesses, the calls and the barriers in the
  same order on the five builds, among them the one after a task's zombie state in
  `OSEndTask`, which `check_order.py` needs on the Cortex-M33 for the hard kernel but the
  RP2040 does not; statics read only after a creation or a dispatch sets them; the
  arguments of a pair completed with `check`, which only completes the pair stored; the
  simulation brought up to date twice at one instant; the halves of a pair swapped,
  independent; an excess that is always 0 when an update begins; a slack of one tick,
  which the fastest speed absorbs at the RP2040's operating points; the flag that keeps
  the speed saved once per pass of the handler, set and cleared within it (107, first
  written as a speed saved stale that the speed reference missed, which it was not: the
  lines read were one off);
- 27 are behaviours no test reads, alive at 2,000 sets where they were run so: the
  handler and the idle task after it left at the speed before rather than the fastest,
  energy only, every task's dispatch setting its own (106); EDF*'s
  ties in the simulation queue after the wraparound, its zombies and the event-driven
  tasks waiting in the arrival queue left unshifted (129, 130, 140); a slack computed
  and not yet installed at the wraparound, credited with 2^30 (143, 144), which the tests
  that take the wrap at each point of an ending task reach without a task slow enough to
  show it; the time a slack runs out from, taken earlier, and the work of an event-driven
  task released at once kept from the instance before, both making the slack smaller
  (229, 362); the time of the last update left behind by `UpdateRemainingWork`, which
  every caller sets but on a second turn of the handler's loop (368); one tick of excess
  more or less at the first update of DRA's simulation (3, 4); the simulation's clock
  taken back when it ran ahead (409) and the path its own comment says only an
  interrupting call reaches (412, 413); the time of an ending task's dispatch taken from
  the slack just installed rather than read again, the same on the host (96, 331); the
  wait-free queue's helpers, two entries of the 3-slot table and the 4 bytes of the
  start, as in the hard kernel (619, 621, 657, 659, 746, 749, 558); the priorities all
  shifted by one, as in the hard kernel (74); and the event FIFO's barriers the compiler
  inlines differently without (631, 633, 635).

Run again with those tests and the set kept, the 143 gave the 4 kills and 107 survivors
declared, 108 with 107 since: the power-aware kernel's mutants score 82.6 % (660 of 799),
95.5 % of the 691 not declared. The three kernels, on the code of 2026-10-10: hard
87.6 %, 97.0 % of those
not declared; soft 84.0 %, 95.4 %; power-aware 82.6 %, 95.5 %. Every survivor is read:
killed, declared with its reason, caught on the ARM builds by `tools/check_order.py`, or
written above.

Taken further the same day, at the user's request (roadmap, task 6). The barriers left
written settle two ways. `ScheduleNextTask` runs in a context marked not to be saved,
`OSEndTask` and `OSSuspendSynchronousTask` setting `_OSNoSaveContext` before they call
it: any interrupt that touches the ready queue ends that context, so only the order of
its stores reaches anyone, which `check_order.py` checks, and the loads its barriers
order do not matter (soft 111, 122, 124, declared). The event FIFO's posting of an
operation, the call of its helper, the clearing, and the descriptor written before the
posting keep their order on every build without each of its barriers, compared from the
disassembly with three faulty variants each caught on every build, a posting left out,
moved after the call, and a field of the descriptor written after it (soft 508, 540,
542, 544, power-aware 631, 633, 635, declared). And three tests:

- `slackatwrap`: under DM_SLACK a task ends one tick before the wrap, the wrap taken at
  each point of its end, a slack computed and not yet installed; an alarm at the
  handler's clear turns its loop once more, which credited the slack with 2^30 when its
  time was left unshifted, a task of lower priority then slowed to the slowest
  (power-aware 143, 144);
- `spuriousturn`, every build: an alarm with nothing due in every pass of the handler
  must change neither the schedule nor the speeds; it caught the time of the last update
  left behind by `UpdateRemainingWork`, the work run counted twice (power-aware 368);
- `firmeventsedf 2`: an event-driven task due before the optional instance, its share
  left out when the server's last deadline lay ahead (soft 203).

EDF*'s ties in the simulation queue after the wraparound (power-aware 129, 130, 140) ran
at 20,000 task sets a build: the first set that failed them, 19989 of `pa_dra`, failed
the kernel as well, a speed below the reference's, and killed nothing. Read as task 7,
the fault was the reference's: a periodic release and a signal at one instant, the
kernel's handler serves them in two passes on the host, an interrupt each, the first
bringing DRA's simulation up to date and counting the server's excess since the last
update, 2 ticks there, before the signal sets the server's deadline; the reference
served both in one pass, the signal first setting that deadline, and lost the excess,
a task with one tick of work left then due at the fastest speed where the kernel, rightly,
took the slack. The reference serves each signal in a pass of its own since, and
`tools/differential.py` keeps the set. They stay written, as soft 199, 204 and 206, a
share computed short by less
than a tick or at 1/256, which the reservation's own pessimism covers in the cases set
up. The scores then: hard 87.6 %, 97.0 % of those not declared; soft 84.1 % (609 of
724), 96.7 % of the 630 not declared; power-aware 83.0 % (663 of 799), 96.4 % of the 688
not declared.
