#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# A third compiler: clang, over our C code compiled as each target builds it, the three
# kernels under both scheduling algorithms, the Cortex-M layer and each port, for the
# Cortex-M0+ of the RP2040 and the Cortex-M33 of the RP2350 and of the STM32U585, as
# tools/analyze.sh takes them. The CI builds the images with two versions of GCC; clang
# reads the same C with a front end of its own, and refused the "b" constraint of GCC's
# inline assembly until 2026-10-04. Compiled only, not linked: clang brings no libgcc.
# Any warning fails the check.
#
#   sh tools/clang_check.sh                  CLANG=... to use another clang
set -eu
cd "$(dirname "$0")/.."
CLANG=${CLANG:-clang}
K=Escapement
M=$K/CORTEX-Mx
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

check() {   # check TARGET CPU PORT_DIR EXAMPLE_DIR [KERNELS]
    target=$1 cpu=$2 port=$3 example=$4 kernels=${5:-"HARD SOFT HARD_PA"}
    for kernel in $kernels; do
        version=; [ "$kernel" = HARD ] || version=-DESCAPEMENT_VERSION_$kernel
        for scheduler in EARLIEST_DEADLINE_FIRST DEADLINE_MONOTONIC_SCHEDULING; do
            echo "== $cpu, $kernel, $scheduler"
            for f in "$K/EscapementHard.c" "$K/EscapementSoft.c" "$K/EscapementHardPA.c" \
                     "$M"/*.c "$port"/*.c; do
                $CLANG --target="$target" -mcpu="$cpu" -mthumb -ffreestanding -fno-builtin \
                    -Wall -O2 -fno-strict-aliasing $version -DSCHEDULER_REAL_TIME_MODE=$scheduler \
                    -I"$example" -I"$port" -I"$M" -I"$K" -c "$f" -o /dev/null 2>>"$LOG"
            done
        done
    done
}
check thumbv6m-none-eabi cortex-m0plus "$M/RP2040" "$M/RP2040/Examples/pico"
check thumbv8m.main-none-eabi cortex-m33 "$M/RP2350" "$M/RP2350/Examples/pico2" "HARD SOFT"
check thumbv8m.main-none-eabi cortex-m33 "$M/STM32U5" "$M/STM32U5/Examples/uno-q" "HARD SOFT"
if grep -q -E "warning:|error:" "$LOG"; then
    cat "$LOG"
    exit 1
fi
echo "no warning"
