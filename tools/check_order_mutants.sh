#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Show that tools/check_order.py can fail: build the hard kernel of an example once per
# _OSMemoryBarrier() of the slot buffers, that barrier removed, and expect check_order.py
# to reject every one of those objects, and to accept the kernel as it is.
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
exit "$missed"
