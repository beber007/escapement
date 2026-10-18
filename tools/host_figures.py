#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""Draw how the kernels schedule two task sets, from the traces of the host build, as SVG.

Each figure is a run of test/host's mode "trace" (test_scheduler.c, TestTrace): the
kernel as it ships schedules the tasks, and the trace says who ran from when and for how
long. Nothing is drawn by hand, and the script checks what the README says of each run
before drawing it, so that a kernel that schedules otherwise fails here:

- edf-dm: tasks (50, 25) and (80, 35), 94 % of the processor. Under EDF every deadline is
  kept; under deadline-monotonic scheduling the second task is unfinished at 80, its
  deadline. The overrun hook lets the run go on (line "H"), the release at 80 skipped.
- firm: tasks (50, 30) with 2 of every 3 instances mandatory and (80, 40) with 1 of every
  2, 110 % of the processor if every instance ran. The soft kernel under EDF keeps every
  deadline by dropping optional instances, two in the 400 ticks shown.

Each figure is written twice, for a light and a dark page, as the README's banner is.
test/host must be built (make -C test/host). No dependency:

  python3 tools/host_figures.py           write docs/images/{edf-dm,firm}-{light,dark}.svg
  python3 tools/host_figures.py --check   fail if a file differs from what the run draws
  python3 tools/host_figures.py --self-test   traces made wrong must be refused

