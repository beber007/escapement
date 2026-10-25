# Writing an application

This guide goes from an empty `main` to tasks that talk to each other and to
interrupts. It covers what the three kernels share and where they differ. The headers
`Escapement/EscapementHard.h`, `EscapementSoft.h` and `EscapementHardPA.h` remain the
reference for every parameter, and the examples under `Escapement/CORTEX-Mx/*/Examples`
show each call at work.

The API is not yet stable: until 1.0 a release may change a call, as semantic
versioning allows for 0.x. `Escapement.h` gives the release an application is built
against: `ESCAPEMENT_RELEASE`, "0.1.0", and its numbers `ESCAPEMENT_RELEASE_MAJOR`,
`_MINOR` and `_PATCH`.

## Choosing a kernel

| Kernel | Header | Build on the Pico | For |
|---|---|---|---|
| Hard | `EscapementHard.h` | `make` | every deadline met |
| Soft | `EscapementSoft.h` | `make KERNEL=SOFT` | (m,k)-firm tasks: m deadlines met in any k instances |
| Power-aware | `EscapementHardPA.h` | `make KERNEL=PA` (also on the NUCLEO-U385RG-Q) | hard deadlines, the speed lowered whenever they allow |

Each kernel schedules by earliest deadline first (EDF) or by deadline-monotonic
priorities (DM). The choice is made by `SCHEDULER_REAL_TIME_MODE` in the example's
`Escapement_Config.h`, or by `make SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING`.

The power-aware kernel adds a policy, `POWER_MANAGEMENT`. OTE is the default and works
under either algorithm. DRA and DR_OTE impose EDF*, and DM_SLACK imposes DM; they are
selected with `make KERNEL=PA POWER=DRA` and so on. `power-aware.md` explains the
policies, and `rp2040.md` measures them on the Pico.

An application includes `Escapement.h` alone, which picks the kernel's header from the
configuration.

## The shape of `main`

`main` initialises the processor and the application, creates the tasks, and hands the
processor to the kernel, which never gives it back. On the Pico:

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
(`*(volatile UINT32 *)0xE000ED08` and `0xD000001C`). Each example defines those it uses.
With the power-aware kernel, `main` also calls `OSInitProcessorSpeed()` after the
clocks.

`OSStartMultitasking(f, argument)` calls `f(argument)` just before scheduling starts.
That is the place to enable the application's interrupts or to signal a first event.
The locals of `main` do not survive the call.

## Time

Times are counted in ticks of the kernel's timer, which are microseconds on every port.
The timers of the RP2040 and the RP2350 run at 1 MHz; TIM2 of the STM32U5 gets there
through a prescaler set for its 160 MHz clock, and the STM32U3's for its 96 MHz, or, under
the power-aware kernel, from the MSIK's 12 MHz, which the changes of speed leave alone.

A period is given in two parts: `periodCycles` full turns of 2^30 ticks, and a remainder
`periodOffset` below 2^30. This lets a period reach years. `periodCycles` is 0 for any
period shorter than 2^30 ticks, some eighteen minutes at 1 MHz, and must stay below
65535. A deadline is at least one tick and at most the period. It must also stay below
2^30 ticks however long the period is, because the kernel adds it to an arrival time in
32 bits.

## Periodic tasks

A task is a function that runs to the end and calls `OSEndTask()` last. The kernel calls
it again at every period. A task must not wait. All tasks run on one stack, and a
preempted task keeps its context beneath the task that preempted it, so a task that
looped waiting for another would wait for ever. A task that needs something from another
reads what is there and returns.

| Kernel | Creation |
|---|---|
| Hard | `OSCreateTask(task, periodCycles, periodOffset, deadline, argument)` |
| Soft | `OSCreateTask(task, wcet, periodCycles, periodOffset, deadline, m, k, startInstance, argument)` |
| Power-aware | `OSCreateTask(task, wcet, periodCycles, periodOffset, deadline, argument)` |

