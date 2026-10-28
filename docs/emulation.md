# Emulation

The kernel runs under [Renode](https://renode.io) on the four chips it targets. Each
chip has its own suite of Robot Framework tests. The CI replays these suites on each
push to `main` and each pull request, under every kernel and algorithm the port builds.
Emulation shows that the kernel schedules and drives the registers in the right order.
It says nothing of energy: the board and an instrument decide that.

| Chip | Platform | Renode | Suite, tests on 2026-10-11 | Builds in the CI |
|---|---|---|---|---:|
| RP2040 (Pico) | matgla/Renode_RP2040, with a fixed timer | 1.16.1, `linux-dotnet` | `escapement_pico.robot`, 12 | 11 |
| RP2350 (Pico 2) | our own, `escapement_pico2.repl` | 1.17.0, portable | `escapement_pico2.robot`, 13 | 4, and 1 under GCC 14.2 |
| STM32U585 (UNO Q) | our own, `escapement_u5.repl` | 1.17.0, portable | `escapement_u5.robot`, 24 | 4 |
| STM32U385 (NUCLEO-U385RG-Q) | our own, `escapement_u3.repl`, which checks the clock set-up and its replay after Stop 2 against RM0487 (`stm32u3.md`) | 1.17.0, portable | `escapement_u3.robot`, 30, those tagged `stop2` also at 48, 24 and 12 MHz, the whole on a `KERNEL=PA` build with `--variable KERNEL:PA` | 6 |

The RP2350, STM32U5 and STM32U3 suites run on a Mac as well as in the CI:

```sh
make -C Escapement/CORTEX-Mx/RP2350/Examples/pico2
renode-test emulation/renode/escapement_pico2.robot
make -C Escapement/CORTEX-Mx/STM32U5/Examples/uno-q
renode-test emulation/renode/escapement_u5.robot
```

The RP2040 suite needs models built against Renode's assemblies. The portable package
for macOS does not expose these assemblies. `emulation/renode/RP2040.md` has the recipe.

The jobs that build and emulate run in an image of our own, `ci/Dockerfile`. Since its
third tag (2026-09-26), the image holds two ARM toolchains. GCC 16.2 from Homebrew is
the one every job uses by default. Ubuntu's GCC 14.2 serves two jobs. One job checks the
compiled order of every variant. The other job runs the Pico 2 suite on its images. The
image also holds both versions of Renode and the RP2040 models, all pinned. The jobs
install nothing, so the runners' apt mirrors no longer hold them up. Those mirrors once
took 19 minutes over the toolchain alone. `.github/workflows/ci-image.yml` builds the
image, by hand only, under a tag that both workflows name.

## The STM32F4, the first target

On 2026-09-20, the kernel ran for the first time since the project took ZottaOS over. It
ran on an STM32F407VG, emulated by Renode's own platform. `TaskLEDF4` had three tasks of
100, 200 and 600 ticks. In an emulated second, the tasks toggled their outputs 1220, 610
and 204 times. The ratios were 5.98, 2.99 and 1.00, against 6, 3 and 1. The F4 suites
checked the three periodic tasks, the UART echo, timer events on TIM14 and the crossing
of the 2^30 boundary under every kernel. They also drew the chronogram that the README
showed.

The F4 stayed for the Cortex-M3/M4 path of the context switch, after the other STM32
families had been removed. Once the RP2350 and the STM32U5 took that path, and the U5
ran on its board with a check of each commit, the F4 was removed on 2026-09-26
(9783ab4). The git history keeps it.

## Crossing the 2^30 boundary

The kernel counts time modulo 2^30. When its counter wraps, the kernel shifts every
temporal variable back. At a 1 µs tick, the wrap happens once every eighteen minutes.
Until these tests, that path had never been executed in the life of this code.

A test reaches the wrap with a timer that runs faster than on the board. On the Pico and
the Pico 2, the timer runs at 1 GHz, set by the suite. On the U5, the timer runs a
thousand times faster, set by `escapement_u5_wrap.repl`. An example, `TaskWrap*`, scales
its periods by the same factor. The kernel then carries the load it would have on
hardware, with a counter that happens to run fast.

On the F4, a trace ran over 1.3 s with the boundary at 1.07 s (2026-09-20). The trace
counted 2,167 pulses. That is exactly the 1,300 + 650 + 217 activations that the three
periods called for. Of these pulses, 377 came after the boundary. With the time shift in
`_OSTimerIsOverflow` neutralised, the test failed. With the shift restored, the test
passed again.

## The Renode timer model had to be fixed

On the F4, without a fix, the kernel stopped on its own overload guard. The cause was in
`Timers.STM32_Timer`, in the `EventGeneration` register:

```csharp
.WithFlag(0, FieldMode.WriteOneToClear, writeCallback: (_, val) =>
{
    if(updateDisable.Value) { return; }   // <- val is never tested
    ...
    updateInterruptFlag = true;
}, name: "Update generation (UG)")
.WithTag("Capture/compare 1 generation (CC1G)", 1, 1)
```

The callback of the `UG` bit ran **whichever bit was written**. `_OSStartTimer` writes
`CC1G` to force its first compare interrupt. That write generated an *update* event
instead, and raised `UIF` rather than `CC1IF`. The kernel read it as a counter overflow.
The kernel then shifted all its times by −2^30 at `CNT = 1`, which left every task
permanently late.

A minimal reproducer shows the defect outside any kernel:

| Sequence | Expected | Renode 1.17.0 |
|---|---|---|
| `DIER = 0`, `EGR <- 0x2` | `SR = 0x0` | `SR = 0x0` |
| `DIER = 3`, `EGR <- 0x2` | `SR = 0x2` (`CC1IF`) | `SR = 0x1` (`UIF`) |

`emulation/renode/Escapement_STM32_Timer.cs` is a copy of the original model (MIT,
Antmicro) with two fixes: a test on the value written guards the `UG` callback, and
`CC1G` through `CC4G` are implemented. Renode compiles this plugin on the fly, so there
is nothing to rebuild. The F4's platform was derived from Renode's platform to change
the type of TIM2 and TIM14. The STM32U5's platform, our own, uses the copy for TIM2,
TIM3 and TIM5. The STM32U3's platform uses it for TIM2, TIM3 and TIM4. Both fixes stay
in this copy, which the CI loads, so no test waits for a Renode release that carries
them. Proposing the fixes upstream was dropped on 2026-09-24. It was taken up again on
2026-10-02. The defects were still in Renode's `master`, and the sequence above
reproduced them on 1.17.0. See renode/renode#1023.

QEMU was tried first (`-machine netduinoplus2`). The kernel started, but its timer never
woke it. The kernel took two exceptions in 60 seconds.

## A platform of our own for the RP2350

Renode models no RP2350 (checked 2026-09-22). The `rp2350Blinking` branch of
matgla/Renode_RP2040 was begun in November 2024. It stopped at a GPIO and a SIO before
its author froze the project. Nothing else covered the chip either. QEMU had an RFC for
the RP2040 only (v3, September 2026, not merged). That RFC models the timer, the clocks,
VREG, the SIO and the UART. Once merged, it could replace the frozen models for the
Pico. QEMU also had a feature request for the RP2350. Wokwi runs in the cloud and is
closed. Its RP2350 was still incomplete.

`emulation/renode/escapement_pico2.repl` is therefore a platform of our own. It has only
what the examples of the RP2350 port touch:

- the Cortex-M33 and its NVIC, which Renode emulates, with 4 bits of priority as on the
  chip;
- the 520 KB of SRAM;
- the PL011 UART0, which Renode models;
- two models written here: `Escapement_RP2350_Timer.cs`, for TIMER0, with the logic of
  the fixed RP2040 timer and the atomic aliases decoded by the model itself, and
  `Escapement_RP2350_SIO.cs`, for the GPIO outputs of the SIO, which drive the LEDs the
  tests watch;
- Python peripherals for the clocks, the crystal, the PLL, the resets, the power-on
  state machine, the pins, the watchdog and the TICKS block. They keep what is written,
  and they read as set the bits that the start-up code waits for.

That last choice bounds what the suite shows. The suite proves that the kernel runs and
schedules on a Cortex-M33, with the timer, the UART and the GPIO of the RP2350 at their
addresses. It does not prove that the clocks are programmed right: every switch and
every reset is acknowledged, whatever was written. Only the board can say that. The
board said it on 2026-09-28 (`architecture.md`). The board also showed what the platform
leaves out. After a debugger's reset, core 1's bootrom waits for core 0 to seed its
redundancy coprocessor before it answers the launch. On the platform, nothing waits
(`tools/rp2350_rcp_seed.S`).

Core 1 is a second Cortex-M33 with its own NVIC, halted at the start. The SIO model
answers each core with its own number and its end of the two inter-core FIFOs. The SIO
model also emulates the bootrom's part in the launch, as the pico-sdk describes it. It
announces core 1 with a 0 and echoes each word. On the sequence 0, 0, 1, vector table,
stack pointer, entry point, it starts core 1 there. `FourSlotCoresPico2` then runs as on
the board. Bare code on core 1 writes into the 4-slot buffer and a plain array, and a
task on core 0 reads both. Renode runs the two cores in slices of time. The slices are
fine enough for the plain array to tear: 629 reads out of 6,368 in 200 ms on 2026-09-24.
No read of the buffer was torn or went backwards. It is for the model to say whether all
interleavings are covered (`test/model/fourslot.py`, two cores). The emulation adds the
kernel's own code, as compiled for the Cortex-M33, carrying the mechanism.

`escapement_pico2.robot` runs the checks of the RP2040 suite that the examples allow:

- the 1 ms probe;
- the three periodic tasks;
- the UART echo, and two tasks sending on the UART at once;
- the timer events;
- the crossing of the 2^30 boundary by `TaskWrapPico2`, with the timer model raised to 1
  GHz as on the RP2040.

The suite adds the 4-slot buffer between the cores, and since 2026-09-25 the 3-slot one
(below). It runs on the hard and the soft kernel under both algorithms, in the CI and on
a Mac: the platform needs no models to build, so Renode's portable package runs it as it
is.

On 2026-09-24, the six tests that existed then passed under the four builds. They failed
where they should on code made faulty on purpose:

- the UART enabled in the first word of the NVIC's registers failed the echo alone (the
  RP2040 driver does that for its interrupts below 32);
