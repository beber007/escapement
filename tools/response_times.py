#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""Worst-case response times of a task set by analysis, against those a trace shows.

Under deadline-monotonic scheduling (--algorithm dm, the default) the analysis is that
of the ZottaOS User Manual (May 2012, eq. 2.3, p. 15), response time analysis for fixed
priorities with the kernel's costs:

    R_i = C_i + sum over j of higher priority of ceil(R_i / P_j) (C_j + C_rsw)
              + sum over every j of ceil(R_i / P_j) (C_isw + C_timer + C_rsw)

C_isw is the time to save a context and enter the timer's interrupt, C_timer the
worst-case time of its handler, C_rsw the time to restore a context. Priorities are
deadline-monotonic, the shorter deadline first. The trace is the CSV of
tools/read_trace.py --csv --absolute, from a build with make TRACE=1 whose tasks leave a
mark on their pin as they start (extra 0) and end (extra 1), as TaskLEDPico's do. Several
reads of the trace may follow one another in the file: an end is paired with the start
before it only if they lie less than a period apart. Every
task is first released at the kernel's origin, TimeOrigin, then every period: the
response of an instance is its end mark less its release.

Under EDF (--algorithm edf) it is Spuri's response time analysis (M. Spuri, "Analysis
of Deadline Scheduled Real-Time Systems", INRIA RR-2772, 1996), exact without costs: the
worst response of task i comes in a busy period where every other task is released at
its start and as often as it can, and i at some offset a in it. Its jobs and those of
deadline no later than its own (ties against it) fill the processor until

    L_i(a) = sum over j != i, D_j <= a + D_i,
                 of min(ceil(L / P_j), 1 + floor((a + D_i - D_j) / P_j)) (C_j + C_rsw)
             + (1 + floor(a / P_i)) C_i
             + sum over every j of ceil(L / P_j) (C_isw + C_timer + C_rsw)

and R_i = max over a of max(C_i, L_i(a) - a), a taking the values k P_j + D_j - D_i
below the synchronous busy period. The costs are counted as eq. 2.3 counts them: each
job that runs ahead of i restores a context once it ends, and each release of any task
enters the timer. The self-test checks the analysis, without costs, against a
simulation of every offset on small task sets drawn at random.

    tools/response_times.py TRACE.csv --task PIN:PERIOD:DEADLINE[:WCET] ...
                            [--costs ISW,TIMER,RSW] [--algorithm dm|edf]
    tools/response_times.py --self-test

