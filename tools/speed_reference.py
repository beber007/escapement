#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""The speed each power-management policy of the power-aware kernel picks, computed apart
from it. tools/differential.py replays each trace of a power-aware build through
check_speeds(), which fails a dispatch whose speed is not this reference's: a kernel that
runs faster than its policy allows keeps every deadline, and only this sees it.

Written from the ZottaOS User Manual (May 2012), chapter 6, pp. 92-101, after Shin and
Choi (OTE) and Aydin, Melhem, Mossé and Mejía-Alvarez (DRA), every amount of work in
ticks at the fastest speed, the remaining work of an instance its WCET less the work it
received:

- OTE: an instance alone in the ready queue may stretch its remaining work to the next
  event, the earlier of the next release and its own deadline; any other runs at the
  fastest speed (pp. 93-94, figure 6.6).
- DRA: a ready queue simulated under EDF*, every instance taking its WCET at the fastest
  speed, drained by the passage of time from its head; an instance dispatched may take,
  beyond its own simulated remaining time, the simulated time left to the instances ahead
  of it, which have ended early (pp. 95-97).
- DR_OTE: the slower of DRA and OTE (p. 98).
- DM_SLACK: the work an instance leaves undone when it ends is slack, which only tasks of
  lower priority may take; always combined with OTE (pp. 99-101).

The slowdown found is rounded up to the slowest operating point at least as fast. The
remaining work is kept in whole ticks, as the manual's update of it (figure 6.7) gives in
integers: at each timer interrupt and at each switch, the work done since the last
update, rounded down.

Run against the kernel on 2026-10-05, the reference disagreed where the manual leaves a
choice open or the kernel departs from it. Each case was read in the kernel and taken over
here as the kernel has it, all of them erring towards the faster speed, none towards a
missed deadline:

- the kernel sets the speed when the task to run changes, not at every timer interrupt
  as the manual's figures 5.10 and 6.7 have it: a release that does not preempt the
  running task leaves its speed, and reclaims nothing;
- an end sets the speed of the task it hands over to before the timer releases what is
  due at that instant;
- EDF* breaks ties of deadline the other way from the paper (EscapementHardPA.h): the
  instance released last first, then the larger TCB address;
- DM_SLACK keeps one slack, the last one left, which runs out with all the time elapsed,
  where the manual keeps one per task; and it takes the slack only for an instance not
  alone in the ready queue, OTE deciding alone by itself, where DR_OTE takes the slower
  of its two.

