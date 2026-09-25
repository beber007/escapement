# Emulation

The kernel **runs** on an STM32F407VG emulated by [Renode](https://renode.io):

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
renode emulation/renode/escapement_f4.resc
(monitor) sysbus LogPeripheralAccess sysbus.gpioPortB true
(monitor) emulation RunFor "1"
```

The jobs that build and emulate run in an image of our own, `ci/Dockerfile`,
which holds the ARM toolchain, both versions of Renode and the RP2040 models, pinned:
they install nothing, and the apt mirrors of the runners, which once took 19 minutes
over the toolchain alone, stay out of the way. `.github/workflows/ci-image.yml` builds
it, by hand, under a tag both workflows name.

Three Robot suites replay such runs on every push, in the `emulation` job of the CI
(the RP2040 has its own job and suite, run on every kernel and algorithm, see
`emulation/renode/RP2040.md`), and the
`variants` job runs them again on the soft kernel and under deadline-monotonic
scheduling, built with `make KERNEL=SOFT` and `make SCHEDULER=...`:

```sh
pip install robotframework==6.1 robotframework-retryfailed psutil pyyaml
renode-test emulation/renode/escapement_f4.robot emulation/renode/escapement_f4_wrap.robot \
            emulation/renode/escapement_f4_events.robot
```

| Test | What it proves |
|---|---|
| The three periodic tasks are scheduled | each of the three tasks raises **and** lowers its output within its time window |
| The UART echo answers | the kernel also schedules interrupt-driven processing |
| Scheduling survives the 2^30 wrap | the kernel keeps scheduling across the wraparound of its clock |
| Timer events wake the event-driven tasks | a timer-event handler on TIM14 wakes event-driven tasks on time: PB13 high 8.40 ms out of every 41.00 ms, PB14 16.80 ms out of every 82.00 ms, within 2 % |

## Crossing the 2^30 boundary

The kernel counts time modulo 2^30 and shifts every temporal variable back when its
counter wraps. At the usual tick rate that happens once every eighteen minutes, which is
why the path had never been executed once in the life of this code.

Reaching it in a test takes a platform, not a trick. `escapement_f4_wrap.repl` clocks TIM2
82,000 times faster; the kernel still programs its prescaler of 81, so its counter ticks at
10 GHz and reaches the boundary after 107 ms. `TaskWrapF4.c` scales its periods by the
same factor, so the kernel carries the load it would have on hardware, with a counter that
happens to run fast.

Traced with `tools/trace_gpio.sh` on 2026-09-20, when the platform ran the counter at 1
GHz and the boundary came at 1.07 s, over 1.3 s: 2,167 pulses, which is exactly the
1,300 + 650 + 217 activations the three periods of 1, 2 and 6 ms called for, and 377 of
them fall after the boundary. Nothing is lost in the crossing. The clock was made ten
times faster the same day: at 1 GHz the test was starving a CI runner.

Neutralising the time shift in `_OSTimerIsOverflow` makes the test fail, and restoring
it makes it pass again.

## The first run

`TaskLEDF4` creates three periodic tasks of 100, 200 and 600 ticks, each
toggling one output of GPIOB. Over one emulated second:

| Output | Period | Toggles | Measured ratio | Theoretical ratio |
|---|---:|---:|---:|---:|
| PB13 | 100 | 1220 | 5.98 | 6.00 |
| PB14 | 200 | 610 | 2.99 | 3.00 |
| PB15 | 600 | 204 | 1.00 | 1.00 |

Deadline-driven scheduling honours the declared periods. This was the first
verified execution of the kernel since the project was taken over.

## The chronogram

The figure in the README is drawn from a trace, not by hand. The example drives
its outputs through the BSRR register of the STM32, one 32-bit write per edge: the pin
in the low half raises it, in the high half lowers it. A Renode watchpoint on that
register reports each write with the elapsed virtual time, which gives an exact
transition list.

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
tools/trace_gpio.sh > docs/data/f4-gpio-trace.csv
tools/chronogram.py docs/data/f4-gpio-trace.csv docs/images/f4-schedule.svg
```

The trace is kept in `docs/data/`, so the figure can be redrawn without running
the emulator at all. Note what the figure cannot show: the tasks of this example
execute for 0 to 26 us against periods of 820 us and more, so they never overlap
— there is no preemption to be seen here, only the regularity of the periods.

The trace was taken again on 2026-09-25. The first, of 2026-09-20, gave 0 to 29 us;
from 2026-09-21 the example wrote the 32-bit BSRR, which the script, still watching
16-bit writes at 0x18 and 0x1A, no longer saw: it captured nothing until it was
fixed.

## The Renode timer model had to be fixed

Without a fix, the kernel deadlocked on its own overload guard. The cause was
in `Timers.STM32_Timer`, in the `EventGeneration` register:

```csharp
.WithFlag(0, FieldMode.WriteOneToClear, writeCallback: (_, val) =>
{
    if(updateDisable.Value) { return; }   // <- val is never tested
    ...
    updateInterruptFlag = true;
}, name: "Update generation (UG)")
.WithTag("Capture/compare 1 generation (CC1G)", 1, 1)
```

The callback of the `UG` bit ran **whichever bit was written**. Writing `CC1G`
— which is what `_OSStartTimer` does to force its first compare interrupt —
therefore generated an *update* event and raised `UIF` instead of `CC1IF`. The
kernel read that as a counter overflow and shifted all its times by −2³⁰ at
`CNT = 1`, leaving every task permanently late.

Minimal reproducer, outside of any kernel:

| Sequence | Expected | Renode 1.17.0 |
|---|---|---|
| `DIER = 0`, `EGR <- 0x2` | `SR = 0x0` | `SR = 0x0` |
| `DIER = 3`, `EGR <- 0x2` | `SR = 0x2` (`CC1IF`) | `SR = 0x1` (`UIF`) |

`emulation/renode/Escapement_STM32_Timer.cs` is a copy of the original model
(MIT, Antmicro) with two fixes: the `UG` callback is guarded by a test on the
value written, and `CC1G` through `CC4G` are implemented. Renode compiles this
plugin on the fly, so there is nothing to rebuild. The CPU platform is derived
in `stm32f4_escapement_cpu.repl` — since Renode does not allow a node to be
redeclared, the file has to be copied to change the type of TIM2.

TIM14, which the timer events of the F4 use, runs on the fixed copy too: its driver
also forces a compare event through `CC1G`, a path this example happens not to take.
Both fixes stay in this copy, which the CI
loads: no test waits on a release of Renode
that carries them, and proposing them upstream, once planned, was dropped (2026-09-24).

QEMU was tried first (`-machine netduinoplus2`): the kernel starts but its
timer is never woken, and it takes two exceptions in 60 seconds.

## A platform of our own for the RP2350

Renode models no RP2350 (checked 2026-09-22), and the `rp2350Blinking` branch of
matgla/Renode_RP2040, begun in November 2024, stopped at a GPIO and a SIO before its
author froze the project. Nothing else covered it either: QEMU had an RFC for the RP2040
only (v3, September 2026, not merged; it models the timer, the clocks, VREG, the SIO and
the UART, and could replace the frozen models for the Pico once merged) and a feature
request for the RP2350, and Wokwi runs in the cloud, closed, with an RP2350 still
incomplete. `emulation/renode/escapement_pico2.repl` is therefore a platform of our own,
with only what the examples of the RP2350 port touch: the Cortex-M33 and its NVIC, which
Renode emulates, with 4 bits of priority as on the chip; the 520 KB of SRAM; the PL011
UART0, which Renode models; and two models written here, `Escapement_RP2350_Timer.cs`,
TIMER0 with the logic of the fixed RP2040 timer and the atomic aliases decoded by the
model itself, and `Escapement_RP2350_SIO.cs`, the GPIO outputs of the SIO driving the
LEDs the tests watch. The clocks, the crystal, the PLL, the resets, the pins and the
TICKS block are Python peripherals that keep what is written and read the bits the
start-up code waits for as set.

That last choice bounds what the suite shows. It proves that the kernel runs and
schedules on a Cortex-M33, with the timer, the UART and the GPIO of the RP2350 at their
addresses; it does not prove that the clocks are programmed right, since every switch
and every reset is acknowledged whatever was written. Only the board will say that.

Core 1 is a second Cortex-M33 with its own NVIC, halted at the start. The SIO model
answers each core with its own number and its end of the two inter-core FIFOs, and plays
the part of the bootrom in the launch, as the pico-sdk describes it: it announces core 1
with a 0, echoes each word, and on 0, 0, 1, vector table, stack pointer, entry point,
starts core 1 there. `FourSlotCoresPico2` then runs as on the board: bare code on core 1
writing into the 4-slot buffer and a plain array, a task on core 0 reading both. Renode
runs the two cores by turns rather than at once, finely enough for the plain array to
tear — 629 reads out of 6,368 in 200 ms on 2026-09-24 — while no read of the buffer was
torn or went backwards. That the interleavings are all covered is the model's to say
(`test/model/fourslot.py`, two cores); what the emulation adds is the kernel's code, as
compiled for the Cortex-M33, carrying it.

`escapement_pico2.robot` runs the checks of the RP2040 suite the examples allow — the 1
ms probe, the three periodic tasks, the UART echo, the timer events, and the crossing of
the 2^30 boundary of the kernel clock by `TaskWrapPico2`, the timer model raised to 1
GHz as on the RP2040 — and the 4-slot buffer between the cores (the 3-slot one since
2026-09-25, below), on the hard and the soft kernel under both algorithms, in the CI and
on a Mac: the platform needs no models to build, so Renode's portable package runs it as
it is. On 2026-09-24 the six tests passed under the four builds, and failed where they
should on code made faulty on purpose (below); on 2026-09-25 they passed again under the
hard EDF build, with the memory barriers the slot buffers now take between cores. The
faults: the UART enabled in the first word of the NVIC's registers, as the RP2040 driver
does for its interrupts below 32, failed the echo alone; the interrupt registers of the
timer at their RP2040 offsets failed the three tests that depend on them; an ALARM1 that
no longer signalled the wrap failed the wrap test alone; a 4-slot writer choosing the
reader's pair failed the test between the cores alone.

The 3-slot buffer between the cores (`ThreeSlotCoresPico2`) needed more. Its writer and
reader hand a slot over with LDREXB/STREXB, and Renode's exclusives are not those of the
RP2350 with ACTLR.EXTEXCLALL set. In Renode 1.17 a
STREX succeeds while its own core's reservation stands and the location still holds
what the LDREX read (`gen_store_exclusive` in tlib, `arch/arm/translate.c`, at the
commit Renode 1.17.0 pins); a plain store by the other core leaves the reservation
alone. On 2026-09-24, with core 0 holding a reservation some 98 % of the time, eleven
stores of the same value by core 1 made none of about 170 STREX fail, and one store of
another value made one fail. The model, given that monitor
(`test/model/threeslot.py`, `value_compare`), still finds a writer that takes the slot
being read, but not a writer that tries its SC once — nor would a real monitor, as long
as core 1 takes no interrupt between its LL and its SC. And where both cores touch a
reserved location, Renode slowed down about a thousandfold or stalled: of three runs of
the same image, one covered 150 ms in 40 s and two stalled within the first 50 ms, and
the test stalled in the suite for over ten minutes.

So the suite plays the monitor of the RP2350 itself (`rp2350_exclusive_monitor.py`,
2026-09-25). It hooks the entry of the byte and word LL and SC functions on both cores,
does the access and returns, so that no LDREX or STREX runs: one reservation per core
on a granule of 16 bytes, cleared by any write of the other core to it whatever the
value, by an exception on its own core, and by its SC. Under it the demo covers 200 ms
in about 8 s and does not stall. `The 3-slot buffer crosses between the two cores` now
runs under the four builds. On the hard kernel: 6,368 reads, none torn or going
backwards, the plain array torn 16 to 31 times, and 1,644 of the reader's SCs failed
because the writer had stored into the granule, which Renode's monitor never does.
The first version of the monitor, earlier the same day, counted some 1,060: its SC wrote
through the bus, which reaches no watchpoint, and left the other core's reservation in
place; only the plain stores cleared it. A writer that may take the slot being read
fails the test (24 and 27 reads torn), and a reader that tries its SC once stalls it.
What it does not catch are the races that need the other core between an LL and its
SC: monitors local to each core, and a writer that tries its SC once while one SC in
three of core 1 fails for no reason, both held for 200 ms without a torn read, with
Renode's usual slice and with one of 1 us, although thousands of writes of the other
core fell inside a reservation. Renode runs each core for a slice of time and seldom
switches inside those few instructions; the model (`test/model/threeslot.py`) covers
every interleaving, and the board will be the third witness.

The queue between the cores (`Escapement_CoreQueue.c`) runs the same way:
`FIFOCoresPico2` passes records from core 1 to a task on core 0 through one such queue
and gives their nodes back through another. At Renode's usual slice the two cores never
overlap in a queue — core 0 takes its records in a burst, core 1 refills the nodes in
its own turn — and the monitor clears no reservation. With a slice of 1 us they do: on
2026-09-25, over 50 ms, 1,600 records taken in order, none torn, and 366 SCs failed
because the other core had written in the granule. `The queue of Evéquoz crosses between
the two cores` sets that slice and asks for such failures on both cores. Each queue has
one producer and one consumer there; two of either are the model's (`fifo_mp.py`).

On one core, `IPCPico2` and `IPCPico` make tasks preempt one another inside the FIFO
queue and a 3-slot buffer: a task of period 5 ms fills the queue and writes the buffer
for 3 ms of each instance, a task of period 1 ms preempts it to put its own records and
read the buffer with `OS_READ_ONLY_ONCE`, and an event-driven task empties the queue.
The kernel's code that completes an operation left posted by the task it preempted runs
only then; the suites hook the queue's helpers and count those entered for a descriptor
on the preempted task's stack. On 2026-09-25, over 50 ms, 660 records went through the
queue in order and 40 slots were read, none twice nor torn, under each of the four
builds of the Pico 2, the helpers completing 7 to 22 operations of another task, and
under the seven builds of the Pico run in the CI's image (hard and soft under both
algorithms, the power-aware kernel under OTE, DRA and DM_SLACK), 35 to 48. The hooks
slow the test to 16 to 36 s.

