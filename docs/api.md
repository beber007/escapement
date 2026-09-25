# Writing an application

This guide goes from an empty `main` to tasks that talk to each other and to
interrupts. It covers what the three kernels share and where they differ; the
headers `Escapement/EscapementHard.h`, `EscapementSoft.h` and `EscapementHardPA.h`
remain the reference for every parameter, and the examples under
`Escapement/CORTEX-Mx/*/Examples` show each call at work.

## Choosing a kernel

| Kernel | Header | Build on the Pico | For |
|---|---|---|---|
| Hard | `EscapementHard.h` | `make` | every deadline met |
| Soft | `EscapementSoft.h` | `make KERNEL=SOFT` | (m,k)-firm tasks: m deadlines met in any k instances |
| Power-aware | `EscapementHardPA.h` | `make KERNEL=PA` | hard deadlines, the speed lowered whenever they allow |

Each schedules by earliest deadline first (EDF) or deadline-monotonic priorities
(DM), set by `SCHEDULER_REAL_TIME_MODE` in the example's `Escapement_Config.h`
or by `make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING`. The power-aware kernel
adds a policy, `POWER_MANAGEMENT`: OTE by default, under either algorithm; DRA
and DR_OTE, which impose EDF*; DM_SLACK, which imposes DM (`make KERNEL=PA
POWER=DRA`). `power-aware.md` explains them, `rp2040.md` measures them.

An application includes `Escapement.h` alone: it picks the kernel's header from
the configuration.

## The shape of `main`

`main` initialises the processor and the application, creates the tasks, and
hands the processor to the kernel, which never gives it back. On the Pico:

```c
#include "Escapement.h"

static void Blink(void *argument);

int main(void)
{
  extern void (* const CortexMxVectorTable[])(void);
  VTOR = (UINT32)CortexMxVectorTable;   /* the image runs from SRAM */
  OSInitializeSystemClocks();           /* crystal, PLL, 125 MHz */
  /* application initialisation: pins, peripherals, queues, buffers */
  OSCreateTask(Blink,0,500000,500000,NULL);   /* hard kernel: every 0.5 s */
  return OSStartMultitasking(NULL,NULL);
}

static void Blink(void *argument)
{
  SIO_GPIO_OUT_XOR = 1u << 25;          /* the LED of the Pico */
  OSEndTask();
}
```

`VTOR` and `SIO_GPIO_OUT_XOR` are register definitions of the application's own
(`*(volatile UINT32 *)0xE000ED08` and `0xD000001C`); the examples define those
they use. The power-aware kernel also calls `OSInitProcessorSpeed()` after the
clocks.

`OSStartMultitasking(f, argument)` calls `f(argument)` just before scheduling
starts, the place to enable the application's interrupts or to signal a first
event. The locals of `main` do not survive it.

## Time

Times are counted in ticks of the kernel's timer: microseconds on the RP2040,
whose timer runs at 1 MHz; on the STM32, whatever `ESCAPEMENT_TIMER_PRESCALER`
makes of the timer clock. A period is given in two parts, `periodCycles` full
turns of 2^30 ticks and a remainder `periodOffset` below 2^30, so that it can
reach years; `periodCycles` is 0 for any period shorter than 2^30 ticks, some
eighteen minutes at 1 MHz. A deadline is at most the period.

## Periodic tasks

A task is a function that runs to the end and calls `OSEndTask()` last; the
kernel calls it again at every period. It must not wait: all tasks run on one
stack, a preempted task keeping its context beneath the task that preempted it,
so a task that looped waiting for another would wait for ever. A task that
needs something from another reads what is there and returns.

| Kernel | Creation |
|---|---|
| Hard | `OSCreateTask(task, periodCycles, periodOffset, deadline, argument)` |
| Soft | `OSCreateTask(task, wcet, periodCycles, periodOffset, deadline, m, k, startInstance, argument)` |
| Power-aware | `OSCreateTask(task, wcet, periodCycles, periodOffset, deadline, argument)` |

Every creation returns `FALSE` when memory runs out. The first instance of every
task arrives when the kernel starts; there is no offset.

**Soft kernel.** Out of any `k` consecutive instances, `m` must meet their
deadline; the others are optional, and run only if the kernel can still meet
every mandatory deadline — a test that reads the declared `wcet`, not the time a
task actually takes. `m = k` makes a task hard. `startInstance` staggers the
pattern of mandatory instances between tasks, and `OSGetTaskInstance()` tells a
task where in its pattern it stands. `wcet` may be 0 when every task has `m = k`.

