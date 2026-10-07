#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""(m,k)-firm task sets under EDF simulated tick by tick, the optional instances admitted
by one test or another, to measure what a test would win before a kernel takes it.

tools/differential.py judges each drop of the kernel's in the schedule as it was, where
admitting one instance changes what follows. This runs the whole schedule again under
each test: a mandatory instance runs by EDF, an optional one is considered when nothing
else is due, in the order of its deadline, admitted or dropped there for good, as the
soft kernel does, then runs ahead of the mandatory instances due later, losing ties. It
shares the task sets of tools/differential.py and its two tests:

  kernel    admission_test(), the soft kernel's test, counting every mandatory instance
            released before the optional one's deadline
  demand    demand_test(), the processor demand criterion from the instant of decision
            until the processor would first be free, with no bound: exact for a kernel
            that costs nothing, too costly on the RP2040 to keep (docs/method.md)
  alone     admits whatever fits by itself: no test at all, which must miss deadlines,
            the witness that the simulation sees them

Each set runs twice, every instance taking its WCET, then a time of its own drawn from 1
tick to its WCET. Printed: the share of the optional instances that ran, and the deadlines
missed, mandatory or admitted.

  python3 tools/firm_admission.py          2000 task sets
  python3 tools/firm_admission.py 300
"""

import importlib.util
import os
import random
import sys

_spec = importlib.util.spec_from_file_location(
    "differential", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "differential.py"))
differential = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(differential)

DURATION = differential.DURATION
TESTS = {
    "kernel": lambda tasks, i, target, t: differential.admission_test(tasks, i, target, t),
    "demand": lambda tasks, i, target, t: differential.demand_test(tasks, i, target, t)[0],
    "alone": lambda tasks, i, target, t: target[1] - tasks[i]["wcet"] >= t,
}


def simulate(tasks, test, takes):
    """Runs tasks for DURATION ticks, instance k of task i taking takes(i, k) ticks: the
    optional instances that ran, those released, and the deadlines missed."""
    # job: [release, deadline, ticks left, optional, state, task, promoted]
    jobs, ran, released, missed = [], 0, 0, 0
    for now in range(DURATION + 1):
        for i, task in enumerate(tasks):
            if now % task["period"] == 0:
                k = now // task["period"]
                for job in jobs:                   # not started by its task's next release
                    if job[5] == i and job[3] and job[4] == "wait":
                        job[4] = "drop"
                optional = not differential.mandatory(k, task["m"], task["k"])
                released += optional
                jobs.append([now, now + task["deadline"], takes(i, k), optional,
                             "wait" if optional else "run", i, False])
        for job in jobs:
            if job[4] == "run" and job[2] > 0 and now >= job[1]:
                missed += 1
                job[4] = "missed"
        due = [job for job in jobs if job[4] == "run" and job[2] > 0]
        if not due:
            for job in sorted((j for j in jobs if j[4] == "wait"), key=lambda j: j[1]):
                if test(tasks, job[5], [job[0], job[1], job[1]], now):
                    job[4], job[6] = "run", True
                    ran += 1
                    due = [job]
                    break
                job[4] = "drop"
        if due and now < DURATION:
            min(due, key=lambda j: (j[1], j[6]))[2] -= 1
        jobs = [j for j in jobs if j[4] in ("wait", "run") and not (j[4] == "run" and j[2] == 0)]
    return ran, released, missed


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rng = random.Random("firm-admission")
    totals = {name: [0, 0, 0] for name in TESTS}
    for n in range(count):
        tasks = differential.draw_firm(rng, False)
        for light in (False, True):
            each = random.Random("firm-admission-%d" % n)
            durations = {(i, k): each.randint(1, t["wcet"]) if light else t["wcet"]
                         for i, t in enumerate(tasks)
                         for k in range(DURATION // t["period"] + 1)}
            for name, test in TESTS.items():
                result = simulate(tasks, test, lambda i, k: durations[(i, k)])
                totals[name] = [a + b for a, b in zip(totals[name], result)]
    for name, (ran, released, missed) in totals.items():
        print("%-8s %d of %d optional instances ran, %.1f %%; %d deadlines missed" %
              (name, ran, released, 100.0 * ran / released, missed))
    if totals["alone"][2] == 0:
        sys.exit("no test at all missed no deadline: the simulation does not see them")
    sys.exit(1 if any(totals[name][2] for name in TESTS if name != "alone") else 0)


if __name__ == "__main__":
    main()
