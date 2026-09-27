#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""Worst-case response times of a task set by analysis, against those a trace shows.

The analysis is that of the ZottaOS User Manual (May 2012, eq. 2.3, p. 15), response
time analysis for fixed priorities with the kernel's costs:

    R_i = C_i + sum over j of higher priority of ceil(R_i / P_j) (C_j + C_rsw)
              + sum over every j of ceil(R_i / P_j) (C_isw + C_timer + C_rsw)

C_isw is the time to save a context and enter the timer's interrupt, C_timer the
worst-case time of its handler, C_rsw the time to restore a context. Priorities are
deadline-monotonic, the shorter deadline first. The trace is the CSV of
tools/read_trace.py --csv --absolute, from a build with make TRACE=1 whose tasks leave a
mark on their pin as they start (extra 0) and end (extra 1), as TaskLEDPico's do. Every
task is first released at the kernel's origin, TimeOrigin, then every period: the
response of an instance is its end mark less its release.

    tools/response_times.py TRACE.csv --task PIN:PERIOD:DEADLINE[:WCET] ...
                            [--costs ISW,TIMER,RSW]
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
                release = start[1]
                responses.append(since - release)
                spans.append((t - start[0]) & 0xFFFFFFFF)
                start = None
        out[pin] = (responses, spans)
    return out


def report(tasks, costs, seen):
    print(f"costs: C_isw {costs[0]} us, C_timer {costs[1]} us, C_rsw {costs[2]} us")
    print(f"{'pin':>4} {'P':>7} {'D':>7} {'C':>7}  {'R analysis':>10}  "
          f"{'R seen, max':>11}  instances")
    for i, task in enumerate(tasks):
        r = response_time(i, tasks, costs)
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
    print("self-test passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("trace", nargs="?")
    parser.add_argument("--task", type=parse_task, action="append", default=[])
    parser.add_argument("--costs", default="0,0,0",
                        help="C_isw,C_timer,C_rsw in microseconds")
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
    report(args.task, costs, seen)


if __name__ == "__main__":
    main()
