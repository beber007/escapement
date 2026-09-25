#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Exhaustive check of the 4-slot buffer of the kernels, on one core and on two.

OSWriteBuffer and GetReadyBuffer4Slot (EscapementHard.c and its two siblings) follow
H. R. Simpson, "Four-slot fully asynchronous communication mechanism", IEE Proceedings E,
137(1), 1990 (doi:10.1049/ip-e.1990.0002), and are modelled statement by statement. On
one core the writer is an interrupt handler: each of its steps runs whole between two
steps of the reader, a task. A writer step either writes one byte into the slot chosen
in advance, or, once that slot is full, publishes it and chooses the next one. The
reader takes the latest pair, announces it, takes the latest slot of that pair, then
reads the slot byte by byte. On two cores, where Simpson meant the mechanism to run,
the writer runs beside the reader, and the publication is four steps, one per
statement of OSWriteBuffer: the reader can come in between any two.

Every interleaving is explored, and two properties checked, those J. Rushby
model-checked for Simpson's algorithm (SRI, 2002):
  - coherence: the writer never writes the slot the reader is reading, so no read
    mixes the bytes of two records;
  - sequencing: the values read never go backwards.

Faulty writers are checked too, and must be caught: they show the check can fail. One
publishes the pair before its slot, which only two cores can tell from the kernel.

Two cores also let each core's accesses be seen out of program order: Armv8-M memory is
weakly ordered (Armv8-M Architecture Reference Manual, DDI0553B.y, B7). explore_weak()
lets each core perform its loads and stores in any order the architecture allows, and
finds which DMB barriers the buffer needs.

    python3 test/model/fourslot.py
"""
import sys
from collections import deque

BYTES = 2      # bytes per slot, so that a record takes several writer steps
LAST = 7       # the writer writes 1, 2, ... LAST, each value in every byte of a slot


# The choice of the next slot, as OSWriteBuffer makes it: the pair the reader is not on,
# then in it the slot that is not its latest (Simpson's writer); and two faulty choices.
KERNEL = (lambda reading: 1 - reading, lambda index, pair: 1 - index[pair])
READERS_PAIR = (lambda reading: reading, KERNEL[1])
LATEST_SLOT = (KERNEL[0], lambda index, pair: index[pair])


# How OSWriteBuffer publishes a full slot and chooses the next, one statement per step:
#   Index[wpair] = windex; Latest = wpair; pair = !Reading; windex = !Index[pair]
KERNEL_ORDER = ("index", "latest", "pair", "slot")
# The same with the pair published before its slot: a reader coming in between takes
# the pair's previous slot, which the writer may be filling.
LATEST_FIRST = ("latest", "index", "pair", "slot")


def explore(choose, cores=1, order=KERNEL_ORDER):
    """Breadth-first search of every state; returns None, or the property violated.

    On one core the writer is an interrupt handler and publishes a slot in a single
    step. On two cores it runs beside the reader, and each statement of the publication
    is a step of its own, any of the reader's able to fall between two of them."""
    empty = tuple((0,) * BYTES for _ in range(4))
    # slots, index, latest, reading, writer pair/slot/byte/value/step,
    # reader step, pair, slot, byte, bytes read, last value read
    start = (empty, (0, 0), 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, (), 0)
    seen, todo = {start}, deque([start])
    while todo:
        state = todo.popleft()
        (slots, index, latest, reading, wpair, windex, wbyte, value, wstep,
         rstep, rpair, rindex, rbyte, got, last) = state
        reader = (rstep, rpair, rindex, rbyte, got, last)
        nexts = []
        if value <= LAST and wstep == 0:                    # write one byte
            if rstep == 3 and (wpair, windex) == (rpair, rindex):
                return f"the writer writes slot [{wpair}][{windex}] while it is read"
            slot = list(slots[2 * wpair + windex])
            slot[wbyte] = value
            written = list(slots)
            written[2 * wpair + windex] = tuple(slot)
            if wbyte + 1 < BYTES:
                nexts.append((tuple(written), index, latest, reading,
                              wpair, windex, wbyte + 1, value, 0) + reader)
            else:
                nexts.append((tuple(written), index, latest, reading,
                              wpair, windex, wbyte, value, 1) + reader)
        elif value <= LAST:                                 # publish, choose the next
            index, pair, idx = list(index), wpair, windex
            steps = order[wstep - 1:] if cores == 1 else order[wstep - 1:wstep]
            for statement in steps:
                if statement == "index":
                    index[wpair] = windex
                elif statement == "latest":
                    latest = wpair
                elif statement == "pair":
                    pair = choose[0](reading)
                else:
                    idx = choose[1](index, pair)
            done = wstep + len(steps) > len(order)
            nexts.append((slots, tuple(index), latest, reading, pair, idx,
                          0 if done else wbyte, value + 1 if done else value,
                          0 if done else wstep + len(steps)) + reader)
        writer = state[:9]
        if rstep == 0:                                      # rpair = Latest
            nexts.append(writer + (1, latest, 0, 0, (), last))
        elif rstep == 1:                                    # Reading = rpair
            nexts.append(state[:3] + (rpair,) + state[4:9] + (2, rpair, 0, 0, (), last))
        elif rstep == 2:                                    # rindex = Index[rpair]
            nexts.append(writer + (3, rpair, index[rpair], 0, (), last))
        else:                                               # read one byte
            got = got + (slots[2 * rpair + rindex][rbyte],)
            if rbyte + 1 < BYTES:
                nexts.append(writer + (3, rpair, rindex, rbyte + 1, got, last))
            elif len(set(got)) != 1:
                return f"a read mixes two records: {got}"
            elif got[0] < last:
                return f"a read returns {got[0]} after {last}"
            else:
                nexts.append(writer + (0, 0, 0, 0, (), got[0]))
        for n in nexts:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    return None


