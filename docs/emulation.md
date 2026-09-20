# Emulation

The kernel **runs** on an STM32F407VG emulated by [Renode](https://renode.io):

```sh
cd Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery && make bin && cd -
renode emulation/renode/escapement_f4.resc
(monitor) sysbus LogPeripheralAccess sysbus.gpioPortB true
(monitor) emulation RunFor "1"
```

Two Robot tests replay that run on every push, in the `emulation` job of the CI:

```sh
pip install robotframework==6.1 robotframework-retryfailed psutil pyyaml
renode-test emulation/renode/escapement_f4.robot
```

| Test | What it proves |
|---|---|
| The three periodic tasks are scheduled | each of the three tasks raises **and** lowers its output within its time window |
| The UART echo answers | the kernel also schedules interrupt-driven processing |

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
