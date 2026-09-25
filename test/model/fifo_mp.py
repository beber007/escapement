#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Exhaustive check of the FIFO queue of Evéquoz between two cores.

The kernels' queue adds to the array-based LL/SC queue of C. Evéquoz ("Non-Blocking
Concurrent FIFO Queues with Single Word Synchronization Primitives", ICPP 2008,
doi:10.1109/ICPP.2008.82) an announced operation that preempting callers complete,
which holds only while preemptions nest, on one core (fifo.py). Between the two cores
of the RP2350 the queue of the paper's Figure 3 is taken, adapted to the LL/SC that
chip has: lock-free, not wait-free, every operation a loop of LL/SC attempts.

    Enqueue(node):                          Dequeue():
      E5  t = Tail                            D5  h = Head
      E6  if t == Head + LEN: return FULL     D6  if h == Tail: return EMPTY
      E9  slot = LL(Q[t % LEN])               D9  slot = LL(Q[h % LEN])
      E10 if t == Tail:                       D10 if h == Head:
      E11   if slot != null:                  D11   if slot == null:
      E12     if LL(Tail) == t:               D12     if LL(Head) == h:
      E13       SC(Tail, t + 1)               D13       SC(Head, h + 1)
      E15   elif SC(Q[t % LEN], node):        D15   elif SC(Q[h % LEN], null):
      E16     if LL(Tail) == t:               D16     if LL(Head) == h:
      E17       SC(Tail, t + 1)               D17       SC(Head, h + 1)
      E18     return OK                       D18     return slot
      and again from E5                       and again from D5

The paper assumes LL/SC as it should be (its Figure 2); the RP2350 gives less, and the
model gives what it does (datasheet 2.1.6, Armv8-M B9): each core holds one
reservation, which any write of the other core to the same granule clears, whatever
the value, and which its own SC clears; an SC may also fail for no visible reason, an
interrupt between it and its LL. The array, Head and Tail all sit in one granule of
16 bytes here, the worst case: every write of one core ends the other's reservation.

Two cores each run a few operations, one step per line above that touches the shared
memory, and every interleaving is explored. Every run must be linearizable — some
order of its operations, consistent with the order of those that did not overlap,
gives each its result from a plain queue of LEN places — and no operation may return
an item nobody enqueued or one already dequeued. Operations need not end: an SC can
fail for ever, which lock-freedom allows; the runs that do end are all checked.

Figure 3 rests on the LL/SC of the paper's Figure 2, where an SC fails only when another
thread's SC on the same word succeeded: an enqueuer whose SC on Tail fails (E16, E17)
may then take it that Tail has moved on. The paper warns that real LL/SC give less (its
Section 5: SCs failing spuriously, reservations covering a set of addresses) and offers
another algorithm for such architectures, built on CAS (its Figure 5). The RP2350 is one:
an SC also fails when the other core wrote anywhere in the granule, or for no visible
reason. With Figure 3 as it stands, Tail then stays behind an enqueue that returned,
and a dequeue that follows answers that the queue is empty while it holds the item;
Head likewise. Escapement_CoreQueue.c keeps Figure 3 and tries that SC again while the LL
still finds the old index, as the kernels' 3-slot writer does (threeslot.py). The
model checks that adaptation; it keeps Figure 3's single SC as a variant that must fail
under the RP2350's LL/SC, and shows it holding under the paper's own, an SC failing only
once its word was written.

Each core is taken to perform its accesses in program order: the implementation
(Escapement_CoreQueue.c) puts a DMB between any two of them to different words.

Faulty variants must be caught: Figure 3's single SC, an enqueuer without the check of
E10, a dequeuer without that of D10 — both caught with SCs that fail only for a reason
too — and monitors local to each core, which do not see the other core's writes, as
the RP2350's are without ACTLR.EXTEXCLALL.

    python3 test/model/fifo_mp.py