# Two cores, weakly ordered. Each core runs its statements as loads and stores that it
# may perform out of program order, as Armv8-M allows for Normal memory (DDI0553B.y,
# B7.2.3-B7.2.5): an access waits only for the earlier ones of its own core to the same
# location (or to one whose address is not known yet), for the loads its address or
# value depends on, and for a DMB between them (B7.2.11). A load may also take its value
# from a store of its own core that the other core does not see yet: Normal memory need
# not be multi-copy atomic (B7.2.2, RGJGP). Control dependencies are not counted: the
# reader's copy branches on BufferItems, never on the bytes it copies, so nothing but a
# barrier orders those loads before the next announcement. Each core may run ahead by
# LOOKAHEAD records.
#
# One record of the writer, k, writes value k + 1 into the slot chosen by record k - 1:
#   D   byte i of Slot[wpair][windex] = k + 1
#   SI  Index[wpair] = windex
#   SL  Latest = wpair
#   LR  pair = !Reading
#   LI  windex = !Index[pair]            (address from LR)
# One read, k:
#   LL  rpair = Latest
#   SR  Reading = rpair                  (value from LL)
#   LX  rindex = Index[rpair]            (address from LL)
#   RD  byte i of Slot[rpair][rindex]    (address from LL and LX)
# A barrier can stand at any of these points; the kernel's are KERNEL_BARRIERS.
WRITER_POINTS = ("before Index", "between Index and Latest", "before Reading", "after")
READER_POINTS = ("after Latest", "after Reading", "after the copy")
KERNEL_BARRIERS = (("before Index", "between Index and Latest"),
                   ("after Latest", "after Reading"))
LOOKAHEAD = 2
WEAK_LAST = 5        # records written: fewer than LAST, the states being many more
STORES = ("D", "SI", "SL", "SR")


def writer_record(k, barriers):
    ops = [("D", k, i) for i in range(BYTES)]
    for point, statements in zip(WRITER_POINTS, (("SI",), ("SL",), ("LR", "LI"), ())):
        if point in barriers:
            ops.append(("DMB", k, WRITER_POINTS.index(point)))
        ops += [(name, k, 0) for name in statements]
    return ops