- the timer's interrupt registers at their RP2040 offsets failed the three tests that
  depend on them;
- an ALARM1 that no longer signalled the wrap failed the wrap test alone;
- a 4-slot writer choosing the reader's pair failed the test between the cores alone.

On 2026-09-25, the tests passed again under the hard EDF build, with the memory barriers
that the slot buffers now take between cores.

### The 3-slot buffer and Renode's exclusive monitor

The 3-slot buffer between the cores (`ThreeSlotCoresPico2`) needed more. Its writer and
reader hand a slot over with LDREXB/STREXB. Renode's exclusives differ from those of the
RP2350 with ACTLR.EXTEXCLALL set. In Renode 1.17, a STREX succeeds while its own core's
reservation stands and the location still holds what the LDREX read
(`gen_store_exclusive` in tlib, `arch/arm/translate.c`, at the commit that Renode 1.17.0
pins). A plain store by the other core leaves the reservation alone. On 2026-09-24, core
0 held a reservation some 98 % of the time. Eleven stores of the same value by core 1
made none of about 170 STREX fail. One store of another value made one fail.

Given that monitor, the model (`test/model/threeslot.py`, `value_compare`) still finds a
writer that takes the slot being read. The model does not find a writer that tries its
SC only once. Neither would a real monitor, as long as core 1 takes no interrupt between
its LL and its SC. Where both cores touched a reserved location, Renode also slowed down
about a thousandfold or stalled. Of three runs of the same image, one covered 150 ms in
40 s and two stalled within the first 50 ms. In the suite, the test stalled for over ten
minutes.