The checks raise differential.Failure, not assert, which python3 -O would take out.
"""
import importlib.util
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = os.path.join(ROOT, "docs", "images")
# The span shown: the hyperperiod of the first set; the patterns of the (m,k)-firm set
# repeat only after 2400, of which the figure shows the first 400
SPAN = 400

_spec = importlib.util.spec_from_file_location(
    "differential", os.path.join(ROOT, "tools", "differential.py"))
differential = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(differential)
Failure = differential.Failure
differential.DURATION = SPAN                 # this script's copy of it runs and checks the span

THEMES = {
    "light": {"bg": "#ffffff", "ink": "#1f2328", "muted": "#59636e", "grid": "#e7ecf0",
              "tasks": ["#1f6feb", "#8250df"], "miss": "#cf222e", "kept": "#1a7f37"},
    "dark": {"bg": "#0d1117", "ink": "#e6edf3", "muted": "#9198a1", "grid": "#21262d",
             "tasks": ["#4493f8", "#ab7df8"], "miss": "#f85149", "kept": "#3fb950"},
}
W, LEFT, RIGHT = 600, 104, 16
LANE_H, LANE_GAP = 26, 12
FONT = "ui-sans-serif,-apple-system,Segoe UI,Helvetica,Arial,sans-serif"


def x(t):
    return LEFT + (W - RIGHT - LEFT) * t / SPAN


def work_by(slices, t):
    """The work slices did by time t."""
    return sum(max(0, min(b, t) - a) for a, b in slices)


def load(tasks):
    """The share of the processor the tasks ask for, in whole per cent, as the README
    gives it."""
    return round(100 * sum(t["wcet"] / t["period"] for t in tasks))


def says(text):
    """A Failure unless the README says text, its lines joined."""
    with open(os.path.join(ROOT, "README.md")) as fh:
        readme = " ".join(fh.read().split())
    require(text in readme, "the README does not say", repr(text))


def require(ok, *why):
    if not ok:
        raise Failure(" ".join(str(w) for w in why))


# The phrases of this script's own Failures, which the algorithm's never contain
OWN = ["30 of 35", "skipped other releases", "two dropped", "not understood",
       "belongs to no release", "skipped under EDF", "skipped in the (m,k)-firm set",
       "does not say"]
_runs = {}


def run(build, tasks, overruns=False):
    """The trace of tasks over the span, as tools/differential.py runs them, each run once
    when --self-test and --check share a process."""
    key = (build, overruns, repr(tasks))
    if key not in _runs:
        _runs[key] = differential.run(build, tasks, overruns=overruns)
    return _runs[key]


# The lines of a trace tools/differential.py reads, and the number of integers each
# carries (test_scheduler.c, TestTrace); it skips any other line, as this script does
FIELDS = {"S": 4, "E": 2, "I": 1, "O": 2, "T": 1}


def parse(trace, tasks):
    """Per task, the slices it ran, the ends of its instances and the releases skipped
    (lines "S", "E" and "O"). Each line differential.check reads must carry its integers
    and a task of the set, or it is a Failure here rather than a crash there."""
    slices = {i: [] for i in range(len(tasks))}
    ends = {i: [] for i in range(len(tasks))}
    skipped = {i: set() for i in range(len(tasks))}
    for line in trace.splitlines():
        f = line.split()
        if not f or f[0] not in FIELDS:
            continue
        try:
            v = [int(n) for n in f[1:]]
            if len(v) != FIELDS[f[0]]:
                raise ValueError
            if f[0] == "S" and v[2] >= 0:
                slices[v[2]].append((v[0], v[0] + v[1]))
            elif f[0] == "S" and v[2] != -1:
                raise ValueError
            elif f[0] == "E":
                ends[v[1]].append(v[0])
            elif f[0] == "O":
                skipped[v[1]].add(v[0])
        except (ValueError, KeyError):
            raise Failure("a line of the trace not understood: %r" % line)
    return slices, ends, skipped


def instances(trace, tasks, firm=False):
    """Per task, each release in the span: its time, the slices it ran, its end, whether
    its release was skipped (line "O"), and, (m,k)-firm, whether it was dropped. An
    instance is the slices up to an "E", and belongs to the oldest release not yet
    served, its own or an earlier one if it is late; an (m,k)-firm instance runs by its
    deadline or not at all, so a release whose deadline passed before the instance began
    was dropped, as is one not begun by a deadline within the span."""
    slices, ends, skipped = parse(trace, tasks)
    out = {}
    for i, task in enumerate(tasks):
        jobs = [{"release": r, "slices": [], "end": None, "skipped": r in skipped[i]}
                for r in range(0, SPAN, task["period"])]
        waiting = [job for job in jobs if not job["skipped"]]

        def serve(chunk, end):
            start = chunk[0][0]
            while firm and waiting and waiting[0]["release"] + task["deadline"] <= start:
                waiting.pop(0)
            require(waiting and waiting[0]["release"] <= start,
                    "a slice of task", i, "at", start, "belongs to no release")
            job = waiting.pop(0)
            job["slices"], job["end"] = chunk, end

        chunk = []
        pending = sorted(ends[i])
        for a, b in sorted(slices[i]):
            chunk.append((a, b))
            if pending and b == pending[0]:
                serve(chunk, pending.pop(0))
                chunk = []
        if chunk:                            # still running when the span ends
            serve(chunk, None)
        for j, job in enumerate(jobs):
            job["dropped"] = (firm and not job["slices"] and not job["skipped"] and
                              job["release"] + task["deadline"] <= SPAN and
                              not differential.mandatory(j, task["m"], task["k"]))
        out[i] = jobs
    return out


def lane(out, th, top, task, jobs, colour, firm=None):
    """One task's row: its slices, a tick at each release, each deadline missed in red,
    under (m,k)-firm scheduling the optional instances pale and those dropped crossed."""
    base, high = top + LANE_H, top + 4
    out.append(f'<text x="{LEFT - 10}" y="{top + 12}" font-size="14" font-weight="600" '
               f'fill="{th["ink"]}" text-anchor="end">{task["name"]}</text>')
    out.append(f'<text x="{LEFT - 10}" y="{top + 27}" font-size="12" fill="{th["muted"]}" '
               f'text-anchor="end">{task["caption"]}</text>')
    out.append(f'<line x1="{LEFT}" y1="{base}" x2="{W - RIGHT}" y2="{base}" '
               f'stroke="{th["muted"]}" stroke-width="1"/>')
    for j, job in enumerate(jobs):
        r = job["release"]
        mark = (f'fill="none" stroke="{th["muted"]}" stroke-width="1.2"' if job["skipped"]
                else f'fill="{th["muted"]}"')
        out.append(f'<path d="M{x(r):.1f} {base + 1} l-4 7 h8 z" {mark}/>')
        optional = firm is not None and not differential.mandatory(j, *firm)
        deadline = r + task["deadline"]
        for a, b in job["slices"]:
            for lo, hi, fill in ((a, min(b, deadline), colour), (max(a, deadline), b, th["miss"])):
                if hi > lo:
                    opacity = ' fill-opacity="0.4"' if optional else ""
                    out.append(f'<rect x="{x(lo):.2f}" y="{high}" width="{x(hi) - x(lo):.2f}" '
                               f'height="{base - high}" fill="{fill}"{opacity}/>')
        if job.get("dropped"):
            cx, cy = x(r) + 9, (high + base) / 2
            out.append(f'<path d="M{cx - 5:.1f} {cy - 5:.1f} l10 10 m0 -10 l-10 10" '
                       f'stroke="{colour}" stroke-width="2" stroke-linecap="round"/>')
        if job["end"] is not None:
            late = job["end"] > deadline
        else:                                # unfinished, or never started, at its deadline
            late = deadline <= SPAN and not job["skipped"] and not job.get("dropped")
        if late:
            out.append(f'<line x1="{x(deadline):.1f}" y1="{top - 4}" x2="{x(deadline):.1f}" '
                       f'y2="{base + 2}" stroke="{th["miss"]}" stroke-width="2.5"/>')
    return base


def panel(out, th, top, title, verdict, ok, tasks, jobs, firm=None):
    out.append(f'<text x="{LEFT}" y="{top}" font-size="15" font-weight="600" '
               f'fill="{th["ink"]}">{title}</text>')
    out.append(f'<text x="{W - RIGHT}" y="{top}" font-size="13" font-weight="600" '
               f'fill="{th["kept"] if ok else th["miss"]}" text-anchor="end">{verdict}</text>')
    first = top + 18
    bottom = first + len(tasks) * (LANE_H + LANE_GAP) - LANE_GAP
    for t in range(0, SPAN + 1, 50):
        out.append(f'<line x1="{x(t):.1f}" y1="{first - 4}" x2="{x(t):.1f}" '
                   f'y2="{bottom + 8}" stroke="{th["grid"]}" stroke-width="1"/>')
        out.append(f'<text x="{x(t):.1f}" y="{bottom + 24}" font-size="12" '
                   f'fill="{th["muted"]}" text-anchor="middle">{t}</text>')
    out.append(f'<text x="{LEFT - 10}" y="{bottom + 24}" font-size="12" '
               f'fill="{th["muted"]}" text-anchor="end">ticks</text>')
    for i, task in enumerate(tasks):
        pattern = (task["m"], task["k"]) if firm else None
        lane(out, th, first + i * (LANE_H + LANE_GAP), task, jobs[i], th["tasks"][i], pattern)
    return bottom + 24


def svg(th, title, lines, body, height):
    head = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{height}" '
            f'viewBox="0 0 {W} {height}" font-family="{FONT}">',
            f'<rect width="{W}" height="{height}" rx="8" fill="{th["bg"]}"/>',
            f'<text x="16" y="28" font-size="17" font-weight="700" fill="{th["ink"]}">'
            f'{title}</text>']
    for n, text in enumerate(lines):
        head.append(f'<text x="16" y="{50 + 18 * n}" font-size="13" fill="{th["muted"]}">'
                    f'{text}</text>')
    return "\n".join(head + body + ["</svg>"]) + "\n"


EDF_DM = [{"kind": "P", "wcet": 25, "period": 50, "deadline": 50, "takes": 25,
           "name": "Task 1", "caption": "every 50, 25"},
          {"kind": "P", "wcet": 35, "period": 80, "deadline": 80, "takes": 35,
           "name": "Task 2", "caption": "every 80, 35"}]


def check_edf_dm(edf_trace, dm_trace, tasks=EDF_DM):
    """The traces checked against EDF and DM (tools/differential.py), which holds EDF to
    every deadline but reads no "O" line without overruns: none may skip a release under
    EDF. Then what the README says of the DM run that the algorithm does not:
    task 2 unfinished at 80 with 30 of its 35 done, and that release alone skipped. Each
    deadline being the next release, any other late instance would skip one too."""
    require(not any(parse(edf_trace, tasks)[2].values()), "a release skipped under EDF")
    skipped = parse(dm_trace, tasks)[2]
    differential.check("hard_edf", tasks, edf_trace, speeds=False)
    differential.check("hard_dm", tasks, dm_trace, speeds=False, overruns=True)
    edf = instances(edf_trace, tasks)
    dm = instances(dm_trace, tasks)
    first = dm[1][0]
    deadline = first["release"] + tasks[1]["deadline"]
    done = work_by(first["slices"], deadline)
    require(first["end"] is not None and first["end"] > deadline == 80 and done == 30,
            "DM: task 2's first instance is not 30 of 35 done at 80", first, done)
    skips = sorted((i, t) for i in skipped for t in skipped[i])
    require(skips == [(1, 80)], "DM skipped other releases", skips)
    says("take %d %% of the processor" % load(tasks))
    return edf, dm


def edf_dm():
    tasks = EDF_DM
    edf, dm = check_edf_dm(run("hard_edf", tasks), run("hard_dm", tasks, overruns=True))

    def draw(th):
        body = []
        y = panel(body, th, 104, "EDF", "every deadline kept", True, tasks, edf)
        y = panel(body, th, y + 34, "Deadline monotonic", "task 2 late at 80, 30 of 35 done",
                  False, tasks, dm)
        for n, text in enumerate(["By default the kernel stops at an overrun; here the hook",
                                  "lets it go on, task 2's release at 80 skipped."]):
            body.append(f'<text x="{LEFT}" y="{y + 22 + 16 * n}" font-size="12" '
                        f'fill="{th["muted"]}">{text}</text>')
        y += 16
        return svg(th, "%d %% of the processor: EDF keeps up, fixed priorities do not"
                   % load(tasks),
                   ["Two periodic tasks, each due by its next release, each within its",
                    "execution time. The hard kernel on the host, traced as it ran."],
                   body, y + 40)
    return draw


FIRM = [{"kind": "F", "wcet": 30, "period": 50, "deadline": 50, "takes": 30, "m": 2,
         "k": 3, "name": "Task 1", "caption": "every 50, 30"},
        {"kind": "F", "wcet": 40, "period": 80, "deadline": 80, "takes": 40, "m": 1,
         "k": 2, "name": "Task 2", "caption": "every 80, 40"}]


def check_firm(trace, tasks=FIRM):
    """The trace checked against the soft kernel's EDF (tools/differential.py), which holds
    every instance run to its deadline and every mandatory one run, but reads no "O" line
    without overruns: none may skip a release here. Then the count the README gives: two
    optional instances dropped in the span."""
    skipped = parse(trace, tasks)[2]
    require(not any(skipped.values()), "a release skipped in the (m,k)-firm set", skipped)
    differential.check("soft_edf", tasks, trace, speeds=False)
    jobs = instances(trace, tasks, firm=True)
    dropped = sum(job["dropped"] for i in jobs for job in jobs[i])
    ran = sum(1 for i, t in enumerate(tasks) for j, job in enumerate(jobs[i])
              if job["slices"] and not differential.mandatory(j, t["m"], t["k"]))
    require(dropped == 2, "the README says two dropped, not", dropped)
    says("ask for %d %%" % load(tasks))
    return jobs, dropped, ran


def firm():
    tasks = FIRM
    jobs, dropped, ran = check_firm(run("soft_edf", tasks))

    def draw(th):
        body = []
        y = panel(body, th, 122, "(m,k)-firm under EDF",
                  f"every deadline kept, {dropped} dropped", True, tasks, jobs, firm=True)
        legend = y + 22
        body.append(f'<rect x="{LEFT}" y="{legend - 10}" width="14" height="12" '
                    f'fill="{th["muted"]}"/>')
        body.append(f'<text x="{LEFT + 20}" y="{legend}" font-size="12" fill="{th["muted"]}">'
                    'mandatory</text>')
        body.append(f'<rect x="{LEFT + 104}" y="{legend - 10}" width="14" height="12" '
                    f'fill="{th["muted"]}" fill-opacity="0.4"/>')
        body.append(f'<text x="{LEFT + 124}" y="{legend}" font-size="12" '
                    f'fill="{th["muted"]}">optional, run ({ran})</text>')
        cx, cy = LEFT + 250, legend - 4
        body.append(f'<path d="M{cx - 5} {cy - 5} l10 10 m0 -10 l-10 10" '
                    f'stroke="{th["muted"]}" stroke-width="2" stroke-linecap="round"/>')
        body.append(f'<text x="{cx + 12}" y="{legend}" font-size="12" fill="{th["muted"]}">'
                    'optional, dropped</text>')
        return svg(th, "%d %% of the processor asked for: the kernel drops what may go"
                   % load(tasks),
                   ["Task 1: 2 of every 3 instances mandatory; task 2: 1 of every 2.",
                    "An optional instance starts only if every mandatory deadline still",
                    "holds. The soft kernel on the host, traced as it ran."],
                   body, legend + 18)
    return draw


def self_test():
    """Traces of the host made wrong, each of which the checks must refuse with a Failure,
    and others they must accept; then the drawing of a miss, and an unknown option."""
    edf, dm = run("hard_edf", EDF_DM), run("hard_dm", EDF_DM, overruns=True)
    firm_trace = run("soft_edf", FIRM)

    def swap(trace, old, new):
        if old not in trace:
            raise RuntimeError("the self-test's trace has no %r" % old)
        return trace.replace(old, new)

    def without(trace, *gone):
        lines = trace.splitlines()
        for g in gone:
            lines.remove(g)
        return "\n".join(lines) + "\n"

    def until(trace, last, task):
        """The trace with every line of task after the line last taken out."""
        lines, out, cut = trace.splitlines(), [], False
        for line in lines:
            f = line.split()
            mine = (f[:1] == ["S"] and f[3] == str(task)) or (f[:1] in (["E"], ["O"]) and
                                                             f[2] == str(task))
            if not (cut and mine):
                out.append(line)
            cut = cut or line == last
        if not cut:
            raise RuntimeError("the self-test's trace has no %r" % last)
        return "\n".join(out) + "\n"

    def with_load(value, test):
        global load
        kept, load = load, lambda tasks: value
        try:
            test()
        finally:
            load = kept

    def longer(tasks, i, wcet):
        changed = [dict(t) for t in tasks]
        changed[i]["wcet"] = changed[i]["takes"] = wcet
        return changed

    # name, the phrase of the Failure that must refuse it, test
    refused = [
        ("the processor idle while task 1 waits under EDF", "idle while",
         lambda: check_edf_dm(without(swap(swap(edf, "S 60 20 0 0", "S 60 20 -1 0"),
                                           "S 80 5 0 0", "S 80 5 -1 0"), "E 85 0"), dm)),
        ("task 2 run before task 1 at 0 under EDF", "while [0] waits",
         lambda: check_edf_dm(swap(edf, "S 0 25 0 0\nE 25 0\nS 25 25 1 0\n",
                                   "S 0 25 1 0\nS 25 25 0 0\nE 50 0\n"), dm)),
        ("a second release skipped under DM", "overruns told",
         lambda: check_edf_dm(edf, swap(dm, "I 160\n", "I 160\nO 160 1\n"))),
        ("task 2's first instance never ending under DM", "did not end",
         lambda: check_edf_dm(edf, until(without(dm, "E 85 1"), "S 80 5 1 0", 1))),
        ("task 2 left waiting until after its next release under DM", "idle while",
         lambda: check_edf_dm(edf, swap(swap(swap(dm, "S 25 25 1 0", "S 25 25 -1 0"),
                                             "S 75 5 1 0", "S 75 5 -1 0"),
                                        "S 85 15 -1 0", "S 85 15 1 0"))),
        ("task 2 run before task 1 under DM", "while [0] waits",
         lambda: check_edf_dm(edf, swap(dm, "S 50 25 0 0\nE 75 0\nS 75 5 1 0\n",
                                        "S 50 5 1 0\nS 55 25 0 0\nE 80 0\n"))),
        ("a release skipped where the task has none under DM", "overruns told",
         lambda: check_edf_dm(edf, swap(dm, "I 100\n", "I 100\nO 100 1\n"))),
        ("task 2 run before task 1 at 0 in the (m,k)-firm set", "while [0] waits",
         lambda: check_firm(swap(firm_trace, "S 0 30 0 0\nE 30 0\nS 30 20 1 0\n",
                                 "S 0 20 1 0\nS 20 30 0 0\nE 50 0\n"))),
        ("a release skipped in the (m,k)-firm set", "skipped in the (m,k)-firm set",
         lambda: check_firm(swap(firm_trace, "I 160\n", "I 160\nO 160 1\n"))),
        ("a mandatory instance left unrun, the processor idle", "idle while",
         lambda: check_firm(without(swap(firm_trace, "S 0 30 0 0", "S 0 30 -1 0"), "E 30 0"))),
        ("a line of the trace garbled", "not understood",
         lambda: check_firm(swap(firm_trace, "S 0 30 0 0", "S 0 3\x7f 0 0"))),
        ("a slice line one field short", "not understood",
         lambda: check_firm(swap(firm_trace, "S 0 30 0 0", "S 0 30 0"))),
        ("a slice line one field long", "not understood",
         lambda: check_firm(swap(firm_trace, "S 0 30 0 0", "S 0 30 0 0 7"))),
        ("a timer line garbled", "not understood",
         lambda: check_firm(swap(firm_trace, "I 50\n", "I 5x0\n"))),
        ("a release skipped under EDF", "skipped under EDF",
         lambda: check_edf_dm(swap(edf, "I 160\n", "I 160\nO 160 1\n"), dm)),
        ("a slice of a task before any of its releases not skipped", "belongs to no release",
         lambda: instances("O 0 0\nS 0 25 0 0\nE 25 0\n", EDF_DM)),
        # The traces as printed, a load the README does not give
        ("the load of the EDF and DM set not the README's", "does not say",
         lambda: with_load(98, lambda: check_edf_dm(edf, dm))),
        ("the load of the (m,k)-firm set not the README's", "does not say",
         lambda: with_load(98, lambda: check_firm(firm_trace))),
        # Kernels scheduling right, task sets of which the README's words are not true
        ("task 2 of 30, done by 80 under DM", "30 of 35",
         lambda: check_edf_dm(run("hard_edf", longer(EDF_DM, 1, 30)),
                              run("hard_dm", longer(EDF_DM, 1, 30), overruns=True),
                              longer(EDF_DM, 1, 30))),
        ("task 2 of 38, two releases skipped under DM", "skipped other releases",
         lambda: check_edf_dm(run("hard_edf", longer(EDF_DM, 1, 38)),
                              run("hard_dm", longer(EDF_DM, 1, 38), overruns=True),
                              longer(EDF_DM, 1, 38))),
        ("task 1 of 20 in the (m,k)-firm set, one dropped", "two dropped",
         lambda: check_firm(run("soft_edf", longer(FIRM, 0, 20)), longer(FIRM, 0, 20))),
    ]
    accepted = [
        ("the traces as the kernels print them", lambda: (check_edf_dm(edf, dm),
                                                          check_firm(firm_trace))),
    ]
    failed = []
    for name, phrase, test in refused:
        try:
            test()
            failed.append("not refused: " + name)
        except Failure as e:
            others = [p for p in OWN if p in str(e) and p != phrase]
            if phrase not in str(e) or others:
                failed.append("refused by another check: %s (%s)" % (name, e))
        except Exception as e:
            failed.append("refused by a crash, not a check: %s (%s)" % (name, type(e).__name__))
    for name, test in accepted:
        try:
            test()
        except Exception as e:
            failed.append("refused wrongly: %s (%s: %s)" % (name, type(e).__name__, e))
    light = THEMES["light"]
    def refuses(test):
        try:
            test()
        except Failure:
            return True
        return False

    out, crosses, split, edge = [], [], [], []
    lane(split, light, 0, EDF_DM[1], [
        {"release": 0, "slices": [(75, 85)], "end": 85, "skipped": False}], "#000000")
    lane(edge, light, 0, EDF_DM[1], [
        {"release": 320, "slices": [(325, 400)], "end": None, "skipped": False}], "#000000")
    lane(out, light, 0, EDF_DM[1], [
        {"release": 0, "slices": [(25, 50)], "end": None, "skipped": False},
        {"release": 80, "slices": [], "end": None, "skipped": True}], "#000000")
    lane(crosses, light, 0, FIRM[1], [
        {"release": 0, "slices": [(0, 40)], "end": 40, "skipped": False},
        {"release": 80, "slices": [], "end": None, "skipped": False, "dropped": True},
        {"release": 360, "slices": [], "end": None, "skipped": False, "dropped": False}],
         "#000000", (1, 2))
    try:
        options(["--chek"])
        unknown = False
    except SystemExit as e:
        unknown = bool(e.code)
    mandatory = []
    lane(mandatory, light, 0, FIRM[1], [
        {"release": 0, "slices": [], "end": None, "skipped": False, "dropped": False}],
         "#000000", (1, 2))
    unrun = instances(without(firm_trace, "S 0 30 0 0", "E 30 0"), FIRM, firm=True)[0][0]
    drawn = [
        ("a mandatory instance never run not taken for one dropped", not unrun["dropped"]),
        ("a load the README does not give refused",
         refuses(lambda: says("take 98 % of the processor"))),
        ("work_by counting a slice across the deadline up to it",
         work_by([(25, 50), (75, 85)], 80) == 30),
        ("a mandatory instance never run drawn as a miss, without a cross",
         any(light["miss"] in line for line in mandatory) and
         not any('stroke-linecap="round"' in line for line in mandatory)),
        ("an instance unfinished past its deadline drawn with its miss",
         any(light["miss"] in line for line in out)),
        ("a release skipped drawn hollow",
         any(line.startswith("<path") and 'fill="none"' in line for line in out)),
        ("a cross on the instance dropped, none on one not dropped",
         sum('stroke-linecap="round"' in line for line in crosses) == 1),
        ("the part of a slice past its deadline drawn as a miss",
         any(line.startswith("<rect") and light["miss"] in line for line in split)),
        ("a miss drawn on an instance due at the end of the span, unfinished",
         any(light["miss"] in line for line in edge)),
        ("an unknown option refused", unknown),
    ]
    failed += ["wrong: " + name for name, ok in drawn if not ok]
    total = len(refused) + len(accepted) + len(drawn)
    for line in failed:
        print(line)
    if failed:
        sys.exit("self-test: %d of %d cases wrong" % (len(failed), total))
    print("self-test: all %d cases right" % total)


def options(argv):
    unknown = [a for a in argv if a not in ("--check", "--self-test")]
    if unknown:
        sys.exit("unknown option %s: python3 tools/host_figures.py [--check | --self-test]"
                 % unknown[0])
    return argv


def main(argv):
    argv = options(argv)
    try:
        figures(argv)
    except FileNotFoundError as e:
        if "test_scheduler_" in str(e.filename):
            sys.exit("%s not found: build test/host first (make -C test/host)" % e.filename)
        sys.exit("host_figures: %s" % e)
    except (OSError, differential.subprocess.TimeoutExpired) as e:
        sys.exit("host_figures: %s" % e)
    except Failure as e:
        sys.exit("host_figures: %s" % e)


def figures(argv):
    if "--self-test" in argv:
        self_test()
        if "--check" not in argv:
            return
    draws = [(name, figure()) for name, figure in (("edf-dm", edf_dm), ("firm", firm))]
    check = "--check" in argv
    stale = []
    for name, draw in draws:
        for theme, th in THEMES.items():
            path = os.path.join(IMAGES, "%s-%s.svg" % (name, theme))
            text = draw(th)
            if check:
                try:
                    with open(path) as fh:
                        same = fh.read() == text
                except FileNotFoundError:
                    same = False
                if not same:
                    stale.append(path)
            else:
                with open(path, "w") as fh:
                    fh.write(text)
                print("wrote", os.path.relpath(path, ROOT))
    if stale:
        sys.exit("not what the kernels draw now (python3 tools/host_figures.py): "
                 + ", ".join(os.path.relpath(p, ROOT) for p in stale))

if __name__ == "__main__":
    main(sys.argv[1:])