Times in microseconds. A task without WCET is given the longest time from a start mark to
an end mark the trace shows for it, which counts any preemption it suffered: an upper
bound, stated as such in the output. A task that marks only its start (the probe of
TaskLEDPico) needs its WCET given.
"""
import argparse
import math
import sys


def response_time(i, tasks, costs):
    """Eq. 2.3 by fixed-point iteration; None if R_i passes the deadline."""
    isw, timer, rsw = costs
    c, d = tasks[i]["wcet"], tasks[i]["deadline"]
    higher = [t for k, t in enumerate(tasks) if priority_over(k, i, tasks)]
    r = c
    while True:
        new = c + sum(math.ceil(r / t["period"]) * (t["wcet"] + rsw) for t in higher) \
                + sum(math.ceil(r / t["period"]) * (isw + timer + rsw) for t in tasks)
        if new > d:
            return None
        if new == r:
            return r
        r = new


def busy_period(tasks, costs):
    """The synchronous busy period: every task released at 0 and as often as it can, the
    kernel's costs included; None when the load reaches the processor's."""
    isw, timer, rsw = costs
    load = sum((t["wcet"] + isw + timer + 2 * rsw) / t["period"] for t in tasks)
    if load >= 1:
        return None
    length = sum(t["wcet"] + isw + timer + 2 * rsw for t in tasks)
    while True:
        new = sum(math.ceil(length / t["period"]) * (t["wcet"] + isw + timer + 2 * rsw)
                  for t in tasks)
        if new == length:
            return length
        length = new


def edf_response_time(i, tasks, costs):
    """Spuri's analysis, the costs added as eq. 2.3 adds them; None if R_i passes the
    deadline or the processor is overloaded."""
    isw, timer, rsw = costs
    length = busy_period(tasks, costs)
    if length is None:
        return None
    ti = tasks[i]
    offsets = set()
    for t in tasks:
        k = 0
        while k * t["period"] + t["deadline"] - ti["deadline"] < length:
            a = k * t["period"] + t["deadline"] - ti["deadline"]
            if a >= 0:
                offsets.add(a)
            k += 1
    worst = ti["wcet"]
    for a in sorted(offsets):
        own = (1 + math.floor(a / ti["period"])) * ti["wcet"]
        others = [t for k, t in enumerate(tasks)
                  if k != i and t["deadline"] <= a + ti["deadline"]]
        window = own
        while True:
            new = own + sum(min(math.ceil(window / t["period"]),
                                1 + math.floor((a + ti["deadline"] - t["deadline"])
                                               / t["period"])) * (t["wcet"] + rsw)
                            for t in others) \
                      + sum(math.ceil(window / t["period"]) * (isw + timer + rsw)
                            for t in tasks)
            if new == window:
                break
            window = new
        worst = max(worst, window - a)
    return worst if worst <= ti["deadline"] else None


def simulated_edf_response(i, tasks, horizon):
    """The worst response of task i by simulation, tick by tick, without costs: every
    other task released at 0 then every period, i first at each offset in turn, every
    tie of deadlines against i. The check of edf_response_time."""
    worst = 0
    for a in range(horizon):
        jobs = []                                   # [release, deadline, left, task]
        for k, t in enumerate(tasks):
            r = a if k == i else 0
            while r < a + horizon:
                jobs.append([r, r + t["deadline"], t["wcet"], k])
                r += t["period"]
        for now in range(3 * horizon):
            ready = [j for j in jobs if j[0] <= now and j[2] > 0]
            if not ready:
                continue
            job = min(ready, key=lambda j: (j[1], j[3] == i, j[0]))
            job[2] -= 1
            if job[2] == 0 and job[3] == i:
                worst = max(worst, now + 1 - job[0])
    return worst


def priority_over(k, i, tasks):
    """Whether task k has priority over task i: the shorter deadline, ties by order."""
    dk, di = tasks[k]["deadline"], tasks[i]["deadline"]
    return dk < di or (dk == di and k < i)


def read_trace(lines):
    """The kernel's origin and the (time, event, pin, extra) of a --csv --absolute trace."""
    origin, events = None, []
    for line in lines:
        line = line.strip()
        if line.startswith("# origin"):
            origin = int(line.split()[2])
        elif line and line[0].isdigit():
            t, e, a, x = (int(v) for v in line.split(","))
            events.append((t, e, a, x))
    if origin is None:
        sys.exit("no origin in the trace: read it with --csv --absolute")
    return origin, events


def observed(origin, events, tasks):
    """Per pin: the responses (end less release) and the start-to-end times seen."""
    out = {}
    for task in tasks:
        pin, period = task["pin"], task["period"]
        responses, spans, start = [], [], None
        for t, e, a, x in events:
            if e != 7 or a != pin:
                continue
            since = (t - origin) & 0xFFFFFFFF
            if x == 0:
                start = (t, since - since % period)
            elif start is not None:
                span = (t - start[0]) & 0xFFFFFFFF
                if span < period:           # not the start of an instance read earlier
                    responses.append(since - start[1])
                    spans.append(span)
                start = None
        out[pin] = (responses, spans)
    return out


def report(tasks, costs, seen, algorithm="dm"):
    analysis = edf_response_time if algorithm == "edf" else response_time
    print(f"{algorithm.upper()}, costs: C_isw {costs[0]} us, C_timer {costs[1]} us, "
          f"C_rsw {costs[2]} us")
    print(f"{'pin':>4} {'P':>7} {'D':>7} {'C':>7}  {'R analysis':>10}  "
          f"{'R seen, max':>11}  instances")
    for i, task in enumerate(tasks):
        r = analysis(i, tasks, costs)
        responses = seen.get(task["pin"], ([], []))[0] if seen else []
        c = f"{task['wcet']}" + ("*" if task.get("bound") else "")
        print(f"{task['pin']:>4} {task['period']:>7} {task['deadline']:>7} {c:>7}  "
              f"{r if r is not None else 'missed':>10}  "
              f"{max(responses) if responses else '-':>11}  {len(responses)}")
    if any(t.get("bound") for t in tasks):
        print("* the longest start-to-end time seen, preemptions included: an upper bound")


def parse_task(text):
    parts = [int(v) for v in text.split(":")]
    if len(parts) not in (3, 4):
        raise argparse.ArgumentTypeError("PIN:PERIOD:DEADLINE[:WCET]")
    task = {"pin": parts[0], "period": parts[1], "deadline": parts[2]}
    if len(parts) == 4:
        task["wcet"] = parts[3]
    return task