The suite therefore emulates the RP2350's monitor itself (`rp2350_exclusive_monitor.py`,
2026-09-25). It hooks the entry of the byte and word LL and SC functions on both cores.
It performs the access and returns, so that no LDREX or STREX runs. Each core holds one
reservation on a granule of 16 bytes. Any write of the other core to that granule clears
the reservation, whatever the value. An exception on the core that holds the reservation
also clears it, and so does that core's own SC. Under this monitor, the demo covers 200
ms in about 8 s and does not stall. `The 3-slot buffer crosses between the two cores`
runs under the four builds.

On the hard kernel, the test gives 6,368 reads, none torn or going backwards. The plain
array is torn 16 to 31 times. Of the reader's SCs, 1,644 failed because the writer had
stored into the granule. Renode's own monitor never makes an SC fail that way. The first
version of the emulated monitor, earlier the same day, counted some 1,060. Its SC wrote
through the bus, which reaches no watchpoint. It also left the other core's reservation
in place, so only the plain stores cleared that reservation. A writer that may take the
slot being read fails the test (24 and 27 reads torn). A reader that tries its SC once
stalls the test.

The test does not catch races that need the other core between an LL and its SC. Two
faulty variants held for 200 ms without a torn read. One variant had monitors local to
each core. The other had a writer that tries its SC once, while one SC in three of core
1 fails for no reason. The variants held with Renode's usual slice and with one of 1 µs,
although thousands of writes of the other core fell inside a reservation. Renode runs
each core for a slice of time, and seldom switches inside those few instructions. The
model (`test/model/threeslot.py`) covers every interleaving, and the board will be the
third witness.

