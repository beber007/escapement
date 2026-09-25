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
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