def reader_record(k, barriers):
    following = ([("SR", k, 0)], [("LX", k, 0)] + [("RD", k, i) for i in range(BYTES)], [])
    ops = [("LL", k, 0)]
    for point, statements in zip(READER_POINTS, following):
        if point in barriers:
            ops.append(("DMB", k, READER_POINTS.index(point)))
        ops += statements
    return ops


def writer_choice(regs, k):
    """The pair and slot record k chose, None while unknown; record -1 is the start."""
    if k < 0:
        return (1, 1)
    pair, index = regs.get(("LR", k)), regs.get(("LI", k))
    return None if pair is None or index is None else (pair, index)


def location(op, regs):
    """(variable, location) of an access, location None while its address is unknown."""
    name, k, i = op
    if name in ("D", "SI"):
        choice = writer_choice(regs, k - 1)
        if choice is None:
            return ("Slot" if name == "D" else "Index", None)
        return ("Slot", (choice[0], choice[1], i)) if name == "D" else ("Index", choice[0])
    if name in ("SL", "LL"):
        return ("Latest", 0)
    if name in ("LR", "SR"):
        return ("Reading", 0)
    if name == "LI":
        return ("Index", regs.get(("LR", k)))
    if name == "LX":
        return ("Index", regs.get(("LL", k)))
    if name == "RD":
        pair, index = regs.get(("LL", k)), regs.get(("LX", k))
        return ("Slot", None if pair is None or index is None else (pair, index, i))
    return ("DMB", None)


def dependencies_met(op, regs):
    name, k, _ = op
    if name in ("D", "SI", "SL"):
        return writer_choice(regs, k - 1) is not None
    if name == "LI":
        return ("LR", k) in regs
    if name in ("SR", "LX"):
        return ("LL", k) in regs
    if name == "RD":
        return ("LX", k) in regs
    return True


def stored(op, regs):
    name, k, _ = op
    if name == "D":
        return k + 1
    if name == "SR":
        return regs[("LL", k)]
    choice = writer_choice(regs, k - 1)
    return choice[1] if name == "SI" else choice[0]


def performable(pending, j, regs):
    """None if the access cannot be performed yet, else (the pending store of its own
    core it takes its value from, or None)."""
    op = pending[j]
    if op[0] == "DMB":
        return (None,) if j == 0 else None
    if not dependencies_met(op, regs):
        return None
    variable, where = location(op, regs)
    source = None
    for earlier in pending[:j]:
        if earlier[0] == "DMB":
            return None
        evariable, ewhere = location(earlier, regs)
        if evariable != variable:
            continue
        if ewhere is None or where is None:
            return None
        if ewhere == where:
            if op[0] in STORES or earlier[0] not in STORES:
                return None
            source = earlier
    return (source,)