### The queue between the cores

The queue between the cores (`Escapement_CoreQueue.c`) runs the same way. In
`FIFOCoresPico2`, each core is a producer and a consumer of the same two queues, as in
the model (`fifo_mp.py`). Each core writes records into free nodes and appends them to
one queue. The core takes records from that queue, whoever wrote them, and gives their
nodes back through the other queue. At Renode's usual slice, the two cores never overlap
in a queue, and the monitor clears no reservation. `The queue of Evéquoz crosses between
the two cores` therefore sets a slice of 1 µs, at which the cores do overlap. The test
asks for SCs failed through the other core's stores, on both cores. On 2026-09-25, with
one producer and one consumer per queue, 1,600 records came through in 50 ms. They came
in order and none was torn. Over that run, 366 SCs failed that way.

On 2026-09-26, with two producers and two consumers per queue, some records were first
taken twice: 1,004 taken of 1,000 written, at that slice only. The emulated monitor was
at fault, not the queue. The hooks of the two cores can run at once. Two SCs on one
granule both found their reservation, and both wrote. Under a lock, as the chip's global
monitor serialises SCs, each of the 500 records of each producer is taken once, whole,
and in its producer's order. This holds under the four builds. The counts show it, and
so do the sums of the counters and of their squares.

### Litmus tests between the cores

`LitmusPico2` runs store buffering, message passing and load buffering between the two
cores, each with and without a DMB (`architecture.md`). Renode runs each core in program
order, so no round may end in a weak outcome: the test checks the harness, and the board
must try the chip. At a slice of 1 µs, both cores go first in every test. On 2026-09-28,
the six tests ran some 6,100 rounds each in 50 ms of emulated time. No round was weak.

### Preemption inside the queue, on one core

`IPCPico2` and `IPCPico` make tasks preempt one another inside the FIFO queue and a
3-slot buffer. A task of period 5 ms fills the queue and writes the buffer for 3 ms of
each instance. A task of period 1 ms preempts it to put its own records and to read the
buffer with `OS_READ_ONLY_ONCE`. An event-driven task empties the queue. Some of the
kernel's code completes an operation left posted by the preempted task. That code runs
only in this case. The suites hook the queue's helpers. They count the helpers entered
for a descriptor on the preempted task's stack.

On 2026-09-25, over 50 ms, 660 records went through the queue in order, and 40 slots
were read, none twice and none torn. That held under each of the four builds of the Pico
2, with the helpers completing 7 to 22 operations of another task. It also held under
the seven builds of the Pico then run in the CI's image, with the helpers completing 35
to 48 operations. Those seven builds were the hard and the soft kernel under both
algorithms, and the power-aware kernel under OTE, DRA and DM_SLACK. The hooks slow the
test to 16 to 36 s.

### The endurance test

The endurance test's firmware (`SoakPico`, `SoakPico2`, `rp2040.md`) runs in both suites
until it has counted two seconds, with every part active and none in error. The pulse is
held to the seconds that the firmware counted, since the firmware's start waits for core
1 to answer its launch. Under Renode, that wait takes a part of the run that varies with
the host: from 0 to over 1.5 s, seen on the Pico 2. Under the RP2040 models, core 1
cannot be launched, and the suite has the firmware skip the part between the cores.

On the Pico 2, the test first stopped on the kernel's overload check in 1 to 4 runs of
6, and only with core 1 running. The cause was not Renode's exclusives, as first
thought. It was a task wiped from the stack while still in the ready queue: the order of
two stores in `OSEndTask` had been left to the compiler (`method.md`). Core 1 only moved
the interrupts onto the instruction between those two stores.

