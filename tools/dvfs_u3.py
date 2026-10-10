#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""DVFSU3 (Examples/nucleo-u385/DVFSU3.c): the phases tools/ppk2_nucleo.py read, matched
in order with the lines of USART1, and the charge a cycle and a CRC of each point.

    tools/dvfs_u3.py docs/data/dvfs-u3-2026-10-10-phases.txt docs/data/dvfs-u3-2026-10-10-lines.txt
"""
import re
import sys
from collections import defaultdict

MHZ = {0: 96, 1: 48, 2: 48, 3: 24, 4: 24, 5: 24, 6: 12, 7: 12}
NAME = ["96R1", "48R1", "48R2", "24R1", "24R2", "24R2B", "12R1", "12R2"]
PHASE_S = 20.0

phases = []
for line in open(sys.argv[1]):
    m = re.match(r"phase (\d+): (\S+)\s+([\d.]+) s, mean\s+(-?[\d.]+) uA, median\s+(-?[\d.]+)", line)
    # a phase of less than a second is the levels between two points, the three bits of
    # the point not changing at the same instant (from 24R1, 011, to 24R2, 100)
    if m and "cut" not in line and float(m.group(3)) >= 1.0:
        phases.append((m.group(2), float(m.group(3)), float(m.group(4)), float(m.group(5))))

lines = []
for line in open(sys.argv[2]):
    w = line.split()
    if len(w) == 10 and w[0] == "DVFS3":
        v = [int(x, 16) for x in w[1:]]
        lines.append({"smps": v[0], "point": v[1], "crcs": v[2], "wrong": v[3],
                      "range": v[4], "boost": v[5], "msis": v[6], "reg": v[7], "nosmps": v[8]})

# each whole phase of the PPK2 against the line sent at its end, by name and order: the
# lines come one a phase, the regulator told by them
acc = defaultdict(list)
li = 0
for name, secs, mean, median in phases:
    while li < len(lines) and NAME[lines[li]["point"]] != name:
        li += 1
    if li == len(lines):
        break
    l = lines[li]
    li += 1
    acc[(l["smps"], l["point"])].append((mean, median, l["crcs"], l["wrong"]))

print("reg   point  phases  mean uA  median uA  CRCs/20s  pC/cycle  nC/CRC  wrong")
best = {}
for (smps, point), rows in sorted(acc.items(), key=lambda k: (-k[0][0], k[0][1])):
    mean = sum(r[0] for r in rows) / len(rows)
    median = sum(r[1] for r in rows) / len(rows)
    crcs = sum(r[2] for r in rows) / len(rows)
    wrong = sum(r[3] for r in rows)
    pc = mean * 1e-6 / (MHZ[point] * 1e6) * 1e12
    nc = mean * 1e-6 * PHASE_S / crcs * 1e9
    best[(smps, point)] = nc
    print(f"{'SMPS' if smps else 'LDO ':4}  {NAME[point]:6} {len(rows):5}  {mean:8.1f}  {median:9.1f}  {crcs:8.0f}  {pc:8.1f}  {nc:6.2f}  {wrong}")
for smps in (1, 0):
    if (smps, 0) in best:
        ref = best[(smps, 0)]
        print(f"{'SMPS' if smps else 'LDO'}: charge a CRC against 96 MHz range 1: " +
              ", ".join(f"{NAME[p]} {100 * (best[(smps, p)] / ref - 1):+.1f} %"
                        for p in range(1, 8) if (smps, p) in best))