def self_test():
    """Known cases: a textbook set without costs, costs adding up, a missed deadline, and
    a synthetic trace."""
    # Liu and Layland's kind of example: C 1, 2, 3 for P = D 4, 6, 12 gives R 1, 3, 10.
    tasks = [{"pin": 1, "period": 4, "deadline": 4, "wcet": 1},
             {"pin": 2, "period": 6, "deadline": 6, "wcet": 2},
             {"pin": 3, "period": 12, "deadline": 12, "wcet": 3}]
    got = [response_time(i, tasks, (0, 0, 0)) for i in range(3)]
    assert got == [1, 3, 10], got
    # With C_isw of 1, the first task also pays one timer entry per task released in its
    # window: R = 1 + 3 at first, and 4 is then a fixed point.
    assert response_time(0, tasks, (1, 0, 0)) == 4
    # A set too heavy for the last task.
    heavy = tasks[:2] + [{"pin": 3, "period": 12, "deadline": 12, "wcet": 6}]
    assert response_time(2, heavy, (0, 0, 0)) is None
    # A synthetic trace: origin 1000, a task of period 100 released at 1000 and 1100,
    # starting 5 and 7 us later, ending 30 and 60 us after its release.
    lines = ["# origin 1000", "time_us,event,arg,extra",
             "1005,7,9,0", "1030,7,9,1", "1107,7,9,0", "1160,7,9,1"]
    origin, events = read_trace(lines)
    seen = observed(origin, events, [{"pin": 9, "period": 100}])
    assert seen[9] == ([30, 60], [25, 53]), seen
    # EDF: a set where task 0, of the later deadline, waits for two jobs of task 1.
    edf = [{"period": 10, "deadline": 10, "wcet": 4},
           {"period": 4, "deadline": 3, "wcet": 1}]
    assert edf_response_time(0, edf, (0, 0, 0)) == 6, edf_response_time(0, edf, (0, 0, 0))
    assert edf_response_time(1, edf, (0, 0, 0)) == 1
    # An overloaded set has no bound.
    assert edf_response_time(0, [{"period": 2, "deadline": 2, "wcet": 2},
                                 {"period": 4, "deadline": 4, "wcet": 1}], (0, 0, 0)) is None
    # Spuri's analysis is exact without costs: it must give, task by task, the worst
    # response a simulation of every offset finds, on small sets drawn at random.
    import random
    rng = random.Random(7)
    checked = 0
    while checked < 300:
        n = rng.randint(2, 3)
        tasks = []
        for _ in range(n):
            period = rng.randint(3, 12)
            deadline = rng.randint(2, period)
            tasks.append({"period": period, "deadline": deadline,
                          "wcet": rng.randint(1, max(1, deadline // 2))})
        horizon = math.lcm(*(t["period"] for t in tasks))
        if busy_period(tasks, (0, 0, 0)) is None or horizon > 60:
            continue
        for i in range(n):
            bound = edf_response_time(i, tasks, (0, 0, 0))
            sim = simulated_edf_response(i, tasks, horizon)
            if bound is not None:
                assert bound == sim, (tasks, i, bound, sim)
            else:
                assert sim > tasks[i]["deadline"] or busy_period(tasks, (0, 0, 0)) is None, \
                    (tasks, i, sim)
        checked += 1
    print("self-test passed: %d task sets, EDF's analysis equal to the simulation" % checked)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("trace", nargs="?")
    parser.add_argument("--task", type=parse_task, action="append", default=[])
    parser.add_argument("--costs", default="0,0,0",
                        help="C_isw,C_timer,C_rsw in microseconds")
    parser.add_argument("--algorithm", choices=["dm", "edf"], default="dm")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.trace or not args.task:
        parser.error("a trace and at least one --task")
    costs = tuple(float(v) for v in args.costs.split(","))
    with open(args.trace) as f:
        origin, events = read_trace(f)
    seen = observed(origin, events, args.task)
    for task in args.task:
        if "wcet" not in task:
            spans = seen[task["pin"]][1]
            if not spans:
                sys.exit(f"pin {task['pin']}: no start and end marks; give its WCET")
            task["wcet"], task["bound"] = max(spans), True
    report(args.task, costs, seen, args.algorithm)


if __name__ == "__main__":
    main()
