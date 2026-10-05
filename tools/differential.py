#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Random task sets run by the kernels on the host, each trace checked against the
algorithm the build schedules by.

test/host's mode "trace" runs tasks that take time, with the kernel as it ships, and
prints who ran from when, for how long, at what speed, and when each instance ended.
This script draws task sets the algorithm can schedule, periodic tasks and, in half of
them, event-driven tasks signalled at random, runs them, and checks each trace on its
own, from the releases, the deadlines and the work each instance received: it shares no
code with the kernels, and does not copy how they break ties. The releases and deadlines
of event-driven tasks come from the algorithms' specification (instances()): a workload
apart under DM, a total bandwidth server under EDF. At every instant of a trace:

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
  which the host does not have);
- the timer interrupts only at a release, and once at an instant (lines "I"): the kernel
  has no periodic tick.

The speeds the power-aware kernel picks are checked apart, on every task set under OTE
and on those of periodic tasks under the other policies: tools/speed_reference.py computes the speed of each dispatch from the policy's
specification, and a speed other than its own fails, a policy that runs faster than it
needs among them, which keeps every deadline.

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
_spec = importlib.util.spec_from_file_location(
    "speed_reference", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "speed_reference.py"))
speed_reference = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed_reference)

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


def periodic(wcet, period, deadline, takes):
    return {"kind": "P", "wcet": wcet, "period": period, "deadline": deadline, "takes": takes}


def firm(wcet, period, takes, m, k):
    """An (m,k)-firm task, its deadline its period: of every k instances, m mandatory."""
    return {"kind": "F", "wcet": wcet, "period": period, "deadline": period, "takes": takes,
            "m": m, "k": k}


def mandatory(j, m, k):
    """Whether instance j of an (m,k)-firm task is mandatory: the pattern that spreads m
    instances evenly over k (test/host, Mandatory), its first instance mandatory."""
    j %= k
    return j == (j * m + k - 1) // k * k // m


def event_driven(takes, workload, signals):
    return {"kind": "E", "takes": takes, "workload": workload, "signals": signals}


