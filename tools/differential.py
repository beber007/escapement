#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Random task sets run by the kernels on the host, each trace checked against the
algorithm the build schedules by.

test/host's mode "trace" runs periodic tasks that take time, with the kernel as it ships,
and prints who ran from when, for how long, at what speed, and when each instance ended.
This script draws task sets the algorithm can schedule, runs them, and checks each trace
on its own, from the arrivals, the deadlines and the work each instance received: it
shares no code with the kernels, and does not copy how they break ties. At every instant
of a trace:

- the task running has an instance released and not done, its oldest;
- no instance waiting has a deadline strictly earlier (EDF, EDF* of the power-aware
  kernel's DRA and DR_OTE), or a relative deadline strictly shorter (deadline-monotonic
  scheduling, DM_SLACK): released during the run of another, such an instance preempts
  it there;
- the processor does not idle while an instance waits, nor through a release;
- an instance ends when it has received its work, done at the rate of each speed, and
  then only;
- it ends by its deadline: the task sets are drawn schedulable, by density for EDF and
  by response-time analysis for deadline-monotonic scheduling, the power-aware kernels
  having to keep that at the speeds they pick;
- under EDF at the fastest speed (hard_edf, soft_edf), within the worst response
  Spuri's analysis gives its task (tools/response_times.py, without the kernel's costs,
  which the host does not have).

What it does not check: the speeds the power-aware kernel picks, beyond that they keep
the deadlines. A policy that runs faster than it needs passes.

  python3 tools/differential.py                 every build, 300 task sets each
  python3 tools/differential.py hard_edf 2000   one build, more sets
  python3 tools/differential.py --self-test     faulty traces must be caught
  BUILD=build-O2 python3 tools/differential.py  the binaries of another build of test/host
