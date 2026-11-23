#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Exhaustive check of the suspension of an event-driven task, on one core.

OSSuspendSynchronousTask (EscapementHard.c and its two siblings) only asks: it stores
the task in SuspendRequest, then raises the soft timer interrupt. The timer's handler
serves the request on entry (ServeSuspendRequest): the task becomes a zombie whose
context is not to be saved, leaves the ready queue, and is enqueued on its event; if a
signal was pending, the task goes on the reschedule list. Then the handler empties that
list into the ready queue, and the context switch wipes the stack of a task whose
context is not to be saved.

Three entities share the core, by priority: the task, below the soft timer interrupt,
below the interrupts that signal the event. Each step of the task and of the handler is
one access to the shared state, except the handler's enqueue on the event. That one is a
single step, as the signal's dequeue is: test/model/fifo.py shows that an enqueue
preempted at any access by a dequeue of the event, between its LL and its SC included,
ends as if one had run before the other. The host's signalinside sends a signal between
each LL of the suspension and its SC, on the kernels' own code. An interrupt that
signals runs whole: it dequeues a task from the event, or leaves a signal pending if
none waits (test/model/fifo.py checks that queue access by access), and pushes the task
it found on the reschedule list, then raises the soft timer interrupt. It may come at
any step of the task or of the handler, as many times as the run allows, the task's end
included. The handler preempts the task, or what runs once it has ended, once raised,
and runs again while the list is not empty, as its loop does.
FinalizeContextSwitchPreparation runs at its entry: a zombie at the head of the ready
queue is taken out of it, its context not to be saved.

Every run must keep these:
- the task is never inserted in the ready queue while it is in it;
- a task rescheduled is a zombie out of the ready queue (the assertion of DEBUG_MODE in
  EmptyRescheduleSynchronousTaskList);
- the context switch never wipes the stack of a task the kernel still runs, nor wipes
  one twice, nor resumes one it has wiped, nor returns to a task that asked to be
  suspended;
- every state not at its end has a next step;
- at the end, nothing is lost: with no signal, the task waits on its event; with one or
  more, it is in the ready queue, and no longer waits on its event. A signal sent while
  one is pending does not add up (docs/api.md): the model keeps one at most.

Faulty variants must be caught, to show the check can fail:
- "task": the task suspends itself, as before 2026-09-25, interrupts unmasked;
- "late": the request stored after the soft timer interrupt is raised;
- "stale": the handler leaves the request in place once it has served it;
- "kept": the handler leaves the task in the ready queue.

    python3 test/model/suspend.py
