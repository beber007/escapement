#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Mutation testing of a kernel: small faults made in its source, one at a time, and the
host test and tools/differential.py run on each.

Coverage says which lines run; a mutant says whether a test notices when one of them is
wrong. Each mutant changes one place of the kernel's code, outside comments and the
preprocessor's lines:

  ror   a comparison: < and <=, > and >=, == and !=, each for the other
  aor   + and -, += and -=
  lcr   && and ||
  cst   a literal 0 and 1, each for the other
  sdl   a statement of one line left out: an assignment, or a call whose result is
        not used

It is compiled into a directory of its own and run with the builds of its kernel
(test/host, make run BUILDS=... IPC=...), then with tools/differential.py on the same
builds. A mutant is killed when a test fails or stops, survives when every test passes,
and is invalid when it does not compile; the score is the share killed of the valid.
A survivor is a behaviour no test checks, or a mutant that changes nothing (an equivalent
one): each is to be read.

  python3 tools/mutants.py hard                 every mutant of EscapementHard.c
  python3 tools/mutants.py soft --jobs 2        two at a time, each nice'd
  python3 tools/mutants.py hard --list          the mutants only, none run
  python3 tools/mutants.py hard --only 120-180  the mutants of those lines
  python3 tools/mutants.py hard --survivors R.jsonl   those that survived a run, again