"""
import sys
from collections import deque

LEN = 2                      # places in the array (a power of two, as the paper asks)
NULL = 0
FULL, EMPTY, OK = "full", "empty", "ok"


def initial(programs, head=0):
    """Queue empty at index head, both cores at the start of their first operation."""
    cores = tuple((0, "start", ()) for _ in programs)
    return ((NULL,) * LEN, head, head, (None, None), cores, ())


def write(reservations, core, target, global_monitor, shared_granule):
    """A write by core to target ends the other core's reservation, if the monitor sees
    it and the reservation is on the same granule: all of the queue's, or target's."""
    other = reservations[1 - core]
    if not global_monitor or other is None or not (shared_granule or other == target):
        return reservations
    r = list(reservations)
    r[1 - core] = None
    return tuple(r)


def steps(state, programs, faults, global_monitor, spurious, shared_granule):
    """Every state one step of one core leads to."""
    q, head, tail, res, cores, history = state
    out = []
    for c, (op, pc, loc) in enumerate(cores):
        if op >= len(programs[c]):
            continue
        kind, arg = programs[c][op]
        succ = []                 # (q, head, tail, res, pc, loc, result or None)

        def go(pc2, loc2, q2=q, head2=head, tail2=tail, res2=res, result=None):
            succ.append((q2, head2, tail2, res2, pc2, loc2, result))

        def sc(target, value, pc_ok, pc_fail, loc2):
            """SC to target ("Q", i) or "Head" / "Tail": its outcomes."""
            if res[c] == target:
                q2, h2, t2 = list(q), head, tail
                if target == "Head":
                    h2 = value
                elif target == "Tail":
                    t2 = value
                else:
                    q2[target[1]] = value
                r2 = list(write(res, c, target, global_monitor, shared_granule))
                r2[c] = None
                go(pc_ok, loc2, tuple(q2), h2, t2, tuple(r2))
            if res[c] != target or spurious:
                r2 = list(res)
                r2[c] = None
                go(pc_fail, loc2, res2=tuple(r2))   # failed: lost, or for no reason

        def ll(target, value, pc2, loc2):
            r2 = list(res)
            r2[c] = target
            go(pc2, loc2 + (value,), res2=tuple(r2))

        if kind == "enq":
            if pc == "start":
                succ.append((q, head, tail, res, "E5", (), ("inv",)))
            elif pc == "E5":
                go("E6", (tail,))
            elif pc == "E6":
                t, = loc
                if t == head + LEN:
                    go("done", (), result=FULL)
                else:
                    go("E9", loc)
            elif pc == "E9":
                t, = loc
                ll(("Q", t % LEN), q[t % LEN], "E10", loc)
            elif pc == "E10":
                t, slot = loc
                if "no E10" in faults or t == tail:
                    go("E12" if slot != NULL else "E15", loc)
                else:
                    go("E5", ())
            elif pc in ("E12", "E16"):
                ll("Tail", tail, pc + "b", loc)
            elif pc in ("E12b", "E16b"):
                t, slot, seen = loc
                nxt = "E5" if pc == "E12b" else "ret"
                if seen == t:
                    go("E13" if pc == "E12b" else "E17", loc[:2])
                else:
                    go(nxt, loc[:2] if nxt == "ret" else ())
            elif pc in ("E13", "E17"):
                t, slot = loc
                if pc == "E13":
                    sc("Tail", t + 1, "E5", "E5", ())
                else:           # again from E16 while it fails; Figure 3 tries once
                    again = "ret" if "single SC" in faults else "E16"
                    sc("Tail", t + 1, "ret", again, loc)
            elif pc == "E15":
                t, slot = loc
                n = len(succ)
                sc(("Q", t % LEN), arg, "E16", "E5", loc)
                succ[n:] = [e if e[4] != "E5" else e[:5] + ((),) + e[6:] for e in succ[n:]]
            elif pc == "ret":
                go("done", (), result=OK)
        else:
            if pc == "start":
                succ.append((q, head, tail, res, "D5", (), ("inv",)))
            elif pc == "D5":
                go("D6", (head,))
            elif pc == "D6":
                h, = loc
                if h == tail:
                    go("done", (), result=EMPTY)
                else:
                    go("D9", loc)
            elif pc == "D9":
                h, = loc
                ll(("Q", h % LEN), q[h % LEN], "D10", loc)
            elif pc == "D10":
                h, slot = loc
                if "no D10" in faults or h == head:
                    go("D12" if slot == NULL else "D15", loc)
                else:
                    go("D5", ())
            elif pc in ("D12", "D16"):
                ll("Head", head, pc + "b", loc)
            elif pc in ("D12b", "D16b"):
                h, slot, seen = loc
                nxt = "D5" if pc == "D12b" else "ret"
                if seen == h:
                    go("D13" if pc == "D12b" else "D17", loc[:2])
                else:
                    go(nxt, loc[:2] if nxt == "ret" else ())
            elif pc in ("D13", "D17"):
                h, slot = loc
                if pc == "D13":
                    sc("Head", h + 1, "D5", "D5", ())
                else:           # again from D16 while it fails; Figure 3 tries once
                    again = "ret" if "single SC" in faults else "D16"
                    sc("Head", h + 1, "ret", again, loc)
            elif pc == "D15":
                h, slot = loc
                n = len(succ)
                sc(("Q", h % LEN), NULL, "D16", "D5", loc)
                succ[n:] = [e if e[4] != "D5" else e[:5] + ((),) + e[6:] for e in succ[n:]]
            elif pc == "ret":
                go("done", (), result=loc[1])

        for q2, h2, t2, r2, pc2, loc2, result in succ:
            ev = ()
            if result == ("inv",):
                ev = ((c, op, "inv", None),)
                result = None
            cores2 = list(cores)
            if pc2 == "done":
                ev = ((c, op, "res", result),)
                cores2[c] = (op + 1, "start", ())
            else:
                cores2[c] = (op, pc2, loc2)
            out.append((q2, h2, t2, r2, tuple(cores2), history + ev))
    return out


def linearizable(history, programs, prefill=()):
    """Some order of the operations, consistent with real time, that a plain queue of
    LEN places, holding prefill at the start, gives these results in."""
    ops = {}
    for i, (c, op, kind, value) in enumerate(history):
        ops.setdefault((c, op), [None, None, None])
        if kind == "inv":
            ops[(c, op)][0] = i
        else:
            ops[(c, op)][1], ops[(c, op)][2] = i, value

    def search(left, queue):
        if not left:
            return True
        for k in left:
            # k may go first only if no other remaining operation ended before k began
            if any(ops[o][1] < ops[k][0] for o in left if o != k):
                continue
            kind, arg = programs[k[0]][k[1]]
            if kind == "enq":
                expected = FULL if len(queue) == LEN else OK
                rest = queue if expected == FULL else queue + (arg,)
            else:
                expected = queue[0] if queue else EMPTY
                rest = queue[1:]
            if ops[k][2] == expected and search([o for o in left if o != k], rest):
                return True
        return False
    return search([k for k, v in ops.items() if v[1] is not None], tuple(prefill))


def explore(programs, faults=(), global_monitor=True, prefill=(), spurious=True,
            shared_granule=True):
    """None if every run that ends is linearizable, else what went wrong."""
    start = initial(programs)
    if prefill:
        q = list(start[0])
        for i, item in enumerate(prefill):
            q[i % LEN] = item
        start = (tuple(q), 0, len(prefill)) + start[3:]
    seen, todo = {start}, deque([start])
    while todo:
        state = todo.popleft()
        q, head, tail, res, cores, history = state
        if all(op >= len(programs[c]) for c, (op, _, _) in enumerate(cores)):
            if not linearizable(history, programs, prefill):
                return "a run is not linearizable: " + " ".join(
                    f"c{c}.{op}{'>' if k == 'inv' else '<' + str(v)}"
                    for c, op, k, v in history)
            continue
        for nxt in steps(state, programs, faults, global_monitor, spurious,
                         shared_granule):
            if nxt not in seen:
                seen.add(nxt)
                todo.append(nxt)
    return None


SCENARIOS = [
    # (what, programs for core 0 and core 1, items already queued)
    ("two enqueuers, then each dequeues",
     ([("enq", 1), ("deq", None)], [("enq", 2), ("deq", None)]), ()),
    ("an enqueuer beside a dequeuer, the queue holding one item",
     ([("enq", 2), ("enq", 3)], [("deq", None), ("deq", None)]), (1,)),
    ("both dequeuing a full queue, then refilling it",
     ([("deq", None), ("enq", 3)], [("deq", None), ("enq", 4)]), (1, 2)),
]


def main():
    ok = True
    print(f"two cores, a queue of {LEN} places, the RP2350 monitor, spurious SC failures:")
    for what, programs, prefill in SCENARIOS:
        result = explore(programs, prefill=prefill)
        print(f"  {what}: {'holds' if result is None else result}")
        ok &= result is None
    for fault, kwargs in (("Figure 3's single SC on Tail and Head", {"faults": ("single SC",)}),
                          ("an enqueuer without the check of E10", {"faults": ("no E10",)}),
                          ("a dequeuer without the check of D10", {"faults": ("no D10",)}),
                          ("monitors local to each core", {"global_monitor": False})):
        caught = None
        for what, programs, prefill in SCENARIOS:
            caught = explore(programs, prefill=prefill, **kwargs)
            if caught:
                break
        print(f"  {fault}: {'caught: ' + caught[:90] if caught else 'NOT CAUGHT'}")
        ok &= caught is not None
    held = all(explore(p, faults=("single SC",), prefill=f, spurious=False,
                       shared_granule=False) is None for _, p, f in SCENARIOS)
    print(f"  Figure 3's single SC under the paper's LL/SC, an SC failing only once its "
          f"word was written: "
          f"{'holds' if held else 'FAILS'}")
    ok &= held
    print("all checks passed" if ok else "CHECKS FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