**Power-aware kernel.** `wcet` is the worst-case execution time in ticks at the
fastest speed; the kernel slows the processor so that the work left fits before
the deadline, and trusts that figure. Understated, it lets the kernel pick a
speed too low for the task, which then misses its deadline. `FourSlotCoresPico`
declares 400 µs for a reader measured at 314 µs at most (`rp2040.md`): measure,
then add a margin. `OSSetMinimalProcessorSpeed(speed)`,
before `OSStartMultitasking`, keeps the processor above an operating point
(`OS_12MHZ_SPEED`, `OS_50MHZ_SPEED`, `OS_125MHZ_SPEED` on the Pico).

## Event-driven tasks

A task that runs when something happens rather than at fixed times waits on an
event descriptor. It ends with `OSSuspendSynchronousTask()` instead of
`OSEndTask()`, and runs again once the event is signalled:

```c
static void *Ready;

static void Producer(void *argument)
{
  /* ... */
  OSScheduleSuspendedTask(Ready);       /* wake the consumer */
  OSEndTask();
}

static void Consumer(void *argument)
{
  /* ... */
  OSSuspendSynchronousTask();
}

int main(void)
{
  /* ... */
  Ready = OSCreateEventDescriptor();
  OSCreateTask(Producer,0,10000,10000,NULL);
  OSCreateSynchronousTask(Consumer,2000,Ready,NULL);   /* hard kernel */
  return OSStartMultitasking(NULL,NULL);
}
```

A signal is remembered: sent while the task still runs, it restarts the task as
soon as it ends, so a task can signal itself to run again. Signals are sent from
tasks, interrupt handlers, or the function given to `OSStartMultitasking`, never
from `main` itself.

The `workload` of an event-driven task is its worst-case execution time divided
by the share of the processor set aside for it, which is also its deadline and
its minimum interarrival time: 2000 above for a task that must be done within
2 ms of its signal. The soft and power-aware kernels add a `wcet` before it and
an `aperiodicUtilization` after it, the share of the processor left to all
event-driven tasks in 256ths, used under EDF only; given a workload of 0, the
soft kernel computes it from the two:

| Kernel | Creation |
|---|---|
| Hard | `OSCreateSynchronousTask(task, workload, event, argument)` |
| Soft, power-aware | `OSCreateSynchronousTask(task, wcet, workload, aperiodicUtilization, event, argument)` |

**Timer events.** `Escapement_TimerEvent` signals an event after a delay:
`OSInitTimerEvent(nodes, priority, OS_IO_TIMER_2)` in `main`, then
`OSScheduleTimerEvent(event, delay, OS_IO_TIMER_2)` from a task, and
`OSUnScheduleTimerEvent` to take a pending one back. `TestTimerEventPico` raises
a pin every 5 ms and has an event-driven task lower it 1 ms later.

## Passing data between tasks

Nothing locks in Escapement, so nothing can be kept waiting by a preempted task.
Three mechanisms pass data without a lock, each for one kind of exchange. They
allocate their memory when created, in `main`.

**A queue, when every item counts.** `OSInitFIFOQueue(nodes, nodeSize)` creates
a queue and a pool of `nodes` buffers of `nodeSize` bytes. The producer takes a
buffer, fills it and enqueues it; the consumer dequeues it, uses it and gives it
back:

```c
void *node = OSGetFreeNodeFIFO(queue);        /* NULL when all are in use */
if (node != NULL) {
   /* fill it */
   OSEnqueueFIFO(queue, node, size);
}
/* ... elsewhere ... */
UINT16 size;
void *item = OSDequeueFIFO(queue, &size);     /* NULL when the queue is empty */
if (item != NULL) {
   /* use it */
   OSReleaseNodeFIFO(queue, item);
}
```

Any number of tasks and interrupt handlers may enqueue and dequeue. A buffer
goes back to the queue it came from.

