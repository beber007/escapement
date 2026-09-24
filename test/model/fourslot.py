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
    print("all checks passed" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
