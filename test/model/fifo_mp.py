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

explore takes each core's accesses in program order. explore_weak lets each core
perform them in any order Armv8-M allows instead, with DMB barriers at chosen points:
it finds five needed, each caught when it is left out, and the six of
Escapement_CoreQueue.c enough, those five and one before a dequeue returns (see its
bounds, below explore).

Faulty variants must be caught: Figure 3's single SC, an enqueuer without the check of
E10, a dequeuer without that of D10 — both caught with SCs that fail only for a reason
too — and monitors local to each core, which do not see the other core's writes, as
the RP2350's are without ACTLR.EXTEXCLALL.

    python3 test/model/fifo_mp.py      # some 100 s, 2.3 GB at most; --jobs N in parallel
    python3 test/model/fifo_mp.py --wide   # the six barriers proven: 30 GB of memory
    python3 test/model/fifo_mp.py --sixth  # the five without the sixth: 55 min, compact
"""
import hashlib
import marshal
import multiprocessing
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


# Two cores, weakly ordered. Armv8-M lets each core's loads and stores be seen out of
# program order (DDI0553B.y, B7); Escapement_CoreQueue.c puts a DMB between any two
# accesses of the queue to different words so that they are not. explore_weak lets each
# core perform its accesses in any order the architecture allows, as threeslot.py and
# fourslot.py do for the slot buffers, and tells which of those barriers the queue
# needs:
#   - a DMB orders everything before it before everything after;
#   - a load may be performed before the branches ahead of it are decided, a store not
#     (a control dependency), nor before the SC ahead of it has succeeded or failed;
#   - an access whose address a load gives waits for that load (the place of an index);
#   - accesses to one location stay in program order, but a load may take its value
#     from a store of its own core the other does not see yet.
# Each core fetches its program ahead, guessing the outcome of each branch: the guess is
# checked once the loads it rests on are performed, and the path dropped if it was wrong,
# as a mispredicted branch leaves no trace. The enqueuer writes what its item points to
# before it enqueues it, and the dequeuer reads it after, checking it finds what was
# written: the barriers at the entry of Enqueue and at the exit of Dequeue stand for it.
#
# The bounds are what makes this tractable, and what limits it: a core has at most
# WINDOW accesses pending, an operation goes round its loop at most ITERATIONS times and
# tries the SC that advances Tail or Head at most SC_TRIES times. A run found wrong is an
# execution the architecture allows, and removing further barriers only allows more: a
# barrier found needed with all the others in place is needed. A run held within the
# window is held for good once the window is as long as the longest run of accesses
# between two barriers of the set, since no access crosses a barrier: for the six of
# Escapement_CoreQueue.c that is 8, which a machine of 30 GB explored on 2026-09-26,
# every scenario holding, each case at most some 25 minutes and 16 GB. The CI explores
# them within 3, a part of that. Of the fifteen points below, the other nine were each
# shown superfluous so, one at a time. "D6 exit" outgrew 26 GB on 2026-09-26; kept as a
# fingerprint (compact), each state 16 bytes where it took some 1.7 KB, the five others
# held without it on 2026-09-29, each scenario at its full window, the longest 9 and
# some 35 million states in 55 minutes (--sixth). It stays all the same: every bound here
# assumes a store is never performed before an SC ahead of it (weak_performable), and an
# audit of the port asked the same day whether Armv8-M promises that, a control
# dependency on an SC's status ordering nothing. Were it not so, Tail could be seen
# advanced before the item it counts is in its place, and the queue would want a DMB
# after E15 and D15, points the model found superfluous under that assumption.
POINTS = ("E0 entry", "E1 after E5", "E2 after E6", "E3 after E9", "E4 after E10",
          "E5 after helping", "E6 after E15", "E7 exit",
          "D0 loop", "D1 after D5", "D2 after D6", "D3 after D9", "D4 after D10",
          "D5 after D15", "D6 exit")
# The barriers of Escapement_CoreQueue.c.
KERNEL_BARRIERS = ("E1 after E5", "E3 after E9", "D1 after D5", "D2 after D6",
                   "D3 after D9", "D6 exit")
# The barriers each shown needed, with the scenario (below) that shows it soonest.
NEEDED = (("E1 after E5", 2), ("E3 after E9", 0), ("D1 after D5", 0), ("D2 after D6", 1),
          ("D3 after D9", 2))
WINDOW = 3
ITERATIONS = 2
SC_TRIES = 2
MEMORY = ("LD", "LX", "ST", "SX")


# An access or a check is a tuple: kind, location, value or guess, register, operation.
# Expressions: an int, ("r", register), ("+1", e); a location ("Head",), ("Tail",),
# ("Q", index) or ("D", item), the contents of an item.
def weak_value(e, regs):
    if isinstance(e, int):
        return e
    if e[0] == "r":
        return regs.get(e[1])
    v = weak_value(e[1], regs)
    return None if v is None else v + 1


def weak_location(where, regs):
    if len(where) == 1:
        return where
    v = weak_value(where[1], regs)
    if v is None:
        return None
    return (where[0], v % LEN) if where[0] == "Q" else (where[0], v)


def weak_check(check, regs):
    """The outcome of a branch, None while a register it reads is unknown."""
    test, args = check
    values = [weak_value(a, regs) for a in args]
    if None in values:
        return None
    if test == "full":
        return values[0] == values[1] + LEN
    if test == "null":
        return values[0] == NULL
    return values[0] == values[1]


def weak_fetch(frame, kind, arg, barriers):
    """Every way the next statement can be fetched: (accesses and checks, new frame,
    the operation's result once it ends). Frame: operation, pc, registers named,
    iteration, SC tries, next register."""
    opi, pc, env, it, tries, n = frame
    env = dict(env)
    idx = "Tail" if kind == "enq" else "Head"
    out = []

    def dmb(point):
        return [("DMB", None, None, None, opi)] if point in barriers else []

    def load(where, exclusive=False):
        return ("LX" if exclusive else "LD", where, None, (opi, n), opi)

    def check(test, args, guess):
        return ("CHK", (test, args), guess, None, opi)

    def go(ops, pc2, it2=it, tries2=tries, n2=n, env2=None, result=None):
        e = env if env2 is None else env2
        out.append((tuple(ops), (opi, pc2, tuple(sorted(e.items())), it2, tries2, n2),
                    result))

    r = ("r", (opi, n))
    e_or_d = "E" if kind == "enq" else "D"
    i0 = env.get("i")                         # the index read at E5 / D5
    if pc == "start":
        if kind == "enq":                     # what the item points to, then the item
            go([("ST", ("D", arg), arg, None, opi)] + dmb("E0 entry"), "5")
        else:
            go([], "5")
    elif pc == "5":                           # E5 t = Tail / D5 h = Head
        env["i"] = r
        go((dmb("D0 loop") if kind == "deq" else []) + [load((idx,))]
           + dmb("E1 after E5" if kind == "enq" else "D1 after D5"), "6", n2=n + 1, env2=env)
    elif pc == "6":                           # E6 full? / D6 empty?
        if kind == "enq":
            other_load = load(("Head",))
            full, test = FULL, ("full", (i0, r))
        else:
            other_load = load(("Tail",))
            full, test = EMPTY, ("eq", (i0, r))
        go([other_load, check(*test, True)], "done", n2=n + 1, result=full)
        go([other_load, check(*test, False)] + dmb(e_or_d + "2 after " + e_or_d + "6"),
           "9", n2=n + 1)
    elif pc == "9":                           # slot = LL(Q[i])
        env["s"] = r
        go([load(("Q", i0), True)] + dmb(e_or_d + "3 after " + e_or_d + "9"), "10",
           n2=n + 1, env2=env)
    elif pc == "10":                          # the index still the same?
        test = ("eq", (i0, r))
        go([load((idx,)), check(*test, True)] + dmb(e_or_d + "4 after " + e_or_d + "10"),
           "11", n2=n + 1)
        if it + 1 < ITERATIONS:
            go([load((idx,)), check(*test, False)], "5", it2=it + 1, n2=n + 1)
    elif pc == "11":                          # E11 slot != null / D11 slot == null: the
        free = kind == "enq"                  # index lags behind; else E15 / D15
        go([check("null", (env["s"],), free)], "15")
        go([check("null", (env["s"],), not free)], "12")
    elif pc == "12":                          # help the index on, then again
        if it + 1 >= ITERATIONS:
            return []
        after = dmb("E5 after helping") if kind == "enq" else []
        ll = load((idx,), True)
        for succeeded in (True, False):
            go([ll, check("eq", (r, i0), True), ("SX", (idx,), ("+1", i0), succeeded, opi)]
               + after, "5", it2=it + 1, n2=n + 1)
        go([ll, check("eq", (r, i0), False)] + after, "5", it2=it + 1, n2=n + 1)
    elif pc == "15":                          # SC the item in / null in
        put = arg if kind == "enq" else NULL
        go([("SX", ("Q", i0), put, True, opi)] + dmb(e_or_d + ("6 after E15" if kind == "enq"
                                                              else "5 after D15")), "16",
           tries2=0)
        if it + 1 < ITERATIONS:
            go([("SX", ("Q", i0), put, False, opi)], "5", it2=it + 1)
    elif pc == "16":                          # advance the index, again while it fails
        ll = load((idx,), True)
        sc = ("SX", (idx,), ("+1", i0))
        go([ll, check("eq", (r, i0), True), sc + (True, opi)], "ret", n2=n + 1)
        if tries + 1 < SC_TRIES:
            go([ll, check("eq", (r, i0), True), sc + (False, opi)], "16", tries2=tries + 1,
               n2=n + 1)
        go([ll, check("eq", (r, i0), False)], "ret", n2=n + 1)
    elif pc == "ret":
        if kind == "enq":
            go(dmb("E7 exit"), "done", result=OK)
        else:                                 # the item's contents, read after
            s = env["s"]
            go(dmb("D6 exit") + [("LD", ("D", s), None, (opi, n), opi),
                                 ("PAY", (s, r), None, None, opi)],
               "done", n2=n + 1, result=s)
    return out


def weak_performable(window, j, regs):
    """None if window[j] cannot be performed now, else the pending store of its own
    core a load takes its value from, or ()."""
    op = window[j]
    kind = op[0]
    if kind == "DMB":
        return () if j == 0 else None
    if kind not in MEMORY:
        return None
    where = weak_location(op[1], regs)
    if where is None or (kind in ("ST", "SX") and weak_value(op[2], regs) is None):
        return None
    source = ()
    for earlier in window[:j]:
        if earlier[0] == "DMB":
            return None
        if kind in ("ST", "SX") and earlier[0] in ("CHK", "SX"):
            return None                       # no store past an undecided branch
        if earlier[0] not in MEMORY:
            continue
        there = weak_location(earlier[1], regs)
        if there is None:
            return None                       # it may be the same location
        if there != where:
            continue
        if kind != "LD" or earlier[0] != "ST":
            return None                       # one location: in program order
        source = earlier
    return source


def fingerprint(state):
    """128 bits of state, for compact: marshal's version 2 writes no references, so
    equal states give equal bytes."""
    return hashlib.blake2b(marshal.dumps(state, 2), digest_size=16).digest()


def explore_weak(programs, barriers=POINTS, prefill=(), compact=False):
    """None if every run that ends is linearizable and every dequeuer finds its item's
    contents, else what went wrong. compact keeps a fingerprint of each state seen rather
    than the state, some 1.7 KB, as SPIN's hash compaction does: two states of n sharing
    one, which would leave a part unexplored, has a chance of some n^2 / 2^129."""
    memory = {("Head",): 0, ("Tail",): len(prefill)}
    memory.update({("Q", i): prefill[i] if i < len(prefill) else NULL for i in range(LEN)})
    for p in programs:
        memory.update({("D", a): 0 for k, a in p if k == "enq"})
    memory.update({("D", a): a for a in prefill})

    def frozen(d):
        return tuple(sorted(d.items(), key=repr))

    def refill(core, c):
        """Every way to top a core's window up: frame, window, registers, results of
        the operations fetched to their end, operations begun."""
        grown, done = [core], []
        while grown:
            frame, window, regs, ended, begun = grown.pop()
            if sum(o[0] in MEMORY for o in window) >= WINDOW or frame[0] >= len(programs[c]):
                done.append((frame, window, regs, ended, begun))
                continue
            kind, arg = programs[c][frame[0]]
            known = dict(regs)
            for ops, frame2, result in weak_fetch(frame, kind, arg, barriers):
                outcomes = [(o, weak_check(o[1], known)) for o in ops if o[0] == "CHK"]
                if any(v is not None and v != o[2] for o, v in outcomes):
                    continue                  # a guess the known registers deny
                ops = tuple(o for o in ops if o[0] != "CHK" or weak_check(o[1], known) is None)
                ended2 = ended
                if frame2[1] == "done":
                    ended2 = ended + ((frame2[0], result),)
                    frame2 = (frame2[0] + 1, "start", (), 0, 0, 0)
                grown.append((frame2, window + ops, regs, ended2, begun))
        return done

    def settle(core, c, history):
        """End the operations fetched to their end with nothing pending, drop a DMB at
        the head of the window, which no core can see, and forget unused registers."""
        frame, window, regs, ended, begun = core
        while window and window[0][0] == "DMB":
            window = window[1:]
        regs, still = dict(regs), []
        for opi, result in ended:
            if any(o[4] == opi for o in window):
                still.append((opi, result))
                continue
            if isinstance(result, tuple):
                result = weak_value(result, regs)
            if opi not in begun:
                history += ((c, opi, "inv", None),)
            history += ((c, opi, "res", result),)
            begun = tuple(b for b in begun if b != opi)
        used = set()

        def uses(e):
            if isinstance(e, tuple):
                if e and e[0] == "r":
                    used.add(e[1])
                else:
                    for x in e:
                        uses(x)
        for o in window:
            uses(o[1])
            uses(o[2])
        uses(frame[2])
        uses(tuple(still))
        regs = tuple(sorted((k, v) for k, v in regs.items() if k in used))
        return (frame, window, regs, tuple(still), begun), history

    fresh = ((0, "start", (), 0, 0, 0), (), (), (), ())
    starts = [(frozen(memory), (None, None), (a, b), ())
              for a in refill(fresh, 0) for b in refill(fresh, 1)]
    key = fingerprint if compact else (lambda state: state)
    seen, todo = {key(s) for s in starts}, list(starts)
    while todo:
        mem, res, cores, history = todo.pop()
        if all(not core[1] and core[0][0] >= len(programs[c]) for c, core in enumerate(cores)):
            if not linearizable_weak(history, programs, prefill):
                return "a run is not linearizable: " + " ".join(
                    f"c{c}.{op}{'>' if k == 'inv' else '<' + str(v)}"
                    for c, op, k, v in history)
            continue
        for c, (frame, window, regs, ended, begun) in enumerate(cores):
            known = dict(regs)
            for j, op in enumerate(window):
                source = weak_performable(window, j, known)
                if source is None:
                    continue
                kind, where = op[0], weak_location(op[1], known) if op[1] else None
                memory = dict(mem)
                outcomes = []                 # memory, reservations, registers
                if kind == "DMB":
                    outcomes.append((memory, res, known))
                elif kind in ("LD", "LX"):
                    after = dict(known)
                    after[op[3]] = weak_value(source[2], known) if source else memory[where]
                    r = list(res)
                    if kind == "LX":
                        r[c] = where
                    outcomes.append((memory, tuple(r), after))
                else:                          # ST, SX: the queue fills one granule
                    r = list(res)
                    succeeded = kind == "ST" or (op[3] and res[c] == where)
                    if kind == "SX":
                        r[c] = None
                        if op[3] != succeeded and op[3]:
                            continue           # guessed a success the monitor denies
                    if succeeded:
                        memory[where] = weak_value(op[2], known)
                        if where[0] != "D":
                            r[1 - c] = None
                    outcomes.append((memory, tuple(r), known))
                for memory2, res2, known2 in outcomes:
                    rest, wrong = [], False
                    for o in window[:j] + window[j + 1:]:
                        if o[0] == "CHK":
                            v = weak_check(o[1], known2)
                            if v is None:
                                rest.append(o)
                            wrong |= v is not None and v != o[2]
                        elif o[0] == "PAY":
                            item, found = (weak_value(x, known2) for x in o[1])
                            if item is None or found is None:
                                rest.append(o)
                            elif item != found:
                                return f"a dequeuer reads item {item}'s contents as {found}"
                        else:
                            rest.append(o)
                    if wrong:
                        continue               # the path was mispredicted
                    h = history
                    begun2 = begun
                    if kind != "DMB" and op[4] not in begun and not any(
                            e[:3] == (c, op[4], "res") for e in history):
                        begun2 = begun + (op[4],)
                        h = h + ((c, op[4], "inv", None),)
                    core = (frame, tuple(rest), tuple(sorted(known2.items())), ended, begun2)
                    for topped in refill(core, c):
                        topped, h2 = settle(topped, c, h)
                        both = list(cores)
                        both[c] = topped
                        state = (frozen(memory2), res2, tuple(both), h2)
                        k = key(state)
                        if k not in seen:
                            seen.add(k)
                            todo.append(state)
                            if compact and len(seen) % 10_000_000 == 0:
                                print(f"    {len(seen) // 1_000_000}M states seen, "
                                      f"{len(todo)} to do", file=sys.stderr, flush=True)
    return None


def longest_run(programs, barriers):
    """The longest run of accesses with no DMB between them, over every path of the
    programs, one operation after another: a WINDOW that long leaves out no order a
    core could perform them in."""
    best = 0
    for program in programs:
        todo, seen = [((0, "start", (), 0, 0, 0), 0)], set()
        while todo:
            frame, run = todo.pop()
            if (frame, run) in seen or frame[0] >= len(program):
                continue
            seen.add((frame, run))
            kind, arg = program[frame[0]]
            for ops, frame2, _ in weak_fetch(frame, kind, arg, barriers):
                r = run
                for o in ops:
                    if o[0] == "DMB":
                        r = 0
                    elif o[0] in MEMORY:
                        r += 1
                        best = max(best, r)
                if frame2[1] == "done":
                    frame2 = (frame2[0] + 1, "start", (), 0, 0, 0)
                todo.append((frame2, r))
    return best


def linearizable_weak(history, programs, prefill=()):
    """As linearizable, each core's operations also kept in their order: under weak
    ordering the accesses of one may be performed before those of the one before."""
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
            if (k[0], k[1] - 1) in left or any(ops[o][1] < ops[k][0] for o in left if o != k):
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
    return search(list(ops), tuple(prefill))


SCENARIOS = [
    # (what, programs for core 0 and core 1, items already queued)
    ("two enqueuers, then each dequeues",
     ([("enq", 1), ("deq", None)], [("enq", 2), ("deq", None)]), ()),
    ("an enqueuer beside a dequeuer, the queue holding one item",
     ([("enq", 2), ("enq", 3)], [("deq", None), ("deq", None)]), (1,)),
    ("both dequeuing a full queue, then refilling it",
     ([("deq", None), ("enq", 3)], [("deq", None), ("enq", 4)]), (1, 2)),
]


def wide():
    """The six barriers of the queue with the window their runs need, every scenario:
    some 25 minutes and 16 GB each at most (2026-09-26)."""
    global WINDOW
    ok = True
    for what, programs, prefill in SCENARIOS:
        WINDOW = longest_run(programs, KERNEL_BARRIERS)
        result = explore_weak(programs, KERNEL_BARRIERS, prefill)
        print(f"  {what}, window {WINDOW}: {'holds' if result is None else result}",
              flush=True)
        ok &= result is None
    sys.exit(0 if ok else 1)


def sixth(jobs):
    """The five barriers shown needed without the one before a dequeue returns, each
    scenario with the window its runs need, states kept compact: whether that sixth one
    is needed. In parallel on jobs processes."""
    five = tuple(b for b in KERNEL_BARRIERS if b != "D6 exit")
    with multiprocessing.Pool(jobs) as pool:
        runs = [(what, longest_run(programs, five),
                 pool.apply_async(explore_sized, (programs, five, prefill,
                                                  longest_run(programs, five))))
                for what, programs, prefill in SCENARIOS]
        ok = True
        for what, window, run in runs:
            result = run.get()
            print(f"  {what}, window {window}, without the DMB before a dequeue returns: "
                  f"{'holds' if result is None else result}", flush=True)
            ok &= result is None
    sys.exit(0 if ok else 1)


def explore_sized(programs, barriers, prefill, window):
    """explore_weak within window pending, states kept compact, in a process of its own."""
    global WINDOW
    WINDOW = window
    return explore_weak(programs, barriers, prefill, compact=True)


def later(pool, fn, *args, **kwargs):
    """fn(*args, **kwargs) on the pool, or here when it is read, with no pool."""
    if pool:
        return pool.apply_async(fn, args, kwargs)

    class Now:
        def get(self):
            return fn(*args, **kwargs)
    return Now()


def main():
    args = sys.argv[1:]
    if args == ["--wide"]:
        wide()
    if args == ["--sixth"]:
        sixth(len(SCENARIOS))
    jobs = int(args[args.index("--jobs") + 1]) if "--jobs" in args else 1
    pool = multiprocessing.Pool(jobs) if jobs > 1 else None
    # Every exploration asked for first, the longest first, so that a pool keeps busy;
    # read in the order they are printed in.
    weak = [later(pool, explore_weak, p, KERNEL_BARRIERS, f) for _, p, f in SCENARIOS]
    needed = []
    for point, scenario in NEEDED + (("every", 1),):
        _, programs, prefill = SCENARIOS[scenario]
        fewer = () if point == "every" else tuple(b for b in POINTS if b != point)
        needed.append((point, later(pool, explore_weak, programs, fewer, prefill)))
    plain = [later(pool, explore, p, prefill=f) for _, p, f in SCENARIOS]
    faults = [(fault, [later(pool, explore, p, prefill=f, **kwargs) for _, p, f in SCENARIOS])
              for fault, kwargs in (
                  ("Figure 3's single SC on Tail and Head", {"faults": ("single SC",)}),
                  ("an enqueuer without the check of E10", {"faults": ("no E10",)}),
                  ("a dequeuer without the check of D10", {"faults": ("no D10",)}),
                  ("monitors local to each core", {"global_monitor": False}))]
    paper = [later(pool, explore, p, faults=("single SC",), prefill=f, spurious=False,
                   shared_granule=False) for _, p, f in SCENARIOS]

    ok = True
    print(f"two cores, a queue of {LEN} places, the RP2350 monitor, spurious SC failures:")
    for (what, _, _), result in zip(SCENARIOS, plain):
        result = result.get()
        print(f"  {what}: {'holds' if result is None else result}")
        ok &= result is None
    for fault, results in faults:
        caught = None
        for result in results:
            caught = result.get()
            if caught:
                break
        print(f"  {fault}: {'caught: ' + caught[:90] if caught else 'NOT CAUGHT'}")
        ok &= caught is not None
    held = all(result.get() is None for result in paper)
    print(f"  Figure 3's single SC under the paper's LL/SC, an SC failing only once its "
          f"word was written: "
          f"{'holds' if held else 'FAILS'}")
    ok &= held
    print("two cores, each free to reorder its accesses as Armv8-M allows, within "
          f"{WINDOW} pending:")
    for (what, _, _), result in zip(SCENARIOS, weak):
        result = result.get()
        print(f"  {what}, the queue's six DMB: {'holds' if result is None else result}")
        ok &= result is None
    for point, result in needed:
        caught = result.get()
        what = "any DMB" if point == "every" else f"{point} DMB, the others in place"
        print(f"  without {what}: {'caught: ' + caught[:80] if caught else 'NOT CAUGHT'}")
        ok &= caught is not None
    print("all checks passed" if ok else "CHECKS FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