The endurance test's firmware (`SoakPico`, `SoakPico2`, `rp2040.md`) runs in both suites
until it has counted two seconds: every part active, none in error, the pulse held to
the seconds the firmware counted, since its start waits for core 1 to answer its launch,
which under Renode takes a part of the run that varies with the host (from 0 to over
1.5 s seen on the Pico 2). Under the RP2040 models core 1 cannot be launched, and the
suite has the firmware skip the part between the cores. On the Pico 2 the test first
stopped on the kernel's overload check in 1 to 4 runs of 6, only with core 1 running:
not Renode's exclusives, as first thought, but a task wiped from the stack while still
in the ready queue, the order of two stores in `OSEndTask` left to the compiler
(`method.md`); core 1 only moved the interrupts onto the instruction between them. `tools/soak_emulated.sh` runs it for
long, several instances side by side on a Linux machine, each with its own build and a
seed that sets the Filler's work and the delay of the timer events
(`soak_emulated.robot`); the models run close to real time, 15 s of virtual time in
20 s. It is how the soft kernel under deadline-monotonic scheduling was seen to hang
there, on 2026-09-25, when an interrupt had joined the firmware (`method.md`).

## A platform of our own for the STM32U5

Renode models no STM32U5 either (checked 2026-09-25: no platform in 1.17.0, nor in the
repositories of renode). It models the STM32L552, of the same family: the same
Cortex-M33, and TIM2, TIM5, USART1 and the ports of GPIO at the same addresses.
`emulation/renode/escapement_u5.repl` takes those models, with the 2 MB of flash and the
768 KB of SRAM of the STM32U575 and the fixed timer model of the F4 platform
(`Escapement_STM32_Timer.cs`), whose CC1G the event manager uses as the F4's does. The
RCC and PWR of the U5 are other blocks at other addresses than the L5's: they are Python
peripherals that keep what is written and read as set the bits the clock set-up waits
for, the voltage range and the booster ready, PLL1 locked, the switch of the system clock
acknowledged. The same bound as for the RP2350 follows: the suite says that the kernel
runs and schedules on the timers, the USART and the GPIO of the chip, not that the
clocks are programmed right.

`escapement_u5.robot` runs the checks of `escapement_pico2.robot` that do not need a
second core. For the 2^30 wrap, the timer has to be built 1000 times faster
(`escapement_u5_wrap.repl`): set at run time, the frequency reaches the counter but not
the compare channels of the model, which keep the rate they were built with, and the
tasks are then never woken. Under the four builds of the port the suite passed 6 tests
of 6 on 2026-09-25, the first build as written, with no change to the port.
