#!/usr/bin/env python3
"""Exhaustive check of the FIFO queue of the kernels, on one core.

FIFOEnqueue, FIFODequeue, their helpers and IncrementFifoQueueIndex (EscapementHard.c
and its two siblings) are modelled access by access: every read or write of the queue,
of its array, of PendingOp or of a descriptor is a step. The LL/SC pair is the one the
Cortex-M0+ emulates (Escapement_Atomic.c): LL raises a single reservation flag, SC
writes only if it is still up and lowers it, and the end of every preempting operation
clears it, as the exit of an interrupt or a context switch does.

A run starts a list of operations in turn. The next one may start at any step of the
one running, which it then preempts, and runs to its end before the preempted one goes
on: preemptions nest, as they do on one core. Every such run is explored, from queues
already holding items and from indices about to wrap.

Each run must be linearizable: some order of its operations, consistent with the order
in which non-overlapping ones ran, must give every operation its result from the
abstract queue, and leave that queue where the array is. The abstract queue holds
items; used for events (signal = SIGNAL), it also holds a pending signal, which the
next enqueue takes instead of inserting, and which a dequeue leaves when it finds no
item. Operations must also end: every run is bounded.

Faulty variants must be caught, to show the check can fail.

    python3 test/model/fifo.py
"""
import itertools
import sys

NULL, SIGNAL, UNSET = 0, -1, "unset"
QLEN = 2
MAXINDEX = 4            # GetFIFOArrayMaxIndex keeps a multiple of the length; small here
NOIDX = 0xFFFF
FAULT = set()           # names of injected faults


# ---- the concrete queue, as the kernels hold it --------------------------------------

def step(state, ops):
    """All successors of a state: the top frame's next step, or a new operation."""
    q, head, tail, pending, reserved, descs, stack, started, history = state
    succ = []
    if started < len(ops):                               # start (preempt with) the next op
        kind, arg = ops[started]
        frame = ("op", started, 0, ())
        succ.append((q, head, tail, pending, reserved, descs, stack + (frame,),
                     started + 1, history + ((started, "start", None),)))
    if stack:
        succ.extend(run_top(state, ops))
    return succ


def desc_get(descs, i, field):
    return dict(descs[i])[field]


def desc_set(descs, i, field, value):
    d = dict(descs[i])
    d[field] = value
    return descs[:i] + (tuple(sorted(d.items())),) + descs[i + 1:]