"""
import sys

MAX_SIGNALS = 2


def task_program(variant):
    if variant == "task":
        return ["enqueue", "zombie", "nosave", "remove", "reschedule", "switch"]
    if variant == "late":
        return ["raise", "request", "await"]
    return ["request", "raise", "await"]


HANDLER = ["finalize", "serve_zombie", "serve_nosave", "serve_remove", "serve_enqueue",
           "empty", "loop", "elect"]


def initial():
    return dict(ready=True, zombie=False, nosave=False, request=False, waiting=False,
                pending=0, resched=0, raised=False, signals=0, signaled=False,
                tpc=0, hpc=None, wiped=False, done=False, served=False,
                again=False)


class Fault(Exception):
    pass


def insert_ready(s):
    if s["ready"]:
        raise Fault("the task inserted in the ready queue while in it")
    if not s["zombie"]:
        raise Fault("a task rescheduled that is no zombie")
    s["ready"], s["zombie"], s["again"] = True, False, True


def wipe_check(s):
    """The instance whose stack is wiped must have left the ready queue; a new one
    released there since starts on a stack of its own."""
    if s["ready"] and not s["again"]:
        raise Fault("the stack of a task the kernel still runs wiped")


def signal(s):
    """An interrupt that signals the event, run whole."""
    s = dict(s)
    s["signals"] += 1
    if s["waiting"]:
        s["waiting"] = False
        s["resched"] += 1
        s["raised"] = True
    else:
        s["pending"] = 1
    return s


def task_step(s, prog):
    s = dict(s)
    op = prog[s["tpc"]]
    s["tpc"] += 1
    if op == "request":
        s["request"] = True
    elif op == "raise":
        s["raised"] = True
    elif op == "await":
        s["tpc"] -= 1                       # it spins until the interrupt takes it
        if not s["raised"]:
            raise Fault("the task waits for an interrupt nothing raised")
    elif op == "enqueue":                   # the task's own enqueue, variant "task"
        if s["pending"]:
            s["pending"], s["signaled"] = 0, True
        else:
            s["waiting"] = True
    elif op == "zombie":
        s["zombie"] = True
    elif op == "nosave":
        s["nosave"] = True
    elif op == "remove":
        s["ready"] = False
    elif op == "reschedule":
        if s["signaled"]:
            s["resched"] += 1
            s["raised"] = True
    elif op == "switch":
        if not s["nosave"]:
            raise Fault("the task ended with its context to be saved")
        wipe_check(s)
        s["wiped"], s["nosave"], s["done"] = True, False, True
    return s


def handler_step(s, variant):
    s = dict(s)
    op = HANDLER[s["hpc"]]
    s["hpc"] += 1
    if op == "finalize":
        s["raised"] = False
        if s["ready"] and s["zombie"]:
            s["ready"], s["nosave"] = False, True
    elif op.startswith("serve"):
        if not s["request"]:
            s["hpc"] = HANDLER.index("empty")
        elif op == "serve_zombie":
            s["zombie"] = True
        elif op == "serve_nosave":
            s["nosave"] = True
        elif op == "serve_remove":
            s["ready"] = variant == "kept"
        else:
            s["request"], s["served"] = variant == "stale", True
            if s["pending"]:
                s["pending"] = 0
                s["resched"] += 1
                s["raised"] = True
            else:
                s["waiting"] = True
    elif op == "empty":
        if s["resched"]:
            s["resched"] -= 1
            insert_ready(s)
            s["hpc"] -= 1
    elif op == "loop":
        s["raised"] = False                 # the PENDSTCLR before the loop's test
        if s["resched"]:
            s["hpc"] = HANDLER.index("empty")
    elif op == "elect":
        s["hpc"] = None
        if s["nosave"]:                     # the context switch wipes the stack
            if s["wiped"]:
                raise Fault("a context wiped twice")
            wipe_check(s)
            s["wiped"], s["nosave"] = True, False
            s["done"] = True
    return s


def key(s):
    return tuple(sorted(s.items()))


def explore(variant):
    prog = task_program(variant)
    seen, stack, ends = set(), [initial()], 0
    while stack:
        s = stack.pop()
        k = key(s)
        if k in seen:
            continue
        seen.add(k)
        succ = []
        if s["signals"] < MAX_SIGNALS:
            succ.append(signal(s))
        if s["hpc"] is not None:
            succ.append(handler_step(s, variant))
        elif s["raised"]:
            t = dict(s)
            t["hpc"] = 0                    # the soft timer interrupt preempts what runs
            succ.append(t)
        elif not s["done"]:
            if s["served"] and not s["wiped"]:
                raise Fault("the handler returned to a task that asked to be suspended")
            succ.append(task_step(s, prog))
        if s["done"] and s["hpc"] is None and not s["raised"]:
            ends += 1
            check_end(s)
        elif not succ:
            raise Fault("a state with no next step, not at its end")
        stack.extend(succ)
    return len(seen), ends


def check_end(s):
    if s["resched"]:
        raise Fault("the task left on the reschedule list")
    if s["signals"] == 0:
        if not s["waiting"] or s["ready"] or s["pending"]:
            raise Fault("with no signal, the task does not wait on its event")
    else:
        if s["waiting"]:
            raise Fault("signaled, the task still waits on its event")
        if not s["ready"]:
            raise Fault("signaled, the task is lost")


def main():
    failed = False
    states, ends = explore("request")
    print(f"the suspension asked of the handler: {states} states, {ends} ends, all kept")
    for variant, what in (("task", "the task suspending itself, unmasked"),
                          ("late", "the request stored after the interrupt is raised"),
                          ("stale", "the request left in place once served"),
                          ("kept", "the task left in the ready queue")):
        try:
            explore(variant)
            print(f"FAILED: not caught: {what}")
            failed = True
        except Fault as fault:
            print(f"caught: {what}: {fault}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    try:
        main()
    except Fault as fault:
        print(f"FAILED: {fault}")
        sys.exit(1)
