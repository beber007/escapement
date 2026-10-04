#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Line and branch coverage of the kernels by the host test, as a check that can fail.

Builds test/host with clang's source-based coverage, runs it, and reads each test binary
on its own. The nine builds compile the kernels under different #if: a line exists in
some builds only, and a view merged over all the binaries left out lines of the
deadline-monotonic slack build, uncovered ones among them (2026-10-04). A line is
covered when a build that compiles it runs it; a branch, when a build takes it.

Every line some build compiles must be covered, or excluded where it stands by a comment
that says why: COVERAGE-LINE for that line, COVERAGE-OFF ... COVERAGE-ON for the lines
from one to the other. An excluded line that is covered fails too, so that no reason
outlives the code it was given for. Branches must not fall below the floor written in
test/host/coverage-floor: raise it when they rise.

  python3 tools/coverage.py            build, run, check
  python3 tools/coverage.py --list     also print every line not covered, excluded or not

It needs clang, llvm-profdata and llvm-cov: Xcode's on macOS, the llvm package on Linux.
"""

import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
HOST = os.path.join(ROOT, "test", "host")
BUILD = "build-cov"
FLOOR = os.path.join(HOST, "coverage-floor")
SOURCES = ["Escapement/EscapementHard.c", "Escapement/EscapementSoft.c",
           "Escapement/EscapementHardPA.c", "Escapement/CORTEX-Mx/RP2350/Escapement_CoreQueue.c"]


def tool(name):
    """An LLVM tool: Xcode's through xcrun, else on the PATH, else its newest versioned
    name, as Debian installs them."""
    if sys.platform == "darwin":
        found = subprocess.run(["xcrun", "-f", name], capture_output=True, text=True)
        if found.returncode == 0:
            return found.stdout.strip()
    if shutil.which(name):
        return name
    versioned = sorted(glob.glob("/usr/bin/%s-[0-9]*" % name),
                       key=lambda p: int(p.rsplit("-", 1)[1]))
    if versioned:
        return versioned[-1]
    sys.exit("coverage: %s not found" % name)


def build_and_run():
    build = os.path.join(HOST, BUILD)
    shutil.rmtree(build, ignore_errors=True)
    profiles = os.path.join(build, "profiles")
    os.makedirs(profiles)
    env = dict(os.environ, LLVM_PROFILE_FILE=os.path.join(profiles, "%p-%m.profraw"))
    # -O0, so that each line keeps its own counter; the sanitizers stay as the Makefile has
    # them. The tests fork: each process writes a profile of its own (%p).
    run = subprocess.run(["make", "-s", "-C", HOST, "run", "BUILD=" + BUILD, "OPT=-O0",
                          "CC=clang -fprofile-instr-generate -fcoverage-mapping"],
                         env=env, capture_output=True, text=True)
    if run.returncode != 0:
        sys.stdout.write(run.stdout[-3000:] + run.stderr[-3000:])
        sys.exit("coverage: the host test failed")
    merged = os.path.join(build, "all.profdata")
    subprocess.run([tool("llvm-profdata"), "merge", "-sparse", "-o", merged] +
                   glob.glob(os.path.join(profiles, "*.profraw")), check=True)
    binaries = sorted(p for p in glob.glob(os.path.join(build, "test_*"))
                      if os.access(p, os.X_OK) and not p.endswith(".dSYM"))
    return merged, binaries


def read_lcov(text, lines, branches):
    """Adds what one binary ran, in LCOV, to the counts kept per source line and branch."""
    source = None
    for record in text.splitlines():
        if record.startswith("SF:"):
            path = os.path.relpath(os.path.realpath(record[3:]), ROOT)
            source = path if path in SOURCES else None
        elif source is None:
            continue
        elif record.startswith("DA:"):
            line, count = record[3:].split(",")[:2]
            key = (source, int(line))
            lines[key] = max(lines.get(key, 0), int(count))
        elif record.startswith("BRDA:"):
            line, block, branch, taken = record[5:].split(",")
            key = (source, int(line), block, branch)
            branches[key] = max(branches.get(key, 0), 0 if taken == "-" else int(taken))


def excluded_lines():
    """The lines each source excludes, with the reason given where the exclusion starts."""
    excluded = {}
    for source in SOURCES:
        with open(os.path.join(ROOT, source), encoding="utf-8") as f:
            text = f.read().splitlines()
        start = None
        for number, content in enumerate(text, 1):
            if "COVERAGE-OFF" in content:
                start = number
            if start is not None or "COVERAGE-LINE" in content:
                excluded[(source, number)] = start or number
            if "COVERAGE-ON" in content:
                if start is None:
                    sys.exit("coverage: %s:%d: COVERAGE-ON without COVERAGE-OFF" % (source, number))
                start = None
        if start is not None:
            sys.exit("coverage: %s:%d: COVERAGE-OFF never closed" % (source, start))
    return excluded


def main():
    listing = "--list" in sys.argv[1:]
    merged, binaries = build_and_run()
    lines, branches = {}, {}
    for binary in binaries:
        export = subprocess.run([tool("llvm-cov"), "export", "-format=lcov",
                                 "-instr-profile=" + merged, binary] +
                                [os.path.join(ROOT, s) for s in SOURCES],
                                capture_output=True, text=True, check=True)
        read_lcov(export.stdout, lines, branches)
    excluded = excluded_lines()

    uncovered = sorted(k for k, n in lines.items() if n == 0 and k not in excluded)
    stale = sorted(k for k, n in lines.items() if n > 0 and k in excluded)
    kept = [k for k in branches if k[:2] not in excluded]
    taken = sum(1 for k in kept if branches[k] > 0)
    counted = [k for k in lines if k not in excluded]
    # Compared as printed, to the hundredth: the floor is written from that figure.
    percent = round(100.0 * taken / len(kept), 2) if kept else 100.0

    print("%d lines in %d binaries, %d excluded; %d of %d branches taken, %.2f %%" %
          (len(counted), len(binaries), sum(1 for k in lines if k in excluded), taken,
           len(kept), percent))
    if listing:
        for key in sorted(k for k, n in lines.items() if n == 0):
            print("  %s:%d%s" % (key[0], key[1], "  (excluded)" if key in excluded else ""))
    failed = False
    for source, line in uncovered:
        print("not covered and not excluded: %s:%d" % (source, line))
        failed = True
    for source, line in stale:
        print("excluded but covered: %s:%d" % (source, line))
        failed = True
    with open(FLOOR, encoding="utf-8") as f:
        floor = float(f.read().split()[0])
    if percent < floor:
        print("branches below the floor of %.2f %%" % floor)
        failed = True
    elif percent >= floor + 0.1:
        print("branches above the floor of %.2f %%: raise it in test/host/coverage-floor" % floor)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