Every creation returns `FALSE` when memory runs out, for a period or a deadline outside
those limits, or, under DM, past the number of tasks the kernel can count: 255 in all,
or 127 with the soft kernel, whose optional instances add the number of tasks to their
priority. The soft and power-aware kernels also refuse a negative `wcet` or one past the
deadline, a task that could never meet it. The first instance of every task arrives when
the kernel starts. There is no offset.

### Soft kernel

Out of any `k` consecutive instances, `m` must meet their deadline. The others are
optional. An optional instance runs only if the kernel can still meet every mandatory
deadline, and that test reads the declared `wcet`, not the time a task actually takes.
The kernel tests an optional instance once, when nothing else is ready, and a drop is
final. The test counts every mandatory instance released before the optional one's
deadline, which is safe and drops more than an exact test would.
`m = k` makes a task hard. `m` is at least 1 and at most `k`. `startInstance` staggers
the pattern of mandatory instances between tasks, and `OSGetTaskInstance()` tells a
task where in its pattern it stands. `wcet` may be 0 when every task has `m = k`.

### Power-aware kernel

`wcet` is the worst-case execution time in ticks at the fastest speed. The kernel slows
the processor so that the work left fits before the deadline, and it trusts that
figure. An understated `wcet` lets the kernel pick a speed too low for the task, which
then misses its deadline. `FourSlotCoresPico` declares 400 µs for a reader measured at
314 µs at most (`rp2040.md`): measure first, then add a margin. Whether slowing down
saves energy depends on the chip: on the STM32U5 it does not, racing to Stop 2 winning at
every load, on the RP2350 some 10 % at best, on the RP2040 up to some 5 % with the
idle task in WFI and nothing once it sleeps in SLEEP, and on the STM32U3 a cycle costs 14 %
less at 48 MHz than at 96 (`power-aware.md`).

`OSSetMinimalProcessorSpeed(speed)`, called before `OSStartMultitasking`, keeps the
processor at or above an operating point (`OS_12MHZ_SPEED`, `OS_50MHZ_SPEED`,
`OS_125MHZ_SPEED` on the Pico; `OS_48MHZ_SPEED`, `OS_96MHZ_SPEED` on the NUCLEO-U385RG-Q).

### When an instance overruns

Every kernel assumes that each mandatory instance ends before its task's next release:
a task set its algorithm schedules (under EDF, deadlines equal to periods, a load of
100 % at most; under DM, in general less), each task within its declared `wcet` (the
soft and power-aware kernels) or within the time the designer's analysis gave it (the
hard kernel). A periodic instance still unfinished when its task is released again
overruns: one that started, under every kernel, and under the soft kernel a mandatory one
that never did. An optional one that never started is dropped instead, which is what it
is for. An event-driven task cannot overrun: it is released again only once it has
ended.

At an overrun the kernel counts it and calls the hook the application gave:

```c
static UINT8 OnOverrun(void *argument)   /* the argument the task was created with */
{
   SetOutputsSafe();                     /* short, from the kernel's timer interrupt */
   OSScheduleSuspendedTask(ReportEvent); /* an event-driven task may report it */
   return OS_OVERRUN_CONTINUE;           /* or OS_OVERRUN_STOP */
}
/* in main, before OSStartMultitasking() */
OSSetOverrunHook(OnOverrun);
```

Without a hook, or when it returns `OS_OVERRUN_STOP`, the kernel stops: interrupts
disabled, a loop for ever, the whole system halted, as ZottaOS did. That is the default:
the contract is broken, and a hard real-time system holds no guarantee past it. The hook
can set the outputs safe first, or arm a watchdog that resets the chip.

