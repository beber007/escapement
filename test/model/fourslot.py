#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Exhaustive check of the 4-slot buffer of the kernels, on one core.

OSWriteBuffer and GetReadyBuffer4Slot (EscapementHard.c and its two siblings) follow
H. R. Simpson, "Four-slot fully asynchronous communication mechanism", IEE Proceedings E,
137(1), 1990 (doi:10.1049/ip-e.1990.0002), and are modelled statement by statement. The writer is an interrupt handler: each of its steps
runs whole between two steps of the reader, a task — one core, the writer preempting.
A writer step either writes one byte into the slot chosen in advance, or, once that
slot is full, publishes it and chooses the next one. The reader takes the latest pair,
announces it, takes the latest slot of that pair, then reads the slot byte by byte.

Every interleaving is explored, and two properties checked, those J. Rushby
model-checked for Simpson's algorithm (SRI, 2002):
  - coherence: the writer never writes the slot the reader is reading, so no read
    mixes the bytes of two records;
  - sequencing: the values read never go backwards.

Two faulty writers are checked too, and must be caught: they show the check can fail.

    python3 test/model/fourslot.py
"""
import sys
from collections import deque

BYTES = 2      # bytes per slot, so that a record takes several writer steps
LAST = 7       # the writer writes 1, 2, ... LAST, each value in every byte of a slot


def kernel(index, reading):
    """Choice of the next slot, as OSWriteBuffer makes it: the pair the reader is not
    on, and in it the slot that is not its latest (Simpson's writer)."""
    pair = 1 - reading
    return pair, 1 - index[pair]


def readers_pair(index, reading):
    pair = reading
    return pair, 1 - index[pair]


def latest_slot(index, reading):
    pair = 1 - reading
    return pair, index[pair]


def explore(choose):
    """Breadth-first search of every state; returns None, or the property violated."""
    empty = tuple((0,) * BYTES for _ in range(4))
    # slots, index, latest, reading, writer pair/slot/byte/value,
    # reader step, pair, slot, byte, bytes read, last value read
    start = (empty, (0, 0), 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, (), 0)
    seen, todo = {start}, deque([start])
    while todo:
        state = todo.popleft()
        (slots, index, latest, reading, wpair, windex, wbyte, value,
         rstep, rpair, rindex, rbyte, got, last) = state
        reader = (rstep, rpair, rindex, rbyte, got, last)
        nexts = []
        if value <= LAST:                                   # a writer step
            if rstep == 3 and (wpair, windex) == (rpair, rindex):
                return f"the writer writes slot [{wpair}][{windex}] while it is read"
            slot = list(slots[2 * wpair + windex])
            slot[wbyte] = value
            written = list(slots)
            written[2 * wpair + windex] = tuple(slot)
            if wbyte + 1 < BYTES:
                nexts.append((tuple(written), index, latest, reading,
                              wpair, windex, wbyte + 1, value) + reader)
            else:                                           # publish, choose the next
                published = list(index)
                published[wpair] = windex
                pair, idx = choose(published, reading)
                nexts.append((tuple(written), tuple(published), wpair, reading,
                              pair, idx, 0, value + 1) + reader)
        writer = state[:8]
        if rstep == 0:                                      # rpair = Latest
            nexts.append(writer + (1, latest, 0, 0, (), last))
        elif rstep == 1:                                    # Reading = rpair
            nexts.append(state[:3] + (rpair,) + state[4:8] + (2, rpair, 0, 0, (), last))
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


def main():
    ok = True
    failure = explore(kernel)
    print(f"  the kernel's 4-slot buffer {'holds' if failure is None else 'FAILS: ' + failure}")
    ok = ok and failure is None
    for name, choose in (("writing into the reader's pair", readers_pair),
                         ("writing over the latest slot", latest_slot)):
        failure = explore(choose)
        print(f"  a writer {name} is caught: {failure or 'NOT CAUGHT'}")
        ok = ok and failure is not None
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
