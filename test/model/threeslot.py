#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Exhaustive check of the 3-slot buffer of the kernels, on one core.

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

Every interleaving is explored, and two properties checked: the writer never writes
the slot being read, the reader never takes a slot past the three, and the values read
never go backwards. Three faulty variants must be caught: a reader that does not retry
a failed SC — the kernels did not, until the model showed it — one whose SC ignores the
reservation, and a writer that may take the slot Reading names.

    python3 test/model/threeslot.py
"""
import sys
from collections import deque

BYTES = 2
LAST = 7
ASKING = 3     # Reading = 3: the reader asks for the latest slot

# next[Reading][Latest] of OSWriteBuffer: the slot that is neither
NEXT = ((1, 2, 1), (2, 2, 0), (1, 0, 0), (1, 2, 0))


def explore(retry=True, sc_checks_reservation=True, next_table=NEXT):
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
                # higher priority has cleared the reservation in between
                for nreading in ({reading, wslot} if reading == ASKING else {reading}):
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


def main():
    ok = True
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
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
