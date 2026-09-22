# Emulation

The kernel **runs** on an STM32F407VG emulated by [Renode](https://renode.io):

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
renode emulation/renode/escapement_f4.resc
(monitor) sysbus LogPeripheralAccess sysbus.gpioPortB true
(monitor) emulation RunFor "1"
```

Four Robot suites replay such runs on every push, in the `emulation` job of the CI
(the RP2040 has its own job and suite, run on every kernel and algorithm, see
`emulation/renode/RP2040.md`), and the
`variants` job runs them again on the soft kernel and under deadline-monotonic
scheduling, built with `make KERNEL=SOFT` and `make SCHEDULER=...`:

```sh
pip install robotframework==6.1 robotframework-retryfailed psutil pyyaml
renode-test emulation/renode/escapement_f4.robot emulation/renode/escapement_f4_wrap.robot \
            emulation/renode/escapement_l1_pa.robot emulation/renode/escapement_f4_events.robot
```

| Test | What it proves |
|---|---|
| The three periodic tasks are scheduled | each of the three tasks raises **and** lowers its output within its time window |
| The UART echo answers | the kernel also schedules interrupt-driven processing |
| Scheduling survives the 2^30 wrap | the kernel keeps scheduling across the wraparound of its clock |
| The power-aware variant schedules its three tasks | the DVFS variant schedules and drives the PLL |
| Timer events wake the event-driven tasks | a timer-event handler on TIM14 wakes event-driven tasks on time: PB13 high 8.40 ms out of every 41.00 ms, PB14 16.80 ms out of every 82.00 ms, within 2 % |

## Crossing the 2^30 boundary

The kernel counts time modulo 2^30 and shifts every temporal variable back when its
counter wraps. At the usual tick rate that happens once every eighteen minutes, which is
why the path had never been executed once in the life of this code.

Reaching it in a test takes a platform, not a trick. `escapement_f4_wrap.repl` clocks TIM2
8200 times faster; the kernel still programs its prescaler of 81, so its counter ticks at
1 GHz and reaches the boundary after 1.07 s. `TaskWrapF4.c` scales its periods by the same
factor — 1, 2 and 6 ms — so the kernel carries the load it would have on hardware, with a
counter that happens to run fast.

Traced with `tools/trace_gpio.sh` over 1.3 s: 2,167 pulses, which is exactly the 1,300 +
650 + 217 activations the three periods call for, and 377 of them fall after the boundary.
Nothing is lost in the crossing.

The test earns its place: neutralising the time shift in `_OSTimerIsOverflow` makes it
fail, and restoring it makes it pass again.

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

The figure in the README is drawn from a trace, not by hand. The kernel drives
its outputs through the BSRR register of the STM32: writing a bit at offset 0x18
raises a pin, writing it at 0x1A lowers it. Two Renode watchpoints report those
writes along with the elapsed virtual time, which gives an exact transition list.

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
tools/trace_gpio.sh > docs/data/f4-gpio-trace.csv
tools/chronogram.py docs/data/f4-gpio-trace.csv docs/images/f4-schedule.svg
```

The trace is kept in `docs/data/`, so the figure can be redrawn without running
the emulator at all. Note what the figure cannot show: the tasks of this example
execute for 0 to 29 us against periods of 820 us and more, so they never overlap
— there is no preemption to be seen here, only the regularity of the periods.

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

**Both fixes are to be proposed upstream to Antmicro.**

QEMU was tried first (`-machine netduinoplus2`): the kernel starts but its
timer is never woken, and it takes two exceptions in 60 seconds.