**Slot buffers, when only the latest value counts.** A sensor read by an
interrupt and consumed by a task wants the newest reading, not a backlog.
`OSInitBuffer(size, type, event)` creates a buffer for one writer and one
reader, `OS_BUFFER_TYPE_4_SLOT` (Simpson's four slots, no atomic instruction)
or `OS_BUFFER_TYPE_3_SLOT` (less memory, an LL/SC pair). The writer calls
`OSWriteBuffer(buffer, data, n)`; once `size` bytes are in, the slot is
published and the next write starts a new one. The reader takes a copy with
`OSGetCopyBuffer(buffer, mode, copy)` or a pointer with
`OSGetReferenceBuffer(buffer, mode, &pointer)`; `OS_READ_ONLY_ONCE` returns each
slot once and 0 after it, `OS_READ_MULTIPLE` the latest slot every time. Given
an event, the buffer signals it at every published slot, which wakes an
event-driven reader.

**LL/SC, for lock-free code of the application's own.** `OSUINT8_LL` …
`OSINT32_SC` load a word and store it back only if nothing intervened. A
store-conditional also fails when an interrupt merely came in between, the
value unchanged: always retry it in a loop, never read a failure as a conflict.

All three assume one core, with exceptions for the slot buffers read with
`OS_READ_MULTIPLE` (`OS_READ_ONLY_ONCE` marks the slot read with an LL/SC pair, which
the Cortex-M0+ emulates for one core). The 4-slot buffer works between the two cores of
the Pico, as `FourSlotCoresPico` shows (`rp2040.md`), and of the Pico 2 under Renode;
there the 3-slot buffer should too, since the port makes `LDREX`/`STREX` see both cores
— its model says so, no board has shown it yet (`ThreeSlotCoresPico2`). Between the
cores the buffer takes no event: signalled from core 1, it would pend the kernel's
interrupt on core 1, where no kernel runs. The LL/SC pair of the Cortex-M0+ is emulated
with a reservation bit that only interrupts clear, and the queue's announced operation
assumes preemptions that nest.

**A queue between the cores, on the Pico 2.** `Escapement_CoreQueue.h` gives the RP2350 a
FIFO queue of pointers that any code on either core may use, task, handler or bare loop
on core 1:

```c
void *queue = OSInitCoreQueue(16);        /* in main; the length a power of 2 */
OSEnqueueCoreQueue(queue, node);          /* FALSE when full; node never NULL */
void *node = OSDequeueCoreQueue(queue);   /* NULL when empty */
```

It carries pointers and owns nothing: nodes go round through a second such queue, as
`FIFOCoresPico2` does. It signals no event, so a task on core 0 polls it. It is
lock-free, not wait-free: an operation retries while the other core keeps winning.

## Interrupts

The kernel owns the vector table and routes every peripheral interrupt through
a descriptor, a structure whose first field is the handler; the handler
receives the descriptor, so it can keep its state there rather than in globals:

```c
typedef struct ButtonDescriptor {
   void (*Handler)(struct ButtonDescriptor *);
   void *Pressed;                            /* the event to signal */
} ButtonDescriptor;

static void ButtonHandler(ButtonDescriptor *descriptor)
{
   /* clear the interrupt in the peripheral, or it fires again */
   OSScheduleSuspendedTask(descriptor->Pressed);
}

static ButtonDescriptor Button = {ButtonHandler, NULL};

/* in main: */
Button.Pressed = OSCreateEventDescriptor();
OSSetISRDescriptor(OS_IO_BANK0, &Button);    /* entries in Escapement_Interrupts.h */
```

The application then enables the interrupt in the peripheral and the NVIC, at
a priority the kernel leaves free: on the Cortex-M0+ of the Pico, levels 0 and
1, the kernel's timer taking one of them (`TIMER_PRIORITY`), SysTick and PendSV
the two lowest. A handler is short: it clears its source, moves data through a
queue or a slot buffer, and signals the task that does the rest.

## Memory

`OSMalloc(size)` allocates for good, before `OSStartMultitasking` only, from
`OSMALLOC_INTERNAL_HEAP_SIZE` bytes set in `Escapement_Config.h`; the queues,
buffers, events and tasks take theirs from it. The stack takes all the RAM left.
There is no `free` and no C library: the code is built freestanding.

## On the Pico

- **The image runs from SRAM**, loaded over SWD; there is no second-stage
  bootloader and nothing is written to the flash (`rp2040.md`):

  ```sh
  openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -f target/rp2040.cfg \
      -c init -c 'reset halt' -c 'load_image build/TaskLEDPico.elf' \
      -c 'resume 0x20000000' -c exit
  ```

  A program that runs core 1 must add `-c 'set USE_CORE 0'` before the target
  file (`tools/fourslot_cores.sh`).
- **The UART**: `OSInitUART(nodes, size, receiveHandler, OS_IO_UART0)`, then
  `OSGetFreeNodeUART` and `OSEnqueueUART` to send; `UARTEchoPico` echoes what it
  receives.
- **Core 1** runs bare code beside the kernel, started by `OSLaunchCore1(entry,
  stackTop)` from `main` (`Escapement_Core1.h`); it shares memory with the tasks
  through a slot buffer, and calls nothing else of the kernel.
- **Watching it run**: `make TRACE=1` records the scheduling in RAM, which
  `tools/read_trace.py` reads without stopping the processor; halting a core
  stops the kernel's timer with it (`rp2040.md`).
- **The Pico 2** takes the same calls, under `RP2350/Examples/pico2`, its core
  at 150 MHz. The power-aware kernel is not ported to it, and no board has run
  the port yet: only Renode has (`emulation.md`).