Only task sets of periodic tasks are referenced: the manual gives event-driven tasks no
dynamic speed under OTE, and leaves their place in DRA's simulation unsaid.
"""

from fractions import Fraction

# Work done per tick at each operating point, in 256ths of the fastest (test/host).
RATES = {0: 24, 1: 102, 2: 256}
FASTEST = max(RATES)
POLICIES = {"pa_edf": "OTE", "pa_dm": "OTE", "pa_dra": "DRA", "pa_drote": "DR_OTE",
            "pa_dmslack": "DM_SLACK"}


class Failure(Exception):
    pass


def level_for(slowdown):
    """The slowest operating point whose rate is at least slowdown, a fraction of the
    fastest speed."""
    for level in sorted(RATES):
        if Fraction(RATES[level], 256) >= slowdown:
            return level
    return FASTEST


def check_speeds(build, tasks, trace, releases):
    """Replays a trace of build and raises Failure at the first dispatch whose speed is
    not the reference's. releases: the instances of tasks, (task, release, deadline, key,
    optional), as tools/differential.py derives them from the algorithms."""
    policy = POLICIES[build]
    todo = sorted(releases, key=lambda x: (x[1], x[0]))
    ready = {}                                 # task -> [release, deadline, remaining]
    released = [-x["period"] for x in tasks]   # the time of each task's last release
    sim = []                                   # DRA: [deadline, release, task, left]
    sim_now = 0                                # the time DRA's simulation is drained to
    slack = [Fraction(0), None]                # DM_SLACK: the slack, the task that left it
    updated = 0                                # the time of the last update of the work
    stretch = [-1, 0, FASTEST]                 # the task running since that update, the
                                               # ticks it ran since, its speed
    running = -1                               # the task of the last segment
    decided = None                             # the speed of the last dispatch
    dispatch = True                            # the next segment follows a dispatch
    interrupted = None                         # the task the last interrupt found running
    at_end = None                              # the speeds the last end decided, its time

    def priority(i):
        """Deadline-monotonic priority, a larger value lower: the deadline, then the
        order of creation, as GetTaskPriority numbers the tasks."""
        return (tasks[i]["deadline"], i)

    def drain(t):
        """DRA's simulation at full speed, from sim_now to t."""
        nonlocal sim_now
        elapsed = Fraction(t - sim_now)
        sim_now = t
        while sim and elapsed > 0:
            used = min(elapsed, sim[0][3])
            sim[0][3] -= used
            elapsed -= used
            if sim[0][3] == 0:
                sim.pop(0)

    def release_until(t):
        while todo and todo[0][1] <= t:
            i, r, d, _, _ = todo.pop(0)
            ready[i] = [r, d, Fraction(tasks[i]["wcet"])]
            released[i] = r
            if policy in ("DRA", "DR_OTE"):
                drain(r)
                sim.append([d, r, i, Fraction(tasks[i]["wcet"])])
                # EDF* as the kernel orders it: at equal deadlines the instance released
                # last first, then the larger TCB address, the task created last with
                # the host's calloc.
                sim.sort(key=lambda x: (x[0], -x[1], -x[2]))

    def ote(t, i):
        """The slowdown OTE gives instance i, or None: not alone, or its work does not
        fit before the next event. The next release is taken from the periods, as the
        kernel knows those past the end of the run; one due at t but not yet made, as
        when an end decides before the timer's release, is the next event."""
        if len(ready) != 1:
            return None
        window = min([released[j] + x["period"] for j, x in enumerate(tasks)] +
                     [ready[i][1]]) - t
        if ready[i][2] > window:
            return None
        return ready[i][2] / window

    def reference(t, i):
        remaining = ready[i][2]
        if remaining <= 0:
            # Past its WCET, an instance has no work left on record: the kernel runs it at
            # the fastest speed, which ends it soonest.
            return FASTEST
        slowdowns = []
        if policy in ("OTE", "DR_OTE", "DM_SLACK"):
            s = ote(t, i)
            if s is not None:
                slowdowns.append(s)
        if policy in ("DRA", "DR_OTE"):
            drain(t)
            ahead = Fraction(0)
            for _, r, j, left in sim:
                if j == i and r == ready[i][0]:
                    slowdowns.append(remaining / (ahead + left))
                    break
                ahead += left
            else:
                raise Failure("t=%d: task %d dispatched, gone from DRA's simulation" %
                              (t, i))
        if policy == "DM_SLACK" and len(ready) > 1 and slack[1] is not None and \
                slack[0] > 0 and priority(i) > priority(slack[1]):
            slowdowns.append(remaining / (slack[0] + remaining))
        return level_for(min(slowdowns)) if slowdowns else FASTEST

    def flush():
        """Brings the remaining work of the task that ran up to date."""
        i, ticks, speed = stretch
        if i in ready:
            ready[i][2] -= ticks * RATES[speed] // 256
        stretch[1] = 0

    for line in trace.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "I":
            t = int(fields[1])
            if not dispatch:
                interrupted = running
            if policy == "DM_SLACK":
                slack[0] = max(Fraction(0), slack[0] - (t - updated))
            updated = t
            flush()
            continue
        if fields[0] == "E":
            t, i = int(fields[1]), int(fields[2])
            flush()
            if policy == "DM_SLACK":
                slack[:] = [max(Fraction(0), ready[i][2]), i]
            del ready[i]
            stretch[:] = [-1, 0, FASTEST]
            updated = t
            decisions = {}
            for j in ready:
                try:
                    decisions[j] = reference(t, j)
                except Failure as e:
                    decisions[j] = e
            at_end = (t, decisions)
            dispatch = True
            continue
        if fields[0] != "S":
            continue
        t, step, i, speed = (int(f) for f in fields[1:])
        release_until(t)
        if interrupted is not None and i != interrupted:
            dispatch = True
        interrupted = None
        if i != stretch[0]:
            flush()
            stretch[:] = [i, 0, speed]
        if i >= 0:
            if dispatch:
                if at_end is not None and at_end[0] == t and i in at_end[1]:
                    expected = at_end[1][i]
                    if isinstance(expected, Failure):
                        raise expected
                else:
                    expected = reference(t, i)
                if speed != expected:
                    state = {"OTE": "", "DRA": "; simulated queue %s" % sim,
                             "DR_OTE": "; simulated queue %s" % sim,
                             "DM_SLACK": "; slack %s" % slack}[policy]
                    raise Failure("t=%d: task %d dispatched at speed %d, the reference "
                                  "%s gives %d; ready %s%s" %
                                  (t, i, speed, policy, expected, ready, state))
                decided = speed
            elif speed != decided:
                raise Failure("t=%d: task %d changes speed from %d to %d without a "
                              "dispatch" % (t, i, decided, speed))
            dispatch = False
        running = i
        stretch[1] += step