"""

import importlib.util
import math
import os
import random
import subprocess
import sys

_spec = importlib.util.spec_from_file_location(
    "response_times", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "response_times.py"))
response_times = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(response_times)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
HOST = os.path.join(ROOT, "test", "host")
BUILDS = ["hard_edf", "hard_dm", "soft_edf", "soft_dm", "pa_edf", "pa_dm", "pa_dra",
          "pa_drote", "pa_dmslack"]
DEADLINE_MONOTONIC = {"hard_dm", "soft_dm", "pa_dm", "pa_dmslack"}
# Work done per tick at each operating point of the host's power-aware build, in 256ths,
# as test/host/host_port.c gives them; the other builds run at the fastest only.
RATES = {0: 24, 1: 102, 2: 256}
DURATION = 6000
MAX_TASKS = 8


class Failure(Exception):
    pass


def draw(rng, deadline_monotonic):
    """A task set the algorithm schedules: (wcet, period, deadline, takes) per task."""
    while True:
        tasks = []
        for _ in range(rng.randint(1, 6)):
            period = rng.randint(10, 600)
            deadline = rng.randint(max(2, period // 2), period)
            wcet = rng.randint(1, max(1, deadline // 4))
            takes = rng.randint(1, wcet)
            tasks.append((wcet, period, deadline, takes))
        if deadline_monotonic and response_times_hold(tasks):
            return tasks
        if not deadline_monotonic and sum(c / d for c, _, d, _ in tasks) <= 1:
            return tasks


def response_times_hold(tasks):
    """Response-time analysis for fixed priorities by deadline, every task released at
    time 0: R = C + sum over higher priorities of ceil(R / P) C, at most the deadline.
    Tasks of equal deadline are counted as interfering both ways."""
    for i, (c, _, d, _) in enumerate(tasks):
        higher = [(cj, pj) for j, (cj, pj, dj, _) in enumerate(tasks) if j != i and dj <= d]
        r = c
        while True:
            nxt = c + sum(math.ceil(r / pj) * cj for cj, pj in higher)
            if nxt > d:
                return False
            if nxt == r:
                break
            r = nxt
    return True


def run(build, tasks):
    binary = os.path.join(HOST, os.environ.get("BUILD", "build"), "test_scheduler_" + build)
    text = "".join("%d %d %d %d\n" % t for t in tasks)
    # What OSMalloc hands out is never freed, by design: test/host/Makefile turns the leak
    # check of AddressSanitizer off, which Linux runs and macOS does not.
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    done = subprocess.run([binary, "trace", str(DURATION)], input=text, capture_output=True,
                          text=True, timeout=60, env=env)
    if done.returncode != 0:
        said = [l for l in (done.stdout + done.stderr).splitlines()
                if l.strip() and not l[:2] in ("S ", "E ")]
        raise Failure("the run failed: %s" % (said[-1] if said else "exit %d" % done.returncode))
    return done.stdout


def check(build, tasks, trace):
    """Raises Failure at the first point the trace breaks the algorithm."""
    dm = build in DEADLINE_MONOTONIC
    power_aware = build.startswith("pa_")
    jobs = {i: [] for i in range(len(tasks))}     # pending instances: [release, deadline, work]
    released = [0] * len(tasks)                   # instances released so far, per task
    ended = [0] * len(tasks)
    longest = [0] * len(tasks)                    # the longest response seen, per task

    def release_until(t):
        for i, (_, period, deadline, takes) in enumerate(tasks):
            while released[i] * period <= t:
                r = released[i] * period
                jobs[i].append([r, r + deadline, takes * 256])
                released[i] += 1

    def key(i):
        return tasks[i][2] if dm else jobs[i][0][1]

    def waiting():
        return [i for i in jobs if jobs[i]]

    now = 0
    pending_end = None                            # (time, task) the last segment must end
    for line in trace.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "E":
            t, i = int(fields[1]), int(fields[2])
            if pending_end != (t, i):
                raise Failure("t=%d: task %d ends, expected %s" % (t, i, pending_end))
            pending_end = None
            continue
        if fields[0] == "T":
            if now != int(fields[1]):
                raise Failure("the trace stops at %d, not %s" % (now, fields[1]))
            continue
        if fields[0] != "S":
            continue
        t, step, i, speed = (int(f) for f in fields[1:])
        if pending_end is not None:
            raise Failure("t=%d: task %d did not end at %d" % (t, pending_end[1], pending_end[0]))
        if t != now or step <= 0:
            raise Failure("t=%d: a segment of %d ticks where %d was expected" % (t, step, now))
        release_until(t)
        later = sorted({k * tasks[j][1] for j in range(len(tasks))
                        for k in range(released[j], released[j] + 2)
                        if t < k * tasks[j][1] < t + step})
        if i < 0:
            if waiting():
                raise Failure("t=%d: idle while task %s waits" % (t, waiting()))
            if later:
                raise Failure("t=%d: idle through the release at %d" % (t, later[0]))
            now = t + step
            continue
        if not jobs[i]:
            raise Failure("t=%d: task %d runs with no instance released" % (t, i))
        best = min(key(j) for j in waiting())
        if key(i) > best:
            raise Failure("t=%d: task %d runs (%s %d) while %s waits with %d" %
                          (t, i, "deadline" if dm else "absolute deadline", key(i),
                           [j for j in waiting() if key(j) == best], best))
        for r in later:
            for j, (_, period, deadline, _) in enumerate(tasks):
                if r % period == 0 and (deadline if dm else r + deadline) < key(i):
                    raise Failure("t=%d: task %d released at %d, before task %d, does not "
                                  "preempt it" % (t, j, r, i))
        rate = RATES[speed] if power_aware else 256
        job = jobs[i][0]
        to_end = -(-job[2] // rate)
        if step > to_end:
            raise Failure("t=%d: task %d runs %d ticks past its work" % (t, i, step - to_end))
        job[2] -= step * rate
        now = t + step
        if job[2] <= 0:
            if now > job[1]:
                raise Failure("task %d: the instance released at %d ends at %d, past its "
                              "deadline %d" % (i, job[0], now, job[1]))
            jobs[i].pop(0)
            ended[i] += 1
            longest[i] = max(longest[i], now - job[0])
            pending_end = (now, i)
    if pending_end is not None:
        raise Failure("task %d did not end at %d" % (pending_end[1], pending_end[0]))
    release_until(now - 1)
    for i in jobs:
        for job in jobs[i]:
            if job[1] <= now:
                raise Failure("task %d: the instance released at %d never ended by its "
                              "deadline %d" % (i, job[0], job[1]))
    if build in ("hard_edf", "soft_edf"):
        analysed = [{"period": p, "deadline": d, "wcet": c} for c, p, d, _ in tasks]
        for i in range(len(tasks)):
            bound = response_times.edf_response_time(i, analysed, (0, 0, 0))
            if bound is None or longest[i] > bound:
                raise Failure("task %d responded in %d, past its bound by analysis, %s" %
                              (i, longest[i], bound))


def self_test():
    """Traces made wrong by hand must fail, each for its reason."""
    tasks = [(20, 100, 100, 15), (30, 150, 120, 30)]
    good = "S 0 15 0 2\nE 15 0\nS 15 30 1 2\nE 45 1\nS 45 55 -1 0\nS 100 15 0 2\nE 115 0\n" \
           "S 115 35 -1 0\nS 150 30 1 2\nE 180 1\nS 180 20 -1 0\nT 200\n"
    global DURATION
    check("pa_edf", tasks, good)
    wrong = {
        "the wrong task first": good.replace("S 0 15 0 2\nE 15 0\nS 15 30 1 2\nE 45 1",
                                             "S 0 30 1 2\nE 30 1\nS 30 15 0 2\nE 45 0"),
        "idle while waiting": good.replace("S 45 55 -1 0\nS 100 15 0 2",
                                           "S 45 60 -1 0\nS 105 10 0 2"),
        "an end too early": good.replace("S 0 15 0 2\nE 15 0\nS 15 30 1 2",
                                         "S 0 10 0 2\nE 10 0\nS 10 35 1 2"),
        "too slow for the deadline": "S 0 15 0 2\nE 15 0\nS 15 109 1 0\nE 124 1\n",
        "no end printed": good.replace("E 45 1\n", ""),
    }
    for name, trace in wrong.items():
        try:
            check("pa_edf", tasks, trace)
        except Failure:
            continue
        sys.exit("self-test: a trace with %s passed" % name)
    print("self-test: %d faulty traces caught" % len(wrong))


def main():
    args = sys.argv[1:]
    if args == ["--self-test"]:
        self_test()
        return
    builds = [args[0]] if args else BUILDS
    count = int(args[1]) if len(args) > 1 else 300
    seed = int(args[2]) if len(args) > 2 else 1
    failed = 0
    for build in builds:
        rng = random.Random("%s-%d" % (build, seed))
        for n in range(count):
            tasks = draw(rng, build in DEADLINE_MONOTONIC)
            try:
                check(build, tasks, run(build, tasks))
            except (Failure, subprocess.TimeoutExpired) as e:
                failed += 1
                print("%s, set %d %s: %s" % (build, n, tasks, e))
                break
        else:
            print("%s: %d task sets, every trace holds" % (build, count))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
