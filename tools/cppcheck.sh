#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Static analysis of our C code by cppcheck: the kernels, the Cortex-M layer, each port
# with its examples, and the host test. Each port is checked with its own include paths,
# which give the kernels the target's types and macros; cppcheck then explores the
# configurations of the #if it meets (hard, soft or power-aware kernel, EDF or
# deadline-monotonic scheduling, Cortex-M0+ or M3/M4/M33).
#
# The level is warning, portability and performance: what may be a defect. At the style
# level cppcheck reports mostly pointers that could be const, and a few false positives
# (fields set by chained assignments taken for uninitialised); it is left out so that
# this check says something when it fails. A finding that is known not to be a defect is
# suppressed where it stands, with a comment saying why (// cppcheck-suppress).
#
# The vendor libraries under STM32/Libraries are not ours and are not checked.
#
#   sh tools/cppcheck.sh
set -eu
cd "$(dirname "$0")/.."

K=Escapement
M=$K/CORTEX-Mx
check() {   # check PORT_DIR EXAMPLE_DIR SOURCES...
    port=$1 example=$2
    shift 2
    echo "== $port"
    cppcheck --quiet --error-exitcode=1 --inline-suppr --std=c99 \
        --enable=warning,portability,performance --platform=arm32-wchar_t4 \
        -I "$example" -I "$port" -I "$M" -I "$K" \
        "$K/EscapementHard.c" "$K/EscapementSoft.c" "$K/EscapementHardPA.c" \
        "$M"/*.c "$port"/*.c "$@"
}
check "$M/RP2040" "$M/RP2040/Examples/pico" "$M/RP2040/Examples/pico"/*.c
check "$M/RP2350" "$M/RP2350/Examples/pico2" "$M/RP2350/Examples/pico2"/*.c
check "$M/STM32" "$M/STM32/Examples/stm32f4-discovery" \
    "$M/STM32/Examples/stm32f4-discovery/TaskLEDF4.c" \
    "$M/STM32/Examples/stm32f4-discovery/TaskWrapF4.c" \
    "$M/STM32/Examples/stm32f4-discovery/TestTimerEventF4.c" \
    "$M/STM32/Examples/stm32f4-discovery/UARTSimpleEchoF4.c"
echo "== host test"
cppcheck --quiet --error-exitcode=1 --inline-suppr --std=c99 \
    --enable=warning,portability,performance \
    -I test/host -I "$K" test/host/*.c "$K/EscapementHard.c" \
    "$M/RP2350/Escapement_CoreQueue.c"
echo "no finding"
