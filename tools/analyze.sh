#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The static analyser of GCC (-fanalyzer) over our C code, compiled as each target
# builds it: the three kernels under both scheduling algorithms, the Cortex-M layer and
# each port, for the Cortex-M0+ of the RP2040, the Cortex-M33 of the RP2350 and of the
# STM32U575, and the Cortex-M4 of the STM32F4. It follows paths through each function and between them,
# looking for a null pointer dereferenced, a value read before it is set, a leak or a
# double free: what cppcheck (tools/cppcheck.sh) reasons about without following paths.
# Any warning fails the check.
#
#   sh tools/analyze.sh                  CROSS_COMPILE=... to use another toolchain
set -eu
cd "$(dirname "$0")/.."
CC=${CROSS_COMPILE:-arm-none-eabi-}gcc
K=Escapement
M=$K/CORTEX-Mx
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

analyze() {   # analyze CPU PORT_DIR EXAMPLE_DIR [KERNELS]
    cpu=$1 port=$2 example=$3 kernels=${4:-"HARD SOFT HARD_PA"}
    for kernel in $kernels; do
        # Escapement_Config.h picks the hard kernel when no other is asked for.
        version=; [ "$kernel" = HARD ] || version=-DESCAPEMENT_VERSION_$kernel
        for scheduler in EARLIEST_DEADLINE_FIRST DEADLINE_MONOTONIC_SCHEDULING; do
            echo "== $cpu, $kernel, $scheduler"
            for f in "$K/EscapementHard.c" "$K/EscapementSoft.c" "$K/EscapementHardPA.c" \
                     "$M"/*.c "$port"/*.c; do
                $CC -mcpu="$cpu" -mthumb -mfloat-abi=soft -ffreestanding -O2 -fno-strict-aliasing -fanalyzer \
                    $version -DSCHEDULER_REAL_TIME_MODE=$scheduler \
                    -I"$example" -I"$port" -I"$M" -I"$K" -c "$f" -o /dev/null 2>>"$LOG"
            done
        done
    done
}
analyze cortex-m0plus "$M/RP2040" "$M/RP2040/Examples/pico"
analyze cortex-m33+nofp "$M/RP2350" "$M/RP2350/Examples/pico2" "HARD SOFT"
analyze cortex-m33+nofp "$M/STM32U5" "$M/STM32U5/Examples/uno-q" "HARD SOFT"
analyze cortex-m4 "$M/STM32" "$M/STM32/Examples/stm32f4-discovery" "HARD SOFT"
if grep -q "warning:" "$LOG"; then
    cat "$LOG"
    exit 1
fi
echo "no finding"
