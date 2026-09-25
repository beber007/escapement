#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Show that tools/check_order.py can fail: build the hard kernel of an example once per
# _OSMemoryBarrier() of the slot buffers, that barrier removed, and expect check_order.py
# to reject every one of those objects, and to accept the kernel as it is; then the same
# for the task-level stores, with mutants of the source listed at the end.
#
#   tools/check_order_mutants.sh Escapement/CORTEX-Mx/RP2350/Examples/pico2
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXAMPLE=$(cd "$1" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# The command the example's Makefile compiles the kernel with, pointed elsewhere.
COMMAND=$(make -s -C "$EXAMPLE" -n -B build/EscapementHard.o | grep -m1 'EscapementHard\.c')
compile() {   # compile SOURCE OBJECT
    (cd "$EXAMPLE" && eval "$(printf '%s' "$COMMAND" |
        sed "s|[^ ]*EscapementHard\.c|$1|; s|-o [^ ]*|-o $2|") -I$ROOT/Escapement")
}

compile "$ROOT/Escapement/EscapementHard.c" "$WORK/kernel.o"
python3 "$ROOT/tools/check_order.py" "$WORK/kernel.o" >/dev/null ||
    { echo "the kernel as it is fails the check"; exit 1; }

BARRIERS=$(grep -c '_OSMemoryBarrier();' "$ROOT/Escapement/EscapementHard.c")
missed=0
k=1
while [ "$k" -le "$BARRIERS" ]; do
    awk -v k="$k" '/_OSMemoryBarrier\(\);/ { if (++n == k) next } { print }' \
        "$ROOT/Escapement/EscapementHard.c" > "$WORK/EscapementHard.c"
    compile "$WORK/EscapementHard.c" "$WORK/mutant.o"
    if python3 "$ROOT/tools/check_order.py" "$WORK/mutant.o" >/dev/null; then
        echo "barrier $k removed: NOT CAUGHT"; missed=$((missed + 1))
    else
        echo "barrier $k removed: caught"
    fi
    k=$((k + 1))
done
[ "$missed" -eq 0 ] && echo "all $BARRIERS removals caught"

# The task-level stores (check_order.py): each mutant turns an order around, or takes a
# store out, in the source, where the compiler cannot restore it; each must be caught.
# KERNEL SCHEDULER NAME: the kernel, its scheduling (EDF or DM), then the mutation
# applied by mutate() below.
mutate() {   # mutate NAME SOURCE MUTANT
    python3 - "$@" <<'PY'
import sys
name, source, mutant = sys.argv[1:]
text = open(source).read()
ZOMBIE = "  _OSActiveTask->TaskState |= STATE_ZOMBIE;\n"
END = "  CompilerBarrier();\n  _OSNoSaveContext = TRUE; // Don't save the context of this task\n"
DROP_DM = ("        _OSActiveTask->TaskState |= STATE_ZOMBIE;\n        CompilerBarrier();\n"
           "        _OSQueueHead->Next[READYQ] = _OSActiveTask->Next[READYQ];\n")
STEPS = ("                 _OSQueueTail->Next[READYQ] = _OSActiveTask->Next[READYQ];\n"
         "                 CompilerBarrier();\n"
         "                 _OSActiveTask->Next[READYQ] = _OSQueueTail;\n")
DROP_EDF = ("           _OSQueueTail->Next[READYQ] = _OSActiveTask->Next[READYQ];\n"
            "           CompilerBarrier();\n"
            "           _OSActiveTask->TaskState |= STATE_ZOMBIE;\n")
UNLINK = "  /* Remove the task from the ready queue */\n  _OSQueueHead->Next[READYQ] ="
mutations = {
    "zombie after the flag": (END, END + "  CompilerBarrier();\n" + ZOMBIE),
    "no promotion mark": ("                 _OSActiveTask->TaskState |= STATE_ACTIVATE;\n", ""),
    "promotion steps swapped": (STEPS, "\n".join(reversed(STEPS.rstrip("\n").split("\n"))) + "\n"),
    "unlink before zombie": (DROP_DM, "\n".join(reversed(DROP_DM.rstrip("\n").split("\n"))) + "\n"),
    "zombie before drop": (DROP_EDF, "\n".join(reversed(DROP_EDF.rstrip("\n").split("\n"))) + "\n"),
    "remaining time not set": ("     SetActiveTaskRemainingTime = TRUE;\n", ""),
    "remaining time cleared early": (UNLINK, UNLINK.replace("  /* Remove",
        "  SetActiveTaskRemainingTime = FALSE;\n  CompilerBarrier();\n  /* Remove")),
}
old, new = mutations[name]
if old not in text:
    sys.exit(f"{name}: the code to mutate is not in {source}")
if name == "zombie after the flag":      # the first zombie store is OSEndTask's
    text = text.replace(ZOMBIE, "", 1)
open(mutant, "w").write(text.replace(old, new, 1))
PY
}
while IFS=: read -r kernel scheduler name; do
    case $kernel in Hard) flags="" ;; Soft) flags="KERNEL=SOFT" ;; HardPA) flags="KERNEL=PA" ;; esac
    [ "$scheduler" = DM ] && flags="$flags SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING"
    # shellcheck disable=SC2086
    COMMAND=$(make -s -C "$EXAMPLE" -n -B "build/Escapement$kernel.o" $flags 2>/dev/null |
        grep -m1 "Escapement$kernel\.c") || continue      # a kernel the port lacks
    mutant() {
        (cd "$EXAMPLE" && eval "$(printf '%s' "$COMMAND" |
            sed "s|[^ ]*Escapement$kernel\.c|$1|; s|-o [^ ]*|-o $2|") -I$ROOT/Escapement")
    }
    mutate "$name" "$ROOT/Escapement/Escapement$kernel.c" "$WORK/Escapement$kernel.c"
    mutant "$WORK/Escapement$kernel.c" "$WORK/mutant.o"
    if python3 "$ROOT/tools/check_order.py" --tasks "$WORK/mutant.o" >/dev/null; then
        echo "$kernel $scheduler, $name: NOT CAUGHT"; missed=$((missed + 1))
    else
        echo "$kernel $scheduler, $name: caught"
    fi
done <<'MUTANTS'
Hard:EDF:zombie after the flag
Soft:EDF:no promotion mark
Soft:EDF:promotion steps swapped
Soft:DM:unlink before zombie
Soft:EDF:zombie before drop
HardPA:EDF:remaining time not set
HardPA:EDF:remaining time cleared early
MUTANTS
exit "$missed"