def explore_weak(barriers=KERNEL_BARRIERS):
    """Every execution of the two cores under weak ordering; None, or what went wrong."""
    wbarriers, rbarriers = barriers
    memory = {("Latest", 0): 0, ("Reading", 0): 0, ("Index", 0): 0, ("Index", 1): 0}
    memory.update({("Slot", (p, x, b)): 0
                   for p in range(2) for x in range(2) for b in range(BYTES)})

    def refill(pending, following, record, limit):
        pending = list(pending)
        while len({op[1] for op in pending}) < LOOKAHEAD and following < limit:
            pending += record(following)
            following += 1
        return tuple(pending), following

    def frozen(mapping):
        return tuple(sorted(mapping.items(), key=repr))

    writer = refill((), 0, lambda k: writer_record(k, wbarriers), WEAK_LAST)
    reader = refill((), 0, lambda k: reader_record(k, rbarriers), float("inf"))
    # memory, the writer's pending accesses, next record and registers, the reader's
    # (its records renumbered from the first not read whole), the bytes read, and the
    # last value read
    start = (frozen(memory), writer[0], writer[1], (), reader[0], reader[1], (), (), 0)
    seen, todo = {start}, deque([start])
    while todo:
        mem, wpending, wnext, wregs, rpending, rnext, rregs, read, last = todo.popleft()
        for side, pending, regs in (("w", wpending, dict(wregs)), ("r", rpending, dict(rregs))):
            for j, op in enumerate(pending):
                verdict = performable(pending, j, regs)
                if verdict is None:
                    continue
                source = verdict[0]
                memory, newregs, bytes_read = dict(mem), dict(regs), dict(read)
                name, k, i = op
                if name in STORES:
                    memory[location(op, regs)] = stored(op, regs)
                elif name != "DMB":
                    value = stored(source, regs) if source else memory[location(op, regs)]
                    if name in ("LR", "LI"):
                        newregs[(name, k)] = 1 - value
                    elif name == "RD":
                        bytes_read[(k, i)] = value
                    else:
                        newregs[(name, k)] = value
                rest = pending[:j] + pending[j + 1:]
                if side == "w":
                    rest, following = refill(rest, wnext, lambda n: writer_record(n, wbarriers),
                                             WEAK_LAST)
                    oldest = min([o[1] for o in rest] + [following])
                    newregs = {key: v for key, v in newregs.items() if key[1] >= oldest - 1}
                    state = [rest, following, newregs, rpending, rnext, dict(rregs)]
                else:
                    rest, following = refill(rest, rnext, lambda n: reader_record(n, rbarriers),
                                             float("inf"))
                    oldest = min([o[1] for o in rest] + [following])
                    newregs = {key: v for key, v in newregs.items() if key[1] >= oldest}
                    state = [wpending, wnext, dict(wregs), rest, following, newregs]
                done, newest = 0, last
                while all((done, b) in bytes_read for b in range(BYTES)):
                    got = tuple(bytes_read.pop((done, b)) for b in range(BYTES))
                    if len(set(got)) != 1:
                        return f"a read mixes two records: {got}"
                    if got[0] < newest:
                        return f"a read returns {got[0]} after {newest}"
                    done, newest = done + 1, got[0]
                if done:           # renumber the reader's records from the first unread
                    state[3] = tuple((o[0], o[1] - done, o[2]) for o in state[3])
                    state[4] -= done
                    state[5] = {(n, r - done): v for (n, r), v in state[5].items()}
                    bytes_read = {(r - done, b): v for (r, b), v in bytes_read.items()}
                following_state = (frozen(memory), state[0], state[1], frozen(state[2]),
                                   state[3], state[4], frozen(state[5]), frozen(bytes_read),
                                   newest)
                if following_state not in seen:
                    seen.add(following_state)
                    todo.append(following_state)
    return None


def main():
    ok = True
    for cores in (1, 2):
        print(f"{'one core, the writer an interrupt' if cores == 1 else 'two cores'}:")
        failure = explore(KERNEL, cores)
        print(f"  the kernel's 4-slot buffer {'holds' if failure is None else 'FAILS: ' + failure}")
        ok = ok and failure is None
        for name, choose in (("writing into the reader's pair", READERS_PAIR),
                             ("writing over the latest slot", LATEST_SLOT)):
            failure = explore(choose, cores)
            print(f"  a writer {name} is caught: {failure or 'NOT CAUGHT'}")
            ok = ok and failure is not None
        # Publishing the pair before its slot is harmless while the publication is one
        # step, and must be caught once a reader can come in between.
        failure = explore(KERNEL, cores, LATEST_FIRST)
        if cores == 1:
            print(f"  a writer publishing the pair first holds: {failure or 'yes'}")
            ok = ok and failure is None
        else:
            print(f"  a writer publishing the pair first is caught: {failure or 'NOT CAUGHT'}")
            ok = ok and failure is not None
    print("two cores, each free to reorder its accesses as Armv8-M allows:")
    failure = explore_weak()
    print(f"  the 4-slot buffer with the kernel's four DMB "
          f"{'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    for side in (0, 1):
        for point in KERNEL_BARRIERS[side]:
            fewer = [list(KERNEL_BARRIERS[0]), list(KERNEL_BARRIERS[1])]
            fewer[side].remove(point)
            failure = explore_weak((tuple(fewer[0]), tuple(fewer[1])))
            who = "writer" if side == 0 else "reader"
            print(f"  without the {who}'s DMB {point}, caught: {failure or 'NOT CAUGHT'}")
            ok = ok and failure is not None
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