Results go to test/host/build-mutants/<kernel>.jsonl as they come, one line a mutant,
and a run started again skips those already there.
"""

import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
HOST = os.path.join(ROOT, "test", "host")
WORK = os.path.join(HOST, "build-mutants")
KERNELS = {
    "hard": ("EscapementHard.c", "HARD", ["hard_edf", "hard_dm"]),
    "soft": ("EscapementSoft.c", "SOFT", ["soft_edf", "soft_dm"]),
    "pa": ("EscapementHardPA.c", "PA", ["pa_edf", "pa_dm", "pa_dra", "pa_drote",
                                        "pa_dmslack"]),
}
LIMIT = 600            # seconds a mutant may take, its build included

OPERATORS = [
    ("ror", r"(?<![<>=!-])<=(?!=)", "<"),
    ("ror", r"(?<![<>=!-])<(?![<=])", "<="),
    ("ror", r"(?<![<>=!-])>=(?!=)", ">"),
    ("ror", r"(?<![<>=!-])>(?![>=])", ">="),
    ("ror", r"==", "!="),
    ("ror", r"!=", "=="),
    ("aor", r"(?<=\s)\+(?=\s)", "-"),
    ("aor", r"(?<=\s)-(?=\s)", "+"),
    ("aor", r"\+=", "-="),
    ("aor", r"-=", "+="),
    ("lcr", r"&&", "||"),
    ("lcr", r"\|\|", "&&"),
    ("cst", r"(?<![\w.])0(?![\w.])", "1"),
    ("cst", r"(?<![\w.])1(?![\w.])", "0"),
]
# A statement of one line that may be left out: an assignment or a bare call inside a
# function, indented, not a declaration, a return, a jump or a part of a larger statement.
STATEMENT = re.compile(r"^\s+(?!return\b|break\b|continue\b|goto\b|else\b|if\b|while\b|"
                       r"for\b|do\b|case\b|default\b|typedef\b|static\b|const\b|"
                       r"volatile\b|struct\b|union\b|enum\b|unsigned\b|signed\b|"
                       r"(?:[A-Z][A-Z0-9_]*|INT\d+|UINT\d+|UINTPTR|BOOL|TCB|ETCB|"
                       r"void|char|int|long|short)\s+\**\w|\w+\s*\(\s*\*)"
                       r"[\w\[\]().>*&-]+\s*(?:[-+*/%&|^]?=|\()[^;{}]*;\s*$")


def code_spans(lines):
    """For each line, the part that is code: comments and string literals blanked out,
    preprocessor lines empty."""
    out, in_comment = [], False
    for line in lines:
        code, i = [], 0
        if not in_comment and line.lstrip().startswith("#"):
            out.append("")
            continue
        while i < len(line):
            if in_comment:
                end = line.find("*/", i)
                if end < 0:
                    code.append(" " * (len(line) - i))
                    i = len(line)
                else:
                    code.append(" " * (end + 2 - i))
                    i, in_comment = end + 2, False
            elif line.startswith("/*", i):
                in_comment = True
            elif line.startswith("//", i):
                code.append(" " * (len(line) - i))
                i = len(line)
            elif line[i] in "\"'":
                q, j = line[i], i + 1
                while j < len(line) and line[j] != q:
                    j += 2 if line[j] == "\\" else 1
                code.append(" " * (j + 1 - i))
                i = j + 1
            else:
                code.append(line[i])
                i += 1
        out.append("".join(code))
    return out


def compiled_lines(kernel):
    """The lines of the kernel's source that some build of test/host compiles, read from
    the preprocessor's line markers with the flags of each build (test/host/Makefile).
    A mutant elsewhere, in code for a target or a variant the host does not build, can
    change nothing the host test runs."""
    source, _, builds = KERNELS[kernel]
    with open(os.path.join(HOST, "Makefile"), encoding="utf-8") as f:
        flags = dict(re.findall(r"^FLAGS_(\w+)\s*=\s*(.*)$", f.read(), re.M))
    path = os.path.join(ROOT, "Escapement", source)
    lines = set()
    for build in builds:
        out = subprocess.run(["cc", "-E", "-I" + HOST, "-I" + os.path.join(ROOT, "Escapement"),
                              "-DCompilerBarrier()=HostCompilerBarrier()"] +
                             flags[build].split() + [path],
                             capture_output=True, text=True, check=True).stdout
        current, number = None, 0
        for line in out.split("\n"):
            marker = re.match(r'^# (\d+) "([^"]*)"', line)
            if marker:
                number, current = int(marker.group(1)), marker.group(2)
                continue
            if current and os.path.basename(current) == source and line.strip():
                lines.add(number)
            number += 1
    # The body of a macro is expanded where it is used, and the markers give that line:
    # its own lines are taken as compiled.
    with open(path, encoding="utf-8") as f:
        text = f.read().split("\n")
    continued = False
    for number, line in enumerate(text, 1):
        if continued:
            lines.add(number)
        continued = (continued or line.lstrip().startswith("#define")) and line.endswith("\\")
    return lines


def mutants(path):
    """Every mutant of the source: (id, line, operator, original, mutated line)."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    found = []
    for number, code in enumerate(code_spans(lines), 1):
        if not code.strip():
            continue
        line = lines[number - 1]
        for name, pattern, replacement in OPERATORS:
            for m in re.finditer(pattern, code):
                mutated = line[:m.start()] + replacement + line[m.end():]
                found.append((name, number, line, mutated))
        if STATEMENT.match(code):
            indent = line[:len(line) - len(line.lstrip())]
            found.append(("sdl", number, line, indent + ";  /* mutant: statement left out */"))
    return [(k, *m) for k, m in enumerate(found)]