With `OS_OVERRUN_CONTINUE`, the instance under way goes on, with its deadline as it is,
and the release that found it is skipped: the task is next released one period later.
Under the soft kernel the release skipped still counts in the task's (m,k) pattern, a
mandatory instance missed. `OSGetOverruns()` gives the overruns of all tasks, and a task
calling `OSGetTaskOverruns()` its own. What going on cannot do, because all tasks share
one stack: drop an instance that started, or give it a later deadline, since its frames
lie above those of the instances it preempted, which cannot run before it ends. Under
EDF the late instance keeps the earliest deadline until it ends, and the others wait for
it: one overrun makes others, EDF's domino. A bounded overrun, a burst of interrupts, is
then lived through; an instance that loops for ever still holds the processor, the
interrupts running. Under DM, only the tasks of lower priority wait.

The kernel notices an overrun only at that next release. With a deadline shorter than
the period, an instance that misses its deadline but ends before its next release is
late, and nothing reports it. Noticing an instance as it passes its `wcet` would take a
budget timer for each instance (issue #17).

### Measuring execution times

Nothing above checks a declared `wcet`. Built with `make WCET=1`
(`ESCAPEMENT_MEASURE_WCET`), the hard and soft kernels add up, for each instance of each
task, the ticks it holds the processor, and keep the largest of the instances ended:

```sh
make WCET=1                                  # then load the image and let it run
tools/wcet.py build/SoakPico.elf --margin 20 # read over SWD, the cores left running
```

`tools/wcet.py` prints, for each task by the name of its code and its argument, the
instances ended, the largest time seen and, with `--margin`, that time plus the margin.
A task reads its own with `OSGetTaskMaxTime()`; another program, a debugger or the other
core, walks the list `_OSTaskTimes`, one `OSTASKTIME` a task, fixed once the kernel
runs. Each figure is one word, written by the context switch and the soft timer
interrupt alone, read whole at any time; a task's figures read one after the other may
straddle the end of an instance.

What is counted: the time from the context switch that gives an instance the processor
to the next one, the soft timer interrupt left out. An instance preempted is not charged
for the tasks that preempt it, nor for the soft timer interrupt electing them; it is for
the peripheral interrupts it meets, the timer's own that raises the soft one among them,
and for the few instructions of the switch around it. Under the soft kernel an instance
that ends is charged too for what `OSEndTask` and `OSSuspendSynchronousTask` do before
they switch: they choose the next instance, and when that is an optional one, run the
admission test on each in line until one is admitted, 13.9 µs in the mean and 31.7 at
most a test, interrupts unmasked as there, on the Pico's bench (`docs/method.md`).
The figure is an upper estimate of what the instance itself took, on the run that was
made.

On the Pico W (2026-10-23), two runs of SoakFirmPico, of three and two minutes: its
(2,5)-firm task, a busy wait of 1500 µs on the clock, 1503 and 1499 µs at most, over the
27,304 and 19,570 instances that ran, the optional ones dropped not counted;
SoakPico's heartbeat, which then scanned core 0's stack whole, 4.4 ms against the 50 µs
it declared (it scans a slice of it a second since, #41). The switch does some 70
instructions more on its longest path, interrupts masked: under a microsecond at 125 MHz,
counted from the disassembly.

It is the worst case seen, not a bound: valid only for a load that took the instance's
worst path. A bound needs static analysis. aiT lists the Cortex-M0, M3 and M4 but not the
M0+ or the M33; OTAWA (LGPL) runs on 32-bit ARM; Bound-T's Cortex-M port is unfinished.
The RP2040 running from SRAM, without a cache, is the likeliest case for one.

The power-aware kernel refuses the option: an instance there takes longer at a lower
speed, and its ticks would need scaling by the speed of each stretch of it, the speed
changed inside the timer handler. Without the option, every image is the same, byte for
byte, as before it existed.

### Tasks created while the kernel runs

`OSCreateTask` works before `OSStartMultitasking` only, its blocks taken by `OSMalloc`.
Built with `make LIVE=N` (`ESCAPEMENT_LIVE_TASKS`), the hard kernel under EDF keeps a pool
of N periodic tasks, which a task, an interrupt handler or the overrun hook creates and
deletes while the kernel runs (#45):

```c
void *task = OSCreateTaskLive(Worker, 0, 1000, 1000, argument);   /* NULL: pool empty */
...
OSDeleteTaskLive(task);
```

`OSCreateTaskLive` takes the timing `OSCreateTask` takes, and checks it the same way. Its
first instance is released at once: the call masks the interrupts for a few instructions,
leaving them as it found them, puts its request on a list and raises the soft timer
interrupt, whose handler serves it at the time it reads once it has shifted the others at
the 2^30 wraparound: before a task goes on, after an interrupt handler or the overrun hook
has returned, or code with interrupts masked has unmasked them. Should the counter pass
2^30 between the handler's test of the overflow and its reading of the time, the request
waits for the shift; the one case the handler cannot tell is a read at the very tick of
the wrap when every read since the wrap before (since the start, for the first) was at
that one's tick, and the overflow's flag tested before its interrupt has had time to raise
it. A task whose deadline is earlier than a calling task's, or the same, preempts it as
soon as the request is served. Called before `OSStartMultitasking`, the request waits for
the handler's first pass, and the task is released at the time that pass reads, a few µs
after the tasks of `OSCreateTask`.

`OSDeleteTaskLive` stops the releases. A task between two instances goes back to the pool
at once. An instance under way, started or only released, runs to its end, since on the
one stack an instance that has started can only end: the block goes back at the next
release, which is not made. Until then the pool is one block short, a period, or longer
if that instance overruns. A task may delete itself. An instance that overruns is counted
and goes through the hook as any other. The call refuses a task not from the pool, one
whose first release is still to come, and one already deleted; once the block is back,
though, the handle may name the task that took it next, which a second call would delete:
a task is deleted once.

The kernel tests no schedulability: whether the processor holds one more task is the
caller's to decide. Deadline-monotonic scheduling fixes the priorities before the start,
and the soft and power-aware kernels compute on the whole set at the start too: the
three refuse the option. With it, the timer runs from the start even with no task, a task
of the pool may be the first. Under `make WCET=1` each block of the pool comes with its
figures, in `_OSTaskTimes` from the start, a block never handed out with no code;
`OSCreateTaskLive` clears them for the task it starts.

`LiveTasksPico` and `LiveTasksPico2` turn a manager of 2 ms round four slots, creating and
deleting workers of 500 to 1300 µs, one of them deleting itself, ask for a fifth whenever
the four slots are full, and count every instance started late or due after its deletion.
On the Pico W (2026-10-24), two minutes of it: 37,564 creations, 37,561 deletions, 345,582
instances, 7,513 fifths refused, none started late, none due after its deletion, every
call answered as expected, four blocks; under Renode, on the Pico 2, 2 s: 626 creations,
624 deletions, 5,874 instances, 125 fifths refused, the same zeros.
Without the option, every image is the same, byte for byte, as before it existed.

## Event-driven tasks

A task that runs when something happens, rather than at fixed times, waits on an event
descriptor. It ends with `OSSuspendSynchronousTask()` instead of `OSEndTask()`, and runs
again once the event is signalled:

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

A signal is remembered. If it arrives while the task still runs, the task restarts as
soon as it ends, so a task can signal itself to run again.

Several tasks may wait on the same event. Each signal wakes one of them: the one that
has waited longest, whatever its priority or its deadline. The waiting tasks form a
FIFO queue in all three kernels (`DequeueEventTask`), as in ZottaOS (User Manual,
May 2012, p. 116). A task that must answer before the others needs an event of its
own. Signals come from tasks,
interrupt handlers, or the function given to `OSStartMultitasking`, never from `main`
itself.

| Kernel | Creation |
|---|---|
| Hard | `OSCreateSynchronousTask(task, workload, event, argument)` |
| Soft, power-aware | `OSCreateSynchronousTask(task, wcet, workload, aperiodicUtilization, event, argument)` |

The `workload` of an event-driven task is its worst-case execution time divided by the
share of the processor set aside for it. It is also the task's deadline and its minimum
interarrival time. In the example above, 2000 suits a task that must be done within
2 ms of its signal. A workload is at least one tick and below 2^30, and at most 255
tasks wait on one event. `OSCreateSynchronousTask` returns `FALSE` outside those limits,
for a `NULL` event, which is what `OSCreateEventDescriptor` returns when memory runs
out, and for the reasons `OSCreateTask` fails.

The soft and power-aware kernels add a `wcet` before the workload and an
`aperiodicUtilization` after it: the share of the processor left to all event-driven
tasks, in 256ths. The soft kernel reads that share under EDF only; in the power-aware
kernel, only DRA and DR_OTE read it. Given a workload of 0 under EDF, the soft kernel
computes it from `wcet` and the share.

Under EDF the soft kernel reserves for the event-driven tasks at least the largest
`wcet / workload` among them, rounded up to a 256th, whatever share was declared. It
refuses a `wcet` that would take the whole processor. Both kernels refuse a negative
`wcet` or one above the workload, under either algorithm. A
`wcet` of 0 leaves the task out of that share.

### Timer events

`Escapement_TimerEvent` signals an event after a delay. `main` calls
`OSInitTimerEvent(nodes, priority, OS_IO_TIMER_2)`, which returns `FALSE` for no node or
short memory; a task then calls
`OSScheduleTimerEvent(event, delay, OS_IO_TIMER_2)`, and `OSUnScheduleTimerEvent` takes a
pending one back. `TestTimerEventPico` raises a pin every 5 ms and has an event-driven
task lower it 1 ms later.

## Passing data between tasks

Nothing locks in Escapement, so nothing can be kept waiting by a preempted task. Three
mechanisms pass data without a lock, each for one kind of exchange. They allocate their
memory when created, in `main`.

### A queue, when every item counts

`OSInitFIFOQueue(nodes, nodeSize)` creates a queue and a pool of `nodes` buffers of
`nodeSize` bytes. The producer takes a buffer, fills it and enqueues it. The consumer
dequeues it, uses it and gives it back:

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

Any number of tasks and interrupt handlers may enqueue and dequeue. A buffer goes back
to the queue it came from.

### Slot buffers, when only the latest value counts

A sensor read by an interrupt and consumed by a task wants the newest reading, not a
backlog. `OSInitBuffer(size, type, event)` creates a buffer for one writer and one
reader. The type is `OS_BUFFER_TYPE_4_SLOT` (Simpson's four slots, no atomic
instruction) or `OS_BUFFER_TYPE_3_SLOT` (less memory, an LL/SC pair).

The writer calls `OSWriteBuffer(buffer, data, n)`. Once `size` bytes are in, the slot is
published and the next write starts a new one. The reader takes a copy with
`OSGetCopyBuffer(buffer, mode, copy)` or a pointer with
`OSGetReferenceBuffer(buffer, mode, &pointer)`. With `OS_READ_ONLY_ONCE` each slot is
returned once, and 0 after that; `OS_READ_MULTIPLE` returns the latest slot every time.
Given an event, the buffer signals it at every published slot, which wakes an
event-driven reader.

### LL/SC, for lock-free code of the application's own

`OSUINT8_LL` … `OSINT32_SC` load a word and store it back only if nothing intervened. A
store-conditional also fails when an interrupt merely came in between and the value did
not change. Always retry it in a loop, and never read a failure as a conflict.

### Between the two cores

All three mechanisms assume one core. Slot buffers are the exception, when read with
`OS_READ_MULTIPLE`; `OS_READ_ONLY_ONCE` marks the slot read with an LL/SC pair, which
the Cortex-M0+ emulates for one core only. The emulated pair relies on a reservation
bit that only interrupts clear, and the queue's announced operation assumes preemptions
that nest: a caller completes, through `PendingOp`, an operation it preempted, whose
descriptor on that operation's stack is then still alive. `test/model/fifo.py` checks
it access by access; nothing checks it at run time.

The 4-slot buffer works between the two cores of the Pico, as `FourSlotCoresPico` shows
on the board (`rp2040.md`). Both slot buffers work between those of the Pico 2, since
the port makes `LDREX`/`STREX` see both cores: their models say so, and so did a Pico 2
on 2026-09-28, 30 runs of `ThreeSlotCoresPico2` none torn (`architecture.md`). Between
the cores the buffer takes no event: signalled from core 1, it would pend the kernel's
interrupt on core 1, where no kernel runs.

### A queue between the cores, on the Pico 2

`Escapement_CoreQueue.h` gives the RP2350 a FIFO queue of pointers. Any code on either
core may use it: a task, a handler, or a bare loop on core 1.

```c
void *queue = OSInitCoreQueue(16);        /* in main; the length a power of 2 */
OSEnqueueCoreQueue(queue, node);          /* FALSE when full; node never NULL */
void *node = OSDequeueCoreQueue(queue);   /* NULL when empty */
```

The queue carries pointers and owns nothing. Nodes go round through a second such
queue, as in `FIFOCoresPico2`. It signals no event, so a task on core 0 polls it. It is
lock-free, not wait-free: an operation retries while the other core keeps winning.

What the node points to is ordered with the queue: the enqueuer's stores before it,
seen by the dequeuer after it. The caller's other accesses are not. An enqueue is no
barrier for what follows it, nor a dequeue for what comes before it. A caller telling
the other core "enqueued" through a flag of its own puts `_OSMemoryBarrier()` before
storing the flag, and the other core one after reading it and before its dequeue.
Otherwise the flag may be seen before the item, and the queue found empty (an audit of
the models, 2026-09-30).

## Interrupts

The kernel owns the vector table and routes every peripheral interrupt through a
descriptor, a structure whose first field is the handler. The handler receives the
descriptor, so it can keep its state there rather than in globals:

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

The application then enables the interrupt in the peripheral and in the NVIC, at a
priority the kernel leaves free. On the Cortex-M0+ of the Pico these are levels 0 and 1,
one of which the kernel's timer takes (`TIMER_PRIORITY`); SysTick and PendSV hold the
two lowest. A handler should be short. It clears its source, moves data through a queue
or a slot buffer, and signals the task that does the rest.

## Memory

`OSMalloc(size)` allocates for good, and only before `OSStartMultitasking`. It draws
from `OSMALLOC_INTERNAL_HEAP_SIZE` bytes set in `Escapement_Config.h`, and the queues,
buffers, events and tasks take their memory from it. The stack takes all the RAM left.
A stack that reaches the globals faults: MSPLIM on the Cortex-M33, a region of the MPU
over the 1 KB kept below the stack on the Cortex-M0+, which a frame of 1 KB or more
steps over. There is no `free` and no C library: the code is built freestanding.

## On the Pico

- The image runs from SRAM, loaded over SWD. There is no second-stage bootloader, and
  nothing is written to the flash (`rp2040.md`):

  ```sh
  openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -f target/rp2040.cfg \
      -c init -c 'reset halt' -c 'load_image build/TaskLEDPico.elf' \
      -c 'resume 0x20000000' -c exit
  ```

  A program that runs core 1 must add `-c 'set USE_CORE 0'` before the target file
  (`tools/fourslot_cores.sh`).
- The UART is set up with `OSInitUART(nodes, size, receiveHandler, OS_IO_UART0)`.
  `OSGetFreeNodeUART` and `OSEnqueueUART` then send. `UARTEchoPico` echoes what it
  receives.
- Core 1 runs bare code beside the kernel, started by `OSLaunchCore1(entry, stackTop)`
  from `main` (`Escapement_Core1.h`). It shares memory with the tasks through a slot
  buffer and calls nothing else of the kernel.
- To watch it run, `make TRACE=1` records the scheduling in RAM, and
  `tools/read_trace.py` reads it without stopping the processor. Halting a core stops
  the kernel's timer with it (`rp2040.md`).

## On the Pico 2

The Pico 2 takes the same calls, under `RP2350/Examples/pico2`, with its core at
150 MHz. The power-aware kernel is not ported to it. A Pico 2 on the bench has run the
six examples that count in memory at each commit since 2026-09-28, and the periods of
the two that toggle outputs were read on a frequency counter on 2026-09-29. The UART
echo and two tasks sending at once have been checked there too since 2026-09-30
(`tools/board_ci.md`).

## On the Arduino UNO Q

The STM32U585 of the UNO Q takes the same calls too, under `STM32U5/Examples/uno-q`,
with its core at 160 MHz. `tools/unoq_load.sh` loads the images into SRAM over the
board's own SWD. The power-aware kernel is not ported to it (`stm32u5.md`).

- Two UARTs are available: `OS_IO_USART1`, on D1 and D0 of the connector at 115,200
  baud, and `OS_IO_LPUART1`, to the board's Linux (`/dev/ttyHS1`) at 57,600 baud by
  default, `OS_LPUART1_BAUD_RATE` otherwise (115,200 for `SleepU5` and `SoakU5`).
- Timer events take `OS_IO_TIM5`, since TIM2 is the kernel's.

### An idle task in Stop 2

`Escapement_Stop2.h` lets the idle task sleep in Stop 2. `main` calls `OSInitStop2()`
after `OSInitializeSystemClocks` and before `OSStartMultitasking`. It returns FALSE, and
leaves the idle task in Sleep, if the 32.768 kHz crystal does not run.

From then on, whenever the next arrival or timer event is 5 ms away or more
(`OS_STOP2_MIN_US`), the idle task sleeps in Stop 2 and has LPTIM1 wake the chip 3 ms
before it (`OS_STOP2_WAKE_US`). The kernel's time is then advanced by what LPTIM1
counted. This asks the following of an application:

- In Stop 2 every clock stops except the 32.768 kHz crystal's, LPTIM1's and, when
  LPUART1 asks for it, HSI16's. The kernel moves TIM2 and TIM5 on afterwards. Any other
  timer the application drives, TIM3 for one, loses the time slept.
- The independent watchdog keeps running in Stop 2 unless an option byte freezes it
  (FLASH_OPTR.IWDG_STOP, not read on the UNO Q). The task that refreshes it keeps its
  period, asleep or not.
- There is no Stop 2 while a UART sends, nor while USART1 has a receive handler.
  LPUART1 receives through Stop 2. Its first byte is sampled while its clock starts,
  hence its 57,600 baud by default; at 115,200 the client sends a wake-up byte first
  and the bytes after it, within `OS_STOP2_LINK_WINDOW_US`, come on a running clock
  (`SleepU5.c`, `stm32u5.md`, "The wake-up byte").
- An interrupt that comes during Stop 2 is taken once the clock is raised again, up to
  some 1 ms later.
- The debug port sleeps with the chip, so an image is loaded with the reset held, as
  `tools/unoq_load.sh` does.
- LPTIM1 belongs to the idle task. An application may call `OSInitLPTimer` and the rest
  of `Escapement_LPTimer.h` only if it does not call `OSInitStop2`.

`OSAllowStop2(FALSE)` keeps the idle task in Sleep until `OSAllowStop2(TRUE)`.
`OSGetStop2Counts` gives the number of entries into Stop 2, the longest wake-up in ticks
of LPTIM1, and the number of wake-ups that came past their event. `SleepU5` uses all of
it.

The STM32U3 port has the same interface (`stm32u3.md`, "The idle task in Stop 2"), with
TIM4 in place of TIM5, LPUART1 on D1 and D0, and USART1 disabled across each Stop 2. Its
counts give, in place of the HSE's misses, the wake-ups whose MSI did not lock again
within `OS_STOP2_LOCK_TICKS`.
