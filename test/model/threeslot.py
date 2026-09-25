#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Exhaustive check of the 3-slot buffer of the kernels, on one core and on two.

OSWriteBuffer and GetReadyBuffer3Slot (EscapementHard.c and its two siblings) follow
Chen and Burns (York, 1997), with a load-linked / store-conditional pair where they use
compare-and-swap. The model takes the pair as the Cortex-M0+ emulates it
(Escapement_Atomic.c): LL raises a single reservation flag, SC writes only if the flag
is still up and lowers it, and every interrupt clears it on its way out (_OSIOHandler).

The writer is an interrupt handler: each of its steps runs whole between two steps of
the reader, a task. A writer step either writes one byte into its slot, or, once the
slot is full, publishes it — Latest, then Reading if the reader asks for a slot, an
SC that a nested interrupt may make fail — and takes the slot neither Reading nor
Latest names. The reader asks for a slot (Reading =
3), then LL, then reads Latest, then SC — again from the LL while the SC fails and
Reading is still 3 — then takes the slot Reading names and reads it byte by byte.

On two cores, as on the RP2350, the writer runs beside the reader, one step per
statement, each core with its own reservation, which a store of the other core to
Reading clears once ACTLR.EXTEXCLALL makes the monitors see it (explore_two_cores).

Two cores also let each core's accesses be seen out of program order, Armv8-M memory
being weakly ordered: explore_weak() finds which DMB barriers the buffer needs there.

Every interleaving is explored, and two properties checked: the writer never writes
the slot being read, the reader never takes a slot past the three, and the values read
never go backwards. Faulty variants must be caught: a reader that does not retry a
failed SC — the kernels did not, until the model showed it — one whose SC ignores the
reservation, a writer that may take the slot Reading names; on two cores, monitors
local to each core, and a writer that does not retry its SC — the kernels did not,
until the two-core model showed it.

    python3 test/model/threeslot.py