`tools/soak_emulated.sh` runs the test for a long time. It runs several instances side
by side on a Linux machine. Each instance has its own build and a seed. The seed sets
the Filler's work and the delay of the timer events (`soak_emulated.robot`). The models
run close to real time: 15 s of virtual time in 20 s. That is how, on 2026-09-25, the
soft kernel under deadline-monotonic scheduling was seen to hang there, once an
interrupt had joined the firmware (`method.md`).

The first run meant to last a day started on 2026-10-01 with four instances. It ended
after 41 min to 1 h 47 min of virtual time, with every count of the firmware at 0
errors: Renode stopped on "Array dimensions exceeded" in `Machine.AppendDirtyAddresses`,
and the machine's memory was gone. Renode 1.16.1 adds each address that a core writes to
a list kept for every other core. Each other core takes its list when it runs, to flush
the code it translated from there. Core 1, left halted, never took its own list. After 5
s of virtual time, core 1's list held some 2 million addresses, and core 0's list a few
dozen. The robot empties that list after every interval (`drop_halted_core_dirty.py`).

With that fix, the run was made again from 2026-10-02 at some 07:05, at 1b67f30, on
pc-bertrand. It ran the hard, the soft, the deadline-monotonic and the power-aware
kernel under DRA, one instance each. All four ran their day of virtual time, 86,400 s,
across 80 wraps of the kernel clock. They ended on 2026-10-04 after 53 to 61 hours, with
every count of the firmware at 0 errors, up to the last of 1,440 readings. The pulse was
at most 100 µs late under deadline-monotonic scheduling, and on time under the three
others. The timer events were at most 90 to 92 µs late, and 190 under DRA. Some 255 KB
of stack were never used. The status `emulation/soak` of 1b67f30 says so.

## A platform of our own for the STM32U5

Renode models no STM32U5 either (checked 2026-09-25: no platform in 1.17.0, nor in the
repositories of renode). Renode does model the STM32L552, of the same family. The L552
has the same Cortex-M33, and TIM2, TIM5, USART1 and the GPIO ports at the same
addresses. `emulation/renode/escapement_u5.repl` takes those models, with the 2 MB of
flash and 768 KB of SRAM of the STM32U585, and with the fixed timer model
(`Escapement_STM32_Timer.cs`). The event manager uses the CC1G of that model, as the
F4's event manager did. The U5's RCC and PWR are other blocks, at other addresses than
the L5's. They are Python peripherals that keep what is written and read as set the bits
that the clock set-up waits for: the voltage range and the booster ready, PLL1 locked,
the switch of the system clock acknowledged. The same bound as for the RP2350 follows.
The suite shows that the kernel runs and schedules on the timers, the USART and the GPIO
of the chip. It does not show that the clocks are programmed right.

`escapement_u5.robot` runs the checks of `escapement_pico2.robot` that do not need a
second core. For the 2^30 wrap, the timer has to be built 1000 times faster
(`escapement_u5_wrap.repl`). If the frequency is set at run time, it reaches the counter
but not the model's compare channels. Those channels keep the rate they were built with,
and the tasks are then never woken.

Under the four builds of the port, the suite passed 6 tests of 6 on 2026-09-25. It
passed on the first build, with the port as written and no change to it. The endurance
test was added the same day. It made the count 7 of 7 once the port routed every
interrupt of the chip to the kernel's dispatcher (`stm32u5.md`). For that test, the
platform gained TIM3 and Renode's model of the independent watchdog. The platform gained
LPUART1 on 2026-09-26. On 2026-09-27, LPTIM1 (Renode's STM32L0 model) was added. Three
tests now cover LPTIM1 on the 32.768 kHz crystal and the idle task sleeping on it,
across the 2^30 wrap too. That makes ten (`stm32u5.md`). The eleventh test, the same
day, runs the endurance test built for the NUCLEO-U575ZI-Q and reads its reports on
USART1. Built to report on LPUART1 as on the UNO Q, the image fails that test. The
twelfth test, on 2026-09-29, has the idle task sleep in Stop 2 while an event-driven
task's arrival lies beyond the wrap (`Stop2EventWrapU5`). Before the fix that this test
found, TIM2 lost 1.5 ms of the period across the wrap (`stm32u5.md`). The tests added
since cover:

- the wake-up from Stop 2 with and without the HSE;
- LPUART1 through Stop 2;
- `SleepU5` run from the NUCLEO's flash;
- the erratum of the MSI PLL.

On 2026-10-05, the suite had 23 tests. The platform does not model MSPLIM, so
`StackGuardU5` runs on the board only.