def run_top(state, ops):
    q, head, tail, pending, reserved, descs, stack, started, history = state
    frame = stack[-1]
    below = stack[:-1]
    ftype, opid, pc, loc = frame
    kind, arg = ops[opid] if ftype == "op" else (None, None)

    def at(pc2, loc2=loc, **changes):
        s = dict(q=q, head=head, tail=tail, pending=pending, reserved=reserved,
                 descs=descs, history=history)
        s.update(changes)
        return (s["q"], s["head"], s["tail"], s["pending"], s["reserved"], s["descs"],
                below + ((ftype, opid, pc2, loc2),), started, s["history"])

    def call(pc2, callee, loc2=loc, **changes):
        st = at(pc2, loc2, **changes)
        return st[:6] + (st[6] + (callee,),) + st[7:]

    def ret(value, **changes):
        # pop the frame; hand the value to the caller, which resumes after its call
        s = dict(q=q, head=head, tail=tail, pending=pending, reserved=reserved,
                 descs=descs, history=history)
        s.update(changes)
        caller = below[-1] if below else None
        rest = below[:-1] if below else below
        if ftype == "op":                                 # end of an operation
            if "reservation-survives" not in FAULT:
                s["reserved"] = False                     # the exit clears it
            return (s["q"], s["head"], s["tail"], s["pending"], s["reserved"], s["descs"],
                    below, started, s["history"] + ((opid, "end", value),))
        ctype, cid, cpc, cloc = caller
        return (s["q"], s["head"], s["tail"], s["pending"], s["reserved"], s["descs"],
                rest + ((ctype, cid, cpc, cloc + (("ret", value),)),), started, s["history"])

    def ll(value):
        return value, True

    signal = SIGNAL if kind and kind.startswith("ev") else NULL

    # ---- FIFOEnqueue / FIFODequeue (top level) ----
    if ftype == "op":
        enq = kind in ("enq", "ev-enq")
        if pc == 0:                                       # initialise the descriptor
            d = (("Done", False), ("Head", NOIDX), ("Item", arg), ("Propose", NOIDX),
                 ("SlotPropose", UNSET), ("SlotReturn", UNSET))
            return [at(1, descs=descs[:opid] + (d,) + descs[opid + 1:])]
        if pc == 1:                                       # op = queue->PendingOp
            return [at(2, (("op", pending),))]
        if pc == 2:
            op = dict(loc)["op"]
            if op is None or "no-help" in FAULT:
                return [at(3)]
            helper = ("enqh" if op[1] == "E" else "deqh", op[0], 0, (("sig", signal),))
            return [call(3, helper, ())]
        if pc == 3:                                       # post this operation
            return [at(4, (), pending=(opid, "E" if enq else "D"))]
        if pc == 4:
            helper = ("enqh" if enq else "deqh", opid, 0, (("sig", signal),))
            return [call(5, helper, ())]
        if pc == 5:
            return [at(6, pending=None)]
        if pc == 6:
            r = desc_get(descs, opid, "SlotReturn")
            if enq:
                return [ret(r == signal)]
            return [ret(NULL if r == SIGNAL else r)]

    # ---- helpers: the propose/commit prologue, shared by both ----
    d_id, loc = opid, dict(loc)
    enqh = ftype == "enqh"
    index_now = tail if enqh else head
    if ftype in ("enqh", "deqh"):
        L = lambda **kv: tuple(sorted({**loc, **kv}.items()))
        if pc == 0:
            if desc_get(descs, d_id, "Propose") != NOIDX:
                return [at(2, L())]
            return [at(1, L(tmp=index_now))]
        if pc == 1:
            return [at(2, L(), descs=desc_set(descs, d_id, "Propose", loc["tmp"]))]
        if pc == 2:
            if desc_get(descs, d_id, "Head") != NOIDX:
                return [at(4, L())]
            return [at(3, L(tmp=desc_get(descs, d_id, "Propose")))]
        if pc == 3:
            return [at(4, L(), descs=desc_set(descs, d_id, "Head", loc["tmp"]))]
        if pc == 4:
            return [at(5, L(i=desc_get(descs, d_id, "Head") % QLEN))]
        if pc == 5:
            if desc_get(descs, d_id, "SlotPropose") != UNSET:
                return [at(7, L())]
            return [at(6, L(tmp=q[loc["i"]]))]
        if pc == 6:
            return [at(7, L(), descs=desc_set(descs, d_id, "SlotPropose", loc["tmp"]))]
        if pc == 7:
            if desc_get(descs, d_id, "SlotReturn") != UNSET:
                return [at(9, L())]
            return [at(8, L(tmp=desc_get(descs, d_id, "SlotPropose")))]
        if pc == 8:
            return [at(9, L(), descs=desc_set(descs, d_id, "SlotReturn", loc["tmp"]))]
        i, sig = loc["i"], loc["sig"]
        if pc == 9:
            r = desc_get(descs, d_id, "SlotReturn")
            go = (r in (NULL, SIGNAL)) if enqh else (r != sig)
            return [at(10, L())] if go else [ret(None)]
        if pc == 10:                                      # slot = LL(&Q[i])
            return [at(11, L(slot=q[i]), reserved=True)]
        slot = loc.get("slot")
        done = lambda: desc_set(descs, d_id, "Done", True)

        def sc(value, pc_ok, pc_fail, loc2):
            if reserved or "sc-always" in FAULT:
                return at(pc_ok, loc2, q=q[:i] + (value,) + q[i + 1:], reserved=False)
            return at(pc_fail, loc2)

        def increment(pc_after):
            callee = ("inc", d_id, 0, (("which", "tail" if enqh else "head"),
                                       ("old", desc_get(descs, d_id, "Head"))))
            return call(pc_after, callee, L())

        # The tests that precede an SC read the descriptor, and a preemption may come
        # between them and the SC: the SC is a step of its own (pc 13), which then goes
        # on at pc_ok or back to the LL.
        def then_sc(value, pc_ok):
            return at(13, L(sc_value=value, sc_ok=pc_ok))

        if pc == 13:
            return [sc(loc["sc_value"], loc["sc_ok"], 10, L())]
        if enqh:
            if pc == 11:
                if desc_get(descs, d_id, "Done"):
                    return [ret(None)]
                if slot == SIGNAL:
                    return [then_sc(NULL, 20)]
                if slot == NULL:
                    if desc_get(descs, d_id, "SlotReturn") == SIGNAL:
                        return [ret(None, descs=done())]
                    return [then_sc(desc_get(descs, d_id, "Item"), 12)]
                return [increment(20)]                    # own item in place: advance Tail
            if pc == 12:
                return [increment(20)]
            if pc == 20:
                return [ret(None, descs=done())]
        else:
            if pc == 11:
                if desc_get(descs, d_id, "Done"):
                    return [ret(None)]
                if slot == SIGNAL:                        # the signal this dequeue left
                    if "signal-unmarked" in FAULT:
                        return [ret(None)]
                    return [ret(None, descs=done())]
                if slot == NULL:
                    if desc_get(descs, d_id, "SlotReturn") == NULL:
                        return [then_sc(SIGNAL, 20)]
                    return [increment(20)]                # item gone: advance Head
                return [then_sc(NULL, 12)]
            if pc == 12:
                return [increment(20)]
            if pc == 20:
                return [ret(None, descs=done())]

    # ---- IncrementFifoQueueIndex ----
    if ftype == "inc":
        which, old = loc["which"], loc["old"]
        new = (old + 1) % MAXINDEX
        cur = tail if which == "tail" else head
        L = lambda **kv: tuple(sorted({**loc, **kv}.items()))
        if pc == 0:                                       # LL(index) == oldValue ?
            if cur != old:
                return [ret(None, reserved=True)]
            return [at(1, L(), reserved=True)]
        if pc == 1:                                       # SC(index, tmp)
            if reserved:
                ch = {"tail": new} if which == "tail" else {"head": new}
                return [ret(None, reserved=False, **ch)]
            return [at(0, L())]
    raise AssertionError(frame)