"""
import sys
from collections import deque

BYTES = 2
LAST = 7
ASKING = 3     # Reading = 3: the reader asks for the latest slot

# next[Reading][Latest] of OSWriteBuffer: the slot that is neither
NEXT = ((1, 2, 1), (2, 2, 0), (1, 0, 0), (1, 2, 0))


def explore(retry=True, sc_checks_reservation=True, next_table=NEXT, writer_retries=True):
    empty = tuple((0,) * BYTES for _ in range(3))
    # slots, Latest, Reading, reservation, writer slot/byte/value,
    # reader step, value loaded by LL, Latest read for SC, slot, byte, bytes read, last
    start = (empty, 0, ASKING, False, 2, 0, 1, 0, 0, 0, 0, 0, (), 0)
    seen, todo = {start}, deque([start])
    while todo:
        state = todo.popleft()
        (slots, latest, reading, reserved, wslot, wbyte, value,
         rstep, loaded, rlatest, rslot, rbyte, got, last) = state
        reader = state[7:]
        nexts = []
        if value <= LAST:                                  # a writer step, whole
            if rstep == 5 and wslot == rslot:
                return f"the writer writes slot {wslot} while it is read"
            slot = list(slots[wslot])
            slot[wbyte] = value
            written = list(slots)
            written[wslot] = tuple(slot)
            if wbyte + 1 < BYTES:
                nexts.append((tuple(written), latest, reading, False,
                              wslot, wbyte + 1, value) + reader)
            else:
                # LL(Reading) == 3: the SC hands the slot over, unless an interrupt of
                # higher priority has cleared the reservation in between; the writer then
                # tries again, and nothing can change Reading while it runs
                handover = {wslot} if writer_retries else {reading, wslot}
                for nreading in (handover if reading == ASKING else {reading}):
                    nexts.append((tuple(written), wslot, nreading, False,
                                  next_table[nreading][wslot], 0, value + 1) + reader)
            # the interrupt clears the reservation on its way out: False above
        writer = state[:7]
        if rstep == 0:                                     # Reading = 3
            nexts.append(state[:2] + (ASKING,) + state[3:7] + (1,) + reader[1:])
        elif rstep == 1:                                   # LL(&Reading)
            nexts.append(state[:3] + (True,) + state[4:7] + (2, reading) + reader[2:])
        elif rstep == 2:                                   # evaluate Latest for the SC
            if loaded == ASKING:
                nexts.append(writer + (3, loaded, latest) + reader[3:])
            else:
                nexts.append(writer + (4,) + reader[1:])
        elif rstep == 3:                                   # SC(&Reading, Latest)
            if reserved or not sc_checks_reservation:
                nexts.append(state[:2] + (rlatest, False) + state[4:7] + (4,) + reader[1:])
            elif retry:                                    # failed: LL again
                nexts.append(writer + (1,) + reader[1:])
            else:
                nexts.append(writer + (4,) + reader[1:])
        elif rstep == 4:                                   # &Slot[Reading]
            if reading == ASKING:
                return "the reader takes slot 3, past the end of the array"
            nexts.append(writer + (5, loaded, rlatest, reading, 0, (), last))
        else:                                              # read one byte
            got = got + (slots[rslot][rbyte],)
            if rbyte + 1 < BYTES:
                nexts.append(writer + (5, loaded, rlatest, rslot, rbyte + 1, got, last))
            elif len(set(got)) != 1:
                return f"a read mixes two records: {got}"
            elif got[0] < last:
                return f"a read returns {got[0]} after {last}"
            else:
                nexts.append(writer + (0, 0, 0, 0, 0, (), got[0]))
        for n in nexts:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    return None


def explore_two_cores(retry=True, global_monitor=True, next_table=NEXT, writer_retries=True,
                      value_compare=False, writer_spurious=True):
    """The same buffer with the writer on a core of its own, as on the RP2350: every
    statement of OSWriteBuffer is a step of its own, and any of the reader's can fall
    between two of them. Each core has its own reservation, taken by its LL and lost when
    the other core stores to Reading — which on the Cortex-M33 of the RP2350 holds only
    once ACTLR.EXTEXCLALL makes the monitors see the stores of the other core; without
    it they are local (global_monitor=False). Either SC may also fail for no visible
    reason, as an SC may: an interrupt, or a store to a neighbouring byte of the same
    reservation granule. Each tries again from its LL while its SC fails and Reading is
    still 3. The writer used to try once: on one core harmless, since whatever made its
    SC fail also cleared the reader's reservation; on two, the reader's survives, its SC
    hands it the Latest it read before the writer published, and the writer, taking the
    slot that is neither Reading nor the new Latest, writes into it.

    value_compare=True is the monitor of Renode instead (1.17, tlib's
    gen_store_exclusive): the other core's stores leave a reservation alone, and an SC
    succeeds while its core's reservation stands and the location still holds what the
    LL read. writer_spurious=False takes away the writer's failures without a visible
    reason: in ThreeSlotCoresPico2 the writer runs bare on core 1 and takes no interrupt
    between its LL and its SC. The reader's stay, the kernel clearing the monitor when
    it switches context on core 0."""
    empty = tuple((0,) * BYTES for _ in range(3))
    # slots, Latest, Reading, reader's and writer's reservations,
    # writer slot/byte/value/step/value loaded by its LL,
    # reader step, value loaded by LL, Latest read for SC, slot, byte, bytes read, last
    start = (empty, 0, ASKING, False, False, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, (), 0)
    seen, todo = {start}, deque([start])
    while todo:
        state = todo.popleft()
        (slots, latest, reading, res_r, res_w, wslot, wbyte, value, wstep, wloaded,
         rstep, loaded, rlatest, rslot, rbyte, got, last) = state
        writer = (wslot, wbyte, value, wstep, wloaded)
        reader = (rstep, loaded, rlatest, rslot, rbyte, got, last)

        def with_(latest=latest, reading=reading, res_r=res_r, res_w=res_w,
                  writer=writer, reader=reader, slots=slots):
            return (slots, latest, reading, res_r, res_w) + writer + reader

        nexts = []
        clears = global_monitor and not value_compare
        if value <= LAST:                                  # a writer step
            if wstep == 0:                                 # write one byte
                if rstep == 5 and wslot == rslot:
                    return f"the writer writes slot {wslot} while it is read"
                slot = list(slots[wslot])
                slot[wbyte] = value
                written = list(slots)
                written[wslot] = tuple(slot)
                done = wbyte + 1 == BYTES
                nexts.append(with_(slots=tuple(written),
                                   writer=(wslot, 0 if done else wbyte + 1, value,
                                           1 if done else 0, 0)))
            elif wstep == 1:                               # Latest = windex
                nexts.append(with_(latest=wslot, writer=(wslot, 0, value, 2, 0)))
            elif wstep == 2:                               # LL(&Reading)
                nexts.append(with_(res_w=True, writer=(wslot, 0, value, 3, reading)))
            elif wstep == 3:                               # == 3 ? SC(&Reading, windex)
                succeeds = (wloaded == ASKING and res_w
                            and (reading == wloaded or not value_compare))
                if succeeds:                               # succeeds, or fails anyway
                    nexts.append(with_(reading=wslot, res_w=False,
                                       res_r=res_r and not clears,
                                       writer=(wslot, 0, value, 4, 0)))
                if not succeeds or writer_spurious:
                    again = wloaded == ASKING and writer_retries
                    nexts.append(with_(res_w=False,
                                       writer=(wslot, 0, value, 2 if again else 4, 0)))
            else:                                          # next[Reading][Latest]
                nexts.append(with_(writer=(next_table[reading][latest], 0, value + 1, 0, 0)))
        if rstep == 0:                                     # Reading = 3
            nexts.append(with_(reading=ASKING, res_w=res_w and not clears,
                               reader=(1,) + reader[1:]))
        elif rstep == 1:                                   # LL(&Reading)
            nexts.append(with_(res_r=True, reader=(2, reading) + reader[2:]))
        elif rstep == 2:                                   # evaluate Latest for the SC
            if loaded == ASKING:
                nexts.append(with_(reader=(3, loaded, latest) + reader[3:]))
            else:
                nexts.append(with_(reader=(4,) + reader[1:]))
        elif rstep == 3:                                   # SC(&Reading, Latest)
            succeeds = res_r and (reading == loaded or not value_compare)
            if succeeds:
                nexts.append(with_(reading=rlatest, res_r=False,
                                   res_w=res_w and not clears,
                                   reader=(4,) + reader[1:]))
            # or fails: LL again, or give up if the reader does not retry
            nexts.append(with_(res_r=False, reader=((1,) if retry else (4,)) + reader[1:]))
        elif rstep == 4:                                   # &Slot[Reading]
            if reading == ASKING:
                return "the reader takes slot 3, past the end of the array"
            nexts.append(with_(reader=(5, loaded, rlatest, reading, 0, (), last)))
        else:                                              # read one byte
            got = got + (slots[rslot][rbyte],)
            if rbyte + 1 < BYTES:
                nexts.append(with_(reader=(5, loaded, rlatest, rslot, rbyte + 1, got, last)))
            elif len(set(got)) != 1:
                return f"a read mixes two records: {got}"
            elif got[0] < last:
                return f"a read returns {got[0]} after {last}"
            else:
                nexts.append(with_(reader=(0, 0, 0, 0, 0, (), got[0])))
        for n in nexts:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    return None


# Two cores, weakly ordered, as fourslot.py models them (explore_weak there): each core
# performs its loads and stores in any order Armv8-M allows (DDI0553B.y, B7), a load may
# take its value from a store of its own core the other does not see yet, and a DMB
# orders everything before it before everything after. Two things are added here.
#
# The LL/SC loops branch. A core may perform the loads after a branch before the branch
# is decided, but no store (a control dependency): so each call is explored along every
# path its loop can take — the LL finds a request or not, the SC succeeds or fails — up
# to RETRIES attempts, and a path is dropped as soon as a load or SC it guessed turns out
# otherwise, which is how a mispredicted branch leaves no trace.
#
# The exclusive monitor is the RP2350's (datasheet 2.1.6): a reservation per core, taken
# by its LL, lost to its own SC and to any store of the other core to the 16-byte
# granule, which holds Reading and Latest both; ACTLR.EXTEXCLALL makes the monitors see
# them (global_monitor=False: they do not). An SC may also fail for no visible reason,
# an interrupt between it and its LL: the reader's always may, the kernel switching
# context on core 0; the writer's when its core takes interrupts (writer_spurious).
#
# One record of the writer, k, fills the slot chosen by record k - 1 with k + 1:
#   D   byte i of Slot[windex] = k + 1
#   SL  Latest = windex
#   LX  LL(&Reading)                     then, if it found 3,
#   SX  SC(&Reading, windex)             again from LX while the SC fails
#   LRn, LLn  windex = next[Reading][Latest]
# One read, k:
#   SA  Reading = 3
#   LX  LL(&Reading)                     then, if it found 3,
#   LLat  Latest                         (before the first LL when latest_first)
#   SX  SC(&Reading, Latest)             again from LX while the SC fails
#   LR  Reading
#   RD  byte i of Slot[Reading]
WRITER_POINTS = ("before Latest", "before the handover", "after the handover", "after")
READER_POINTS = ("after asking", "after the handover", "after the copy")
KERNEL_BARRIERS = (("before Latest", "before the handover"),
                   ("after asking", "after the copy"))
LOOKAHEAD = 2
WEAK_LAST = 3        # records written: the paths make the states many more
RETRIES = 2
STORES = ("D", "SL", "SA", "SX")


def loop_paths(retries=True):
    """The ways an LL/SC loop can go, as (LL found 3, SC succeeded or None) pairs."""
    paths = []

    def walk(prefix, attempts):
        paths.append(prefix + ((False, None),))
        if attempts:
            paths.append(prefix + ((True, True),))
            if not retries:
                paths.append(prefix + ((True, False),))
            elif attempts > 1:
                walk(prefix + ((True, False),), attempts - 1)
    walk((), RETRIES)
    return paths


def weak_writer_record(k, barriers, path):
    ops = [("D", k, i, None) for i in range(BYTES)]
    if "before Latest" in barriers:
        ops.append(("DMB", k, 0, None))
    ops.append(("SL", k, 0, None))
    if "before the handover" in barriers:
        ops.append(("DMB", k, 1, None))
    for attempt, (asking, succeeded) in enumerate(path):
        ops.append(("LX", k, attempt, asking))
        if asking:
            ops.append(("SX", k, attempt, succeeded))
    if "after the handover" in barriers:
        ops.append(("DMB", k, 2, None))
    ops += [("LRn", k, 0, None), ("LLn", k, 0, None)]
    if "after" in barriers:
        ops.append(("DMB", k, 3, None))
    return ops


def weak_reader_record(k, barriers, path, latest_first):
    ops = [("SA", k, 0, None)]
    if "after asking" in barriers:
        ops.append(("DMB", k, 0, None))
    if latest_first:
        ops.append(("LLat", k, 0, None))
    for attempt, (asking, succeeded) in enumerate(path):
        ops.append(("LX", k, attempt, asking))
        if asking:
            if not latest_first:
                ops.append(("LLat", k, attempt, None))
            ops.append(("SX", k, attempt, succeeded))
    if "after the handover" in barriers:
        ops.append(("DMB", k, 1, None))
    ops.append(("LR", k, 0, None))
    ops += [("RD", k, i, None) for i in range(BYTES)]
    if "after the copy" in barriers:
        ops.append(("DMB", k, 2, None))
    return ops


def weak_writer_slot(regs, k):
    """The slot record k chose, None while unknown; record -1 is the start."""
    if k < 0:
        return 2
    reading, latest = regs.get(("LRn", k)), regs.get(("LLn", k))
    return None if reading is None or latest is None else NEXT[reading][latest]


def weak_location(op, regs):
    name, k, i, _ = op
    if name in ("D", "RD"):
        slot = weak_writer_slot(regs, k - 1) if name == "D" else regs.get(("LR", k))
        return ("Slot", None if slot is None else (slot, i))
    if name in ("SL", "LLn", "LLat"):
        return ("Latest", 0)
    if name in ("SA", "SX", "LX", "LRn", "LR"):
        return ("Reading", 0)
    return ("DMB", None)


def weak_stored(side, op, regs, latest_first):
    name, k, attempt, _ = op
    if name == "D":
        return k + 1
    if name == "SA":
        return ASKING
    if side == "w":                                    # SL, SX
        return weak_writer_slot(regs, k - 1)
    return regs[("LLat", k, 0 if latest_first else attempt)]


def weak_performable(side, pending, j, regs, latest_first):
    """None if the access cannot be performed yet, else (the pending store of its own
    core it takes its value from, or None)."""
    op = pending[j]
    name, k, attempt, _ = op
    if name == "DMB":
        return (None,) if j == 0 else None
    if side == "w" and name in ("D", "SL", "SX") and weak_writer_slot(regs, k - 1) is None:
        return None
    if side == "r" and name == "SX" and ("LLat", k, 0 if latest_first else attempt) not in regs:
        return None
    if name == "RD" and ("LR", k) not in regs:
        return None
    variable, where = weak_location(op, regs)
    source = None
    for earlier in pending[:j]:
        if earlier[0] == "DMB":
            return None
        if name in STORES and earlier[0] in ("LX", "SX"):
            return None                                # no store past an open branch
        evariable, ewhere = weak_location(earlier, regs)
        if evariable != variable:
            continue
        if ewhere is None or where is None:
            return None
        if ewhere == where:
            if name in STORES or name == "LX" or earlier[0] not in STORES or earlier[0] == "SX":
                return None
            source = earlier
    return (source,)


def explore_weak(barriers=KERNEL_BARRIERS, latest_first=False, writer_spurious=True,
                 global_monitor=True, writer_retries=True):
    """Every execution of the two cores under weak ordering; None, or what went wrong."""
    wbarriers, rbarriers = barriers
    memory = {("Latest", 0): 0, ("Reading", 0): ASKING}
    memory.update({("Slot", (slot, b)): 0 for slot in range(3) for b in range(BYTES)})

    def frozen(mapping):
        return tuple(sorted(mapping.items(), key=repr))

    def refills(pending, following, side):
        """Every way to top the pending accesses up, one per path of the new calls."""
        results = [(tuple(pending), following)]
        while True:
            grown = []
            for accesses, n in results:
                if len({o[1] for o in accesses}) < LOOKAHEAD and (side == "r" or n < WEAK_LAST):
                    for path in loop_paths(writer_retries if side == "w" else True):
                        record = (weak_writer_record(n, wbarriers, path) if side == "w"
                                  else weak_reader_record(n, rbarriers, path, latest_first))
                        grown.append((accesses + tuple(record), n + 1))
                else:
                    grown.append((accesses, n))
            if grown == results:
                return results
            results = grown

    # memory, the writer's pending accesses, next record, registers and reservation,
    # the reader's (its records renumbered from the first not read whole), the bytes
    # read, and the last value read
    starts = [(frozen(memory), wp, wn, (), False, rp, rn, (), False, (), 0)
              for wp, wn in refills((), 0, "w") for rp, rn in refills((), 0, "r")]
    seen, todo = set(starts), deque(starts)
    while todo:
        mem, wp, wn, wregs, wres, rp, rn, rregs, rres, read, last = todo.popleft()
        for side in ("w", "r"):
            pending, regs = (wp, dict(wregs)) if side == "w" else (rp, dict(rregs))
            own, other = (wres, rres) if side == "w" else (rres, wres)
            for j, op in enumerate(pending):
                verdict = weak_performable(side, pending, j, regs, latest_first)
                if verdict is None:
                    continue
                source = verdict[0]
                name, k, attempt, guess = op
                memory, newregs, bytes_read = dict(mem), dict(regs), dict(read)
                outcomes = []          # memory, registers, own and other reservation
                if name == "DMB":
                    outcomes.append((memory, newregs, own, other))
                elif name == "SX":
                    spurious = writer_spurious if side == "w" else True
                    for succeeded in ((True, False) if own and spurious else (own,)):
                        if succeeded != guess:
                            continue           # not the path this call took
                        after = dict(memory)
                        if succeeded:
                            after[("Reading", 0)] = weak_stored(side, op, regs, latest_first)
                        outcomes.append((after, newregs, False,
                                         other and not (succeeded and global_monitor)))
                elif name in STORES:
                    where = weak_location(op, regs)
                    memory[where] = weak_stored(side, op, regs, latest_first)
                    in_granule = where[0] in ("Reading", "Latest")
                    outcomes.append((memory, newregs, own,
                                     other and not (in_granule and global_monitor)))
                else:
                    where = weak_location(op, regs)
                    value = (weak_stored(side, source, regs, latest_first) if source
                             else memory[where])
                    reserved = own
                    if name == "LX":
                        if (value == ASKING) != guess:
                            continue           # not the path this call took
                        reserved = True
                    elif name == "RD":
                        bytes_read[(k, attempt)] = value
                    elif name == "LR":
                        if value == ASKING:
                            return "the reader takes slot 3, past the end of the array"
                        newregs[("LR", k)] = value
                    elif name == "LLat":
                        newregs[("LLat", k, attempt)] = value
                    else:
                        newregs[(name, k)] = value
                    outcomes.append((memory, newregs, reserved, other))
                rest = pending[:j] + pending[j + 1:]
                for after, afterregs, afterown, afterother in outcomes:
                    for topped, following in refills(rest, wn if side == "w" else rn, side):
                        oldest = min([o[1] for o in topped] + [following])
                        keep = oldest - 1 if side == "w" else oldest
                        afterregs = {key: v for key, v in afterregs.items() if key[1] >= keep}
                        if side == "w":
                            state = [topped, following, afterregs, afterown,
                                     rp, rn, dict(rregs), afterother]
                        else:
                            state = [wp, wn, dict(wregs), afterother,
                                     topped, following, afterregs, afterown]
                        pending_bytes, done, newest = dict(bytes_read), 0, last
                        while all((done, b) in pending_bytes for b in range(BYTES)):
                            got = tuple(pending_bytes.pop((done, b)) for b in range(BYTES))
                            if len(set(got)) != 1:
                                return f"a read mixes two records: {got}"
                            if got[0] < newest:
                                return f"a read returns {got[0]} after {newest}"
                            done, newest = done + 1, got[0]
                        if done:       # renumber the reader's records from the first unread
                            state[4] = tuple((o[0], o[1] - done) + o[2:] for o in state[4])
                            state[5] -= done
                            state[6] = {(key[0], key[1] - done) + key[2:]: v
                                        for key, v in state[6].items()}
                            pending_bytes = {(r - done, b): v
                                             for (r, b), v in pending_bytes.items()}
                        successor = (frozen(after), state[0], state[1], frozen(state[2]),
                                     state[3], state[4], state[5], frozen(state[6]), state[7],
                                     frozen(pending_bytes), newest)
                        if successor not in seen:
                            seen.add(successor)
                            todo.append(successor)
    return None


def main():
    ok = True
    print("one core, the writer an interrupt, LL/SC as the Cortex-M0+ emulates it:")
    failure = explore()
    print(f"  the kernel's 3-slot buffer {'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    failure = explore(retry=False)
    print(f"  a reader that does not retry a failed SC is caught: {failure or 'NOT CAUGHT'}")
    ok = ok and failure is not None
    faulty_next = tuple(tuple(r if r < 3 else n for n in row) for r, row in enumerate(NEXT))
    for name, kwargs in (("reader whose SC ignores the reservation",
                          {"sc_checks_reservation": False}),
                         ("writer that may take the slot being read",
                          {"next_table": faulty_next})):
        failure = explore(**kwargs)
        print(f"  a {name} is caught: {failure or 'NOT CAUGHT'}")
        ok = ok and failure is not None
    print("two cores, the exclusive monitors seeing both (ACTLR.EXTEXCLALL):")
    failure = explore_two_cores()
    print(f"  the kernel's 3-slot buffer {'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    for name, kwargs in (("the monitors local to each core", {"global_monitor": False}),
                         ("a writer that does not retry a failed SC, as the kernels did",
                          {"writer_retries": False}),
                         ("a reader that does not retry a failed SC", {"retry": False}),
                         ("a writer that may take the slot being read",
                          {"next_table": faulty_next})):
        failure = explore_two_cores(**kwargs)
        print(f"  {name} is caught: {failure or 'NOT CAUGHT'}")
        ok = ok and failure is not None
    print("two cores, the monitor of Renode (an SC compares values), the writer's SC"
          " failing only for a reason:")
    renode = {"value_compare": True, "writer_spurious": False}
    failure = explore_two_cores(**renode)
    print(f"  the kernel's 3-slot buffer {'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    failure = explore_two_cores(next_table=faulty_next, **renode)
    print(f"  a writer that may take the slot being read is caught: {failure or 'NOT CAUGHT'}")
    ok = ok and failure is not None
    failure = explore_two_cores(retry=False, **renode)
    print(f"  a reader that does not retry a failed SC is caught: {failure or 'NOT CAUGHT'}")
    ok = ok and failure is not None
    failure = explore_two_cores(writer_retries=False, **renode)
    print(f"  a writer that does not retry a failed SC cannot be seen there:"
          f" {'right' if failure is None else 'WRONG, it fails: ' + failure}")
    ok = ok and failure is None
    print("two cores, each free to reorder its accesses as Armv8-M allows, the writer's"
          " core taking interrupts:")
    failure = explore_weak()
    print(f"  the 3-slot buffer with the kernel's four DMB "
          f"{'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    failure = explore_weak(latest_first=True)
    print(f"  with the reader taking Latest before its LL, "
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
    for name, kwargs in (("the monitors local to each core", {"global_monitor": False}),
                         ("a writer that does not retry a failed SC",
                          {"writer_retries": False})):
        failure = explore_weak(**kwargs)
        print(f"  {name} is caught: {failure or 'NOT CAUGHT'}")
        ok = ok and failure is not None
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