def run_mutant(kernel, mutant):
    """Builds and tests one mutant; returns its record."""
    ident, op, number, original, mutated = mutant
    source, variable, builds = KERNELS[kernel]
    work = os.path.join(WORK, "%s-%d" % (kernel, ident))
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    with open(os.path.join(ROOT, "Escapement", source), encoding="utf-8") as f:
        lines = f.read().split("\n")
    lines[number - 1] = mutated
    with open(os.path.join(work, source), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    record = {"id": ident, "op": op, "line": number, "original": original.strip(),
              "mutated": mutated.strip()}
    build = os.path.relpath(os.path.join(work, "build"), HOST)
    make = ["nice", "-n", "10", "make", "-s", "-C", HOST, "BUILD=" + build,
            "%s=%s" % (variable, os.path.join(work, source))]
    targets = ["%s/test_scheduler_%s" % (build, b) for b in builds] + \
              ["%s/test_ipc_%s" % (build, kernel)]
    try:
        if subprocess.run(make + targets, capture_output=True, timeout=LIMIT).returncode:
            record["verdict"] = "invalid"
            return record
        test = subprocess.run(make + ["run", "BUILDS=" + " ".join(builds), "IPC=" + kernel],
                              capture_output=True, text=True, timeout=LIMIT)
        if test.returncode:
            failed = [l for l in test.stdout.splitlines() if "FAILED" in l]
            record["verdict"], record["by"] = "killed", (failed or ["host test"])[0].strip()
            return record
        env = dict(os.environ, BUILD=build)
        for b in builds:
            diff = subprocess.run(["nice", "-n", "10", sys.executable,
                                   os.path.join(ROOT, "tools", "differential.py"), b, "100"],
                                  capture_output=True, text=True, timeout=LIMIT, env=env)
            if diff.returncode:
                record["verdict"] = "killed"
                record["by"] = "differential: " + diff.stdout.strip().splitlines()[-1][:160]
                return record
        record["verdict"] = "survived"
    except subprocess.TimeoutExpired:
        record["verdict"], record["by"] = "killed", "timeout"
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("kernel", choices=sorted(KERNELS))
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--only", help="FIRST-LAST: the lines whose mutants to run")
    parser.add_argument("--survivors", help="RESULTS.jsonl: run again those that survived "
                        "there, into RESULTS-again.jsonl")
    args = parser.parse_args()
    source = os.path.join(ROOT, "Escapement", KERNELS[args.kernel][0])
    todo = mutants(source)
    compiled = compiled_lines(args.kernel)
    skipped = [m for m in todo if m[2] not in compiled]
    todo = [m for m in todo if m[2] in compiled]
    if args.only:
        first, last = (int(v) for v in args.only.split("-"))
        todo = [m for m in todo if first <= m[2] <= last]
    if args.list:
        for ident, op, number, original, mutated in todo:
            print("%4d %s %5d  %s" % (ident, op, number, mutated.strip()))
        print("%d mutants, and %d on lines no build of the host compiles" %
              (len(todo), len(skipped)))
        return
    os.makedirs(WORK, exist_ok=True)
    results = os.path.join(WORK, args.kernel + ".jsonl")
    if args.survivors:
        with open(args.survivors) as f:
            again = {json.loads(l)["id"] for l in f
                     if l.strip() and json.loads(l)["verdict"] == "survived"}
        todo = [m for m in todo if m[0] in again]
        results = args.survivors.replace(".jsonl", "-again.jsonl")
    done = set()
    if os.path.exists(results):
        with open(results) as f:
            done = {json.loads(l)["id"] for l in f if l.strip()}
    todo = [m for m in todo if m[0] not in done]
    print("%d mutants to run, %d already done, %d on lines the host does not compile" %
          (len(todo), len(done), len(skipped)), flush=True)
    with open(results, "a") as out, \
         concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for record in pool.map(lambda m: run_mutant(args.kernel, m), todo):
            out.write(json.dumps(record) + "\n")
            out.flush()
            print("%4d %-8s %s %d: %s" % (record["id"], record["verdict"], record["op"],
                                          record["line"], record["mutated"][:70]), flush=True)
    with open(results) as f:
        records = [json.loads(l) for l in f if l.strip()]
    records = [r for r in records if r["line"] in compiled]
    valid = [r for r in records if r["verdict"] != "invalid"]
    killed = [r for r in valid if r["verdict"] == "killed"]
    print("%d mutants: %d killed, %d survived, %d invalid; score %.1f %%" %
          (len(records), len(killed), len(valid) - len(killed), len(records) - len(valid),
           100.0 * len(killed) / len(valid) if valid else 0))


if __name__ == "__main__":
    main()