# ---- the abstract queue, and the check -----------------------------------------------

def abstract(items, signalled, kind, arg):
    """Apply one operation; returns (items, signalled, result)."""
    if kind == "enq":
        if len(items) == QLEN:
            return items, signalled, False
        return items + (arg,), signalled, True
    if kind == "deq":
        return (items[1:], signalled, items[0]) if items else (items, signalled, NULL)
    if kind == "ev-enq":                                  # a task blocks on the event
        if signalled:
            return items, False, True
        return items + (arg,), signalled, False
    if kind == "ev-deq":                                  # the event is signalled
        if items:
            return items[1:], signalled, items[0]
        return items, True, NULL
    raise AssertionError(kind)


def contents(q, head, tail):
    """Items from Head to Tail, and whether a signal waits at Head."""
    items, h = [], head
    while h != tail:
        items.append(q[h % QLEN])
        h = (h + 1) % MAXINDEX
    return tuple(items), q[head % QLEN] == SIGNAL and head == tail


def linearizable(ops, history, final, start_items, start_signal):
    spans = {}
    for t, (opid, what, value) in enumerate(history):
        spans.setdefault(opid, [None, None, None])
        if what == "start":
            spans[opid][0] = t
        else:
            spans[opid][1], spans[opid][2] = t, value
    ids = list(spans)
    for order in itertools.permutations(ids):
        pos = {o: k for k, o in enumerate(order)}
        if any(spans[a][1] < spans[b][0] and pos[a] > pos[b] for a in ids for b in ids):
            continue
        items, signalled, ok = start_items, start_signal, True
        for o in order:
            items, signalled, result = abstract(items, signalled, *ops[o])
            if result != spans[o][2]:
                ok = False
                break
        if ok and (items, signalled) == final:
            return True
    return False


def explore(ops, start_items=(), start_signal=False, index=0):
    q = [NULL] * QLEN
    for k, item in enumerate(start_items):
        q[(index + k) % QLEN] = item
    tail = (index + len(start_items)) % MAXINDEX
    if start_signal:
        q[index % QLEN] = SIGNAL
    start = (tuple(q), index, tail, None, False, tuple(() for _ in ops), (), 0, ())
    seen, todo, steps = {start}, [start], 0
    while todo:
        state = todo.pop()
        steps += 1
        if steps > 2_000_000:
            return "the exploration does not end"
        q, head, tail, pending, reserved, descs, stack, started, history = state
        if started == len(ops) and not stack:
            final = contents(q, head, tail)
            if not linearizable(ops, history, final, start_items, start_signal):
                return f"not linearizable: {ops} from {start_items} -> {history}, final {final}"
            continue
        for n in step(state, ops):
            if len(n[6]) > 12:
                return f"unbounded: {ops}"
            if n not in seen:
                seen.add(n)
                todo.append(n)
    return None


SCENARIOS = [
    # user queue, signal NULL
    ([("enq", 1), ("enq", 2), ("deq", None)], (), False),
    ([("enq", 1), ("deq", None), ("deq", None)], (), False),
    ([("deq", None), ("enq", 2), ("deq", None)], (7,), False),
    ([("enq", 1), ("enq", 2), ("enq", 3)], (), False),                    # a full queue
    ([("deq", None), ("enq", 1), ("enq", 2)], (7, 8), False),
    ([("deq", None), ("enq", 1), ("enq", 2)], (7,), False),               # back to the same slot
    # event queue: tasks block (ev-enq), signals wake them (ev-deq)
    ([("ev-deq", None), ("ev-enq", 1), ("ev-deq", None)], (), False),
    ([("ev-enq", 1), ("ev-deq", None), ("ev-enq", 2)], (), False),
    ([("ev-deq", None), ("ev-deq", None), ("ev-enq", 1)], (), False),
    ([("ev-enq", 1), ("ev-enq", 2), ("ev-deq", None)], (), False),
    ([("ev-enq", 2), ("ev-deq", None)], (), True),
]


def check_all():
    for ops, items, signalled in SCENARIOS:
        for order in set(itertools.permutations(ops)):
            for index in range(MAXINDEX):
                failure = explore(list(order), items, signalled, index)
                if failure:
                    return failure
    return None


def main():
    ok = True
    failure = check_all()
    print(f"  the kernel's FIFO queue {'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    for fault, name in (("signal-unmarked",
                         "a dequeue that leaves the signal it finds unmarked (the kernels before)"),
                        ("sc-always", "an SC that ignores the reservation"),
                        ("reservation-survives", "a reservation that survives a preemption"),
                        ("no-help", "a caller that does not complete the announced operation")):
        FAULT.clear()
        FAULT.add(fault)
        failure = check_all()
        print(f"  {name} is caught: {'yes' if failure else 'NOT CAUGHT'}")
        ok = ok and failure is not None
    FAULT.clear()
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