def draw_firm(rng, deadline_monotonic):
    """(m,k)-firm tasks, whose mandatory instances the algorithm schedules, where all their
    instances together would overload the processor: the kernel must drop optional ones,
    and admit only those that fit. Each task takes its WCET, so that an optional instance
    wrongly admitted misses its deadline in the run; main() runs each set again with tasks
    taking less, where worst_case_end() checks each one admitted. The mandatory instances
    of a task come at least floor(k/m) periods apart, which the tests take for their
    period, their deadline staying the period; the declared load of every instance, often
    past 1, forces drops."""
    while True:
        tasks = []
        for _ in range(rng.randint(2, 5)):
            period = rng.randint(20, 600)
            k = rng.randint(1, 5)
            m = rng.randint(1, k)
            wcet = rng.randint(1, max(1, period // 2))
            tasks.append(firm(wcet, period, wcet, m, k))
        spaced = [(t["wcet"], t["period"] * (t["k"] // t["m"]), t["period"]) for t in tasks]
        if deadline_monotonic:
            ok = True
            for i, (c, _, d) in enumerate(spaced):
                higher = [(cj, pj) for j, (cj, pj, dj) in enumerate(spaced) if j != i and dj <= d]
                r = c
                while ok:
                    nxt = c + sum(math.ceil(r / pj) * cj for cj, pj in higher)
                    if nxt > d:
                        ok = False
                    elif nxt == r:
                        break
                    r = nxt
            if ok:
                return tasks
        else:
            # Deadlines shorter than the spacing: utilization does not decide (a (1,4) task
            # of period 537 still has 537 to end in). Spuri's analysis, exact for such
            # sporadic tasks under EDF, decides.
            analysed = [{"wcet": c, "period": p, "deadline": d} for c, p, d in spaced]
            if all(response_times.edf_response_time(i, analysed, (0, 0, 0)) is not None
                   for i in range(len(analysed))):
                return tasks


def draw(rng, deadline_monotonic):
    """A task set the algorithm schedules: periodic tasks, and up to two event-driven ones
    signalled at random, at least twice the sum of their workloads apart, so that each
    instance has ended, by its deadline, before its next signal."""
    while True:
        tasks = []
        for _ in range(rng.randint(1, 6)):
            period = rng.randint(10, 600)
            deadline = rng.randint(max(2, period // 2), period)
            wcet = rng.randint(1, max(1, deadline // 4))
            tasks.append(periodic(wcet, period, deadline, rng.randint(1, wcet)))
        events = rng.choice([0, 0, 1, 2])
        workloads = [rng.randint(20, 400) for _ in range(events)]
        for workload in workloads:
            gap = 2 * sum(workloads)
            signals, t = [], rng.randint(1, 300)
            while t < DURATION:
                signals.append(t)
                t += rng.randint(gap, 2 * gap)
            tasks.append(event_driven(rng.randint(1, max(1, workload // 4)), workload, signals))
        if deadline_monotonic and response_times_hold(tasks):
            return tasks
        load = sum(t["wcet"] / t["deadline"] if t["kind"] == "P" else t["takes"] / t["workload"]
                   for t in tasks)
        if not deadline_monotonic and load <= 1:
            return tasks


def response_times_hold(tasks):
    """Response-time analysis for fixed priorities by deadline, every task released at
    time 0, an event-driven task as a sporadic one of period and deadline its workload:
    R = C + sum over higher priorities of ceil(R / P) C, at most the deadline. Tasks of
    equal deadline are counted as interfering both ways."""
    flat = [(t["wcet"], t["period"], t["deadline"]) if t["kind"] == "P" else
            (t["takes"], t["workload"], t["workload"]) for t in tasks]
    for i, (c, _, d) in enumerate(flat):
        higher = [(cj, pj) for j, (cj, pj, dj) in enumerate(flat) if j != i and dj <= d]
        r = c
        while True:
            nxt = c + sum(math.ceil(r / pj) * cj for cj, pj in higher)
            if nxt > d:
                return False
            if nxt == r:
                break
            r = nxt
    return True


def instances(tasks, dm):
    """Every instance the run must hold, from the specification of the algorithms rather
    than the kernels: (task, release, deadline, key), the key what decides between waiting
    instances, the absolute deadline under EDF, the relative one under DM. A periodic task
    is released every period from 0. An event-driven task under DM is released at its
    signal, or a workload after its previous release if that is later, its deadline a
    workload after; under EDF the event-driven tasks share a total bandwidth server
    (Spuri and Buttazzo, 1996): an instance released at r gets the deadline
    D = max(D, r) + workload, D the last deadline given to any of them."""
    out = []
    for i, t in enumerate(tasks):
        if t["kind"] in "PF":
            k = 0
            while k * t["period"] <= DURATION:
                r = k * t["period"]
                optional = t["kind"] == "F" and not mandatory(k, t["m"], t["k"])
                out.append((i, r, r + t["deadline"], t["deadline"] if dm else r + t["deadline"],
                            optional))
                k += 1
    signals = sorted((s, i) for i, t in enumerate(tasks) if t["kind"] == "E"
                     for s in t["signals"])
    server, last = 0, {}
    for s, i in signals:
        w = tasks[i]["workload"]
        if dm:
            r = max(s, last.get(i, -w) + w)
            last[i] = r
            out.append((i, r, r + w, w, False))
        else:
            r = max(s, last.get(i, 0))
            server = server + w if server > r else r + w
            last[i] = server
            out.append((i, r, server, server, False))
    return sorted(out, key=lambda x: (x[1], x[0]))


def run(build, tasks):
    binary = os.path.join(HOST, os.environ.get("BUILD", "build"), "test_scheduler_" + build)
    lines = []
    for t in tasks:
        if t["kind"] == "P":
            lines.append("P %d %d %d %d" % (t["wcet"], t["period"], t["deadline"], t["takes"]))
        elif t["kind"] == "F":
            lines.append("F %d %d %d %d %d %d" % (t["wcet"], t["period"], t["deadline"],
                                                  t["takes"], t["m"], t["k"]))
        else:
            lines.append("E %d %d" % (t["takes"], t["workload"]))
    for s, i in sorted((s, i) for i, t in enumerate(tasks) if t["kind"] == "E"
                       for s in t["signals"]):
        lines.append("S %d %d" % (s, i))
    # What OSMalloc hands out is never freed, by design: test/host/Makefile turns the leak
    # check of AddressSanitizer off, which Linux runs and macOS does not.
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    done = subprocess.run([binary, "trace", str(DURATION)], input="\n".join(lines) + "\n",
                          capture_output=True, text=True, timeout=60, env=env)
    if done.returncode != 0:
        said = [l for l in (done.stdout + done.stderr).splitlines()
                if l.strip() and not l[:2] in ("S ", "E ")]
        raise Failure("the run failed: %s" % (said[-1] if said else "exit %d" % done.returncode))
    return done.stdout


def worst_case_end(tasks, jobs, todo, i, target, t, dm):
    """When the optional instance target of task i, starting at t, would end were every
    instance to take its WCET: the instances pending at t, mandatory or optional and
    started, their WCET less the work they received; every mandatory instance released
    before its deadline; and the instance itself, which loses every tie. Optional instances
    not yet started never delay it: the kernel considers one only when no other instance
    waits (EDF), or behind the instances promoted to their base priority (DM). Event-driven
    tasks are left out, as their worst case is the kernel's reservation, not a schedule."""
    ready = []                                    # [release, key, ticks left, target]
    for j, pending in jobs.items():
        for job in pending:
            if job is target:
                continue
            if not job[4] or job[5]:
                received = tasks[j]["takes"] * 256 - job[3]
                left = -(-(tasks[j]["wcet"] * 256 - received) // 256)
                if left > 0:
                    ready.append([job[0], job[2], left, False])
    for j, r, _, key, optional in todo:
        if r < target[1] and not optional:
            ready.append([r, key, tasks[j]["wcet"], False])
    ready.append([t, target[2], tasks[i]["wcet"], True])
    now = t
    while True:
        released = [x for x in ready if x[0] <= now]
        if not released:
            now = min(x[0] for x in ready)
            continue
        run = min(released, key=lambda x: (x[1], x[3]))
        later = [x[0] for x in ready if x[0] > now]
        step = min([run[2]] + [r - now for r in later])
        run[2] -= step
        now += step
        if run[2] == 0:
            if run[3]:
                return now
            ready.remove(run)


def check(build, tasks, trace, speeds=True):
    """Raises Failure at the first point the trace breaks the algorithm, or, speeds
    True, a speed the power-aware policy of build would not pick."""
    dm = build in DEADLINE_MONOTONIC
    power_aware = build.startswith("pa_")
    todo = instances(tasks, dm)                   # not yet released, in release order
    # pending: [release, deadline, key, work, optional, started, admissible]; an optional
    # instance of an (m,k)-firm task may be dropped, and is not waited for until it starts;
    # admissible, it may have been admitted at an end where nothing else was due
    jobs = {i: [] for i in range(len(tasks))}
    longest = [0] * len(tasks)                    # the longest response seen, per task

    def release_until(t):
        while todo and todo[0][1] <= t:
            i, r, d, k, optional = todo.pop(0)
            # An optional instance not started when its task arrives again is dropped.
            jobs[i] = [j for j in jobs[i] if not (j[4] and not j[5])]
            jobs[i].append([r, d, k, tasks[i]["takes"] * 256, optional, False, False])

    def due(i):
        """The instance of task i that must run: mandatory, or optional and started."""
        for j in jobs[i]:
            if not j[4] or j[5]:
                return j
        return None

    def key(i):
        return due(i)[2]

    def waiting():
        return [i for i in jobs if due(i) is not None]

    now = 0
    pending_end = None                            # (time, task) the last segment must end
    release_times = {x[1] for x in todo}
    last_interrupt = None
    for line in trace.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "I":
            t = int(fields[1])
            if t not in release_times:
                raise Failure("t=%d: the timer interrupts where nothing is released" % t)
            if t == last_interrupt:
                raise Failure("t=%d: the timer interrupts twice at one instant" % t)
            last_interrupt = t
            continue
        if fields[0] == "E":
            t, i = int(fields[1]), int(fields[2])
            if pending_end != (t, i):
                raise Failure("t=%d: task %d ends, expected %s" % (t, i, pending_end))
            pending_end = None
            # The end is served before a release at the same instant: with no other
            # instance due, the kernel may admit an optional one there, which that release
            # then preempts before it has run (admitted below).
            release_until(t - 1)
            if not [j for j in waiting() if due(j)[0] < t]:
                for pending in jobs.values():
                    for job in pending:
                        if job[4] and not job[5] and job[0] < t:
                            job[6] = True
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
        later = [x for x in todo if t < x[1] < t + step and not x[4]]
        if i < 0:
            if waiting():
                raise Failure("t=%d: idle while task %s waits" % (t, waiting()))
            if later:
                raise Failure("t=%d: idle through the release of task %d at %d" %
                              (t, later[0][0], later[0][1]))
            now = t + step
            continue
        if not jobs[i]:
            raise Failure("t=%d: task %d runs with no instance released" % (t, i))
        job = due(i) or jobs[i][0]
        if job[4] and not job[5]:
            # An optional instance starts only when no other instance must run. An instance
            # released at this very instant does not count: a task ending at it is served
            # before the timer's release, and may hand over to the optional one first.
            before = [j for j in waiting() if due(j)[0] < t]
            if before and not job[6]:
                raise Failure("t=%d: an optional instance of task %d starts while %s must "
                              "run" % (t, i, before))
            if all(x["kind"] in "PF" for x in tasks):
                end = worst_case_end(tasks, jobs, todo, i, job, t, dm)
                if end > job[1]:
                    raise Failure("t=%d: an optional instance of task %d starts that, every "
                                  "task taking its WCET, ends at %d, past its deadline %d" %
                                  (t, i, end, job[1]))
            job[5] = True
        best = min(key(j) for j in waiting())
        if key(i) > best:
            raise Failure("t=%d: task %d runs (%s %d) while %s waits with %d" %
                          (t, i, "deadline" if dm else "absolute deadline", key(i),
                           [j for j in waiting() if key(j) == best], best))
        for j, r, _, k, _ in later:
            if k < key(i):
                raise Failure("t=%d: task %d released at %d, before task %d, does not "
                              "preempt it" % (t, j, r, i))
        rate = RATES[speed] if power_aware else 256
        to_end = -(-job[3] // rate)
        if step > to_end:
            raise Failure("t=%d: task %d runs %d ticks past its work" % (t, i, step - to_end))
        job[3] -= step * rate
        now = t + step
        if job[3] <= 0:
            if now > job[1]:
                raise Failure("task %d: the instance released at %d ends at %d, past its "
                              "deadline %d" % (i, job[0], now, job[1]))
            jobs[i].remove(job)
            longest[i] = max(longest[i], now - job[0])
            pending_end = (now, i)
    if pending_end is not None:
        raise Failure("task %d did not end at %d" % (pending_end[1], pending_end[0]))
    release_until(now - 1)
    for i in jobs:
        for job in jobs[i]:
            if job[1] <= now and (not job[4] or job[5]):
                raise Failure("task %d: the instance released at %d never ended by its "
                              "deadline %d" % (i, job[0], job[1]))
    if speeds and power_aware and (build in ("pa_edf", "pa_dm") or
                                   all(t["kind"] == "P" for t in tasks)):
        try:
            speed_reference.check_speeds(build, tasks, trace, instances(tasks, dm))
        except speed_reference.Failure as e:
            raise Failure(str(e))
    if build in ("hard_edf", "soft_edf") and all(t["kind"] == "P" for t in tasks):
        analysed = [{"period": t["period"], "deadline": t["deadline"], "wcet": t["wcet"]}
                    for t in tasks]
        for i in range(len(tasks)):
            bound = response_times.edf_response_time(i, analysed, (0, 0, 0))
            if bound is None or longest[i] > bound:
                raise Failure("task %d responded in %d, past its bound by analysis, %s" %
                              (i, longest[i], bound))


def self_test():
    """Traces made wrong by hand must fail, each for its reason."""
    tasks = [periodic(20, 100, 100, 15), periodic(30, 150, 120, 30)]
    good = "S 0 15 0 2\nE 15 0\nS 15 30 1 2\nE 45 1\nS 45 55 -1 0\nI 100\nS 100 15 0 2\n" \
           "E 115 0\nS 115 35 -1 0\nI 150\nS 150 30 1 2\nE 180 1\nS 180 20 -1 0\nT 200\n"
    global DURATION
    check("pa_edf", tasks, good, speeds=False)
    wrong = {
        "the wrong task first": good.replace("S 0 15 0 2\nE 15 0\nS 15 30 1 2\nE 45 1",
                                             "S 0 30 1 2\nE 30 1\nS 30 15 0 2\nE 45 0"),
        "idle while waiting": good.replace("S 45 55 -1 0\nI 100\nS 100 15 0 2",
                                           "S 45 60 -1 0\nI 100\nS 105 10 0 2"),
        "an end too early": good.replace("S 0 15 0 2\nE 15 0\nS 15 30 1 2",
                                         "S 0 10 0 2\nE 10 0\nS 10 35 1 2"),
        "too slow for the deadline": "S 0 15 0 2\nE 15 0\nS 15 109 1 0\nE 124 1\n",
        "no end printed": good.replace("E 45 1\n", ""),
        "a tick": good.replace("S 45 55 -1 0\n", "S 45 5 -1 0\nI 50\nS 50 50 -1 0\n"),
        "two interrupts at one instant": good.replace("I 100\n", "I 100\nI 100\n"),
    }
    for name, trace in wrong.items():
        try:
            check("pa_edf", tasks, trace, speeds=False)
        except Failure:
            continue
        sys.exit("self-test: a trace with %s passed" % name)
    # An event-driven task of workload 300 signalled at 50, then at 95, beside the first
    # periodic task: released at its signal, and preempted at 100 by the periodic instance
    # due at 200, its own deadline the server's, 395.
    events = [periodic(20, 100, 100, 15), event_driven(10, 300, [50])]
    good = "S 0 15 0 2\nE 15 0\nS 15 35 -1 0\nS 50 10 1 2\nE 60 1\nS 60 40 -1 0\n" \
           "S 100 15 0 2\nE 115 0\nS 115 85 -1 0\nT 200\n"
    check("pa_edf", events, good, speeds=False)
    late = events[:1] + [event_driven(10, 300, [95])]
    wrong_events = {
        "an event released late": (events, good.replace(
            "S 15 35 -1 0\nS 50 10 1 2\nE 60 1\nS 60 40 -1 0",
            "S 15 40 -1 0\nS 55 10 1 2\nE 65 1\nS 65 35 -1 0")),
        "an event not preempted by an earlier deadline": (late,
            "S 0 15 0 2\nE 15 0\nS 15 80 -1 0\nS 95 10 1 2\nE 105 1\nS 105 15 0 2\n"
            "E 120 0\nS 120 80 -1 0\nT 200\n"),
    }
    for name, (tasks, trace) in wrong_events.items():
        try:
            check("pa_edf", tasks, trace, speeds=False)
        except Failure:
            continue
        sys.exit("self-test: a trace with %s passed" % name)
    # The speeds, on a trace of the kernel under OTE: the second task, alone from 15, has
    # 30 ticks of WCET to do in the 85 before the first one's release at 100, which 102/256
    # of the fastest speed does; the first, alone at 200, 20 in the 100 before 300.
    tasks = [periodic(20, 100, 100, 15), periodic(30, 150, 120, 30)]
    good = "S 0 15 0 2\nE 15 0\nS 15 76 1 1\nE 91 1\nS 91 9 -1 0\nI 100\nS 100 15 0 2\n" \
           "E 115 0\nS 115 35 -1 0\nI 150\nS 150 30 1 2\nE 180 1\nS 180 20 -1 0\nI 200\n" \
           "S 200 38 0 1\nE 238 0\nS 238 62 -1 0\nT 300\n"
    check("pa_edf", tasks, good)
    wrong_speeds = {
        "faster than OTE needs": good.replace("S 15 76 1 1", "S 15 76 1 2"),
        "slower than OTE allows": good.replace("S 15 76 1 1", "S 15 76 1 0"),
        "faster alone at 200": good.replace("S 200 38 0 1", "S 200 38 0 2"),
        "a change of speed without a dispatch": good.replace(
            "S 200 38 0 1\n", "S 200 20 0 1\nS 220 18 0 2\n"),
    }
    for name, trace in wrong_speeds.items():
        try:
            speed_reference.check_speeds("pa_edf", tasks, trace, instances(tasks, False))
        except speed_reference.Failure:
            continue
        sys.exit("self-test: a trace with %s passed" % name)
    # With an event-driven task signalled at 50, its deadline 350: the periodic task alone
    # at 300 may stretch to 350 only, its 20 ticks in 50 needing the fastest speed; and
    # the event-driven instance runs at the fastest.
    events = [periodic(20, 100, 100, 15), event_driven(10, 300, [50])]
    good = "S 0 15 0 2\nE 15 0\nS 15 35 -1 0\nS 50 10 1 2\nE 60 1\nS 60 40 -1 0\n" \
           "I 100\nS 100 38 0 1\nE 138 0\nS 138 62 -1 0\nI 200\nS 200 38 0 1\nE 238 0\n" \
           "S 238 62 -1 0\nI 300\nS 300 15 0 2\nE 315 0\nS 315 85 -1 0\nT 400\n"
    check("pa_edf", events, good)
    wrong_events = {
        "a stretch past an event-driven release": good.replace("S 300 15 0 2", "S 300 15 0 1"),
        "an event-driven instance slowed down": good.replace("S 50 10 1 2", "S 50 10 1 1"),
    }
    for name, trace in wrong_events.items():
        try:
            speed_reference.check_speeds("pa_edf", events, trace, instances(events, False))
        except speed_reference.Failure:
            continue
        sys.exit("self-test: a trace with %s passed" % name)
    wrong_speeds.update(wrong_events)
    print("self-test: %d faulty traces caught" %
          (len(wrong) + len(wrong_events) + len(wrong_speeds)))


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
            if build.startswith("soft") and rng.random() < 0.5:
                tasks = draw_firm(rng, build in DEADLINE_MONOTONIC)
            else:
                tasks = draw(rng, build in DEADLINE_MONOTONIC)
            try:
                check(build, tasks, run(build, tasks))
                if any(t["kind"] == "F" for t in tasks):
                    light = random.Random("%s-%d-%d" % (build, seed, n))
                    tasks = [dict(t, takes=light.randint(1, t["wcet"])) for t in tasks]
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
