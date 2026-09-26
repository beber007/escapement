#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Build the images the board checks run (tools/board_ci.sh) into OUT/<check>_<kernel>/:
# by the CI, which hands them to the bench as an artifact, so that the bench needs no
# compiler, or by the bench itself.
#
#   tools/board_images.sh OUT
#
# The cost and the timer events are measured with the counters of the example on, which
# it ships off: the configuration is changed for those images, and put back after.
set -eu

OUT=$1
PICO=Escapement/CORTEX-Mx/RP2040/Examples/pico
CONFIG=$PICO/Escapement_Config.h
SAVED=$(mktemp)
cp "$CONFIG" "$SAVED"
trap 'cp "$SAVED" "$CONFIG"; rm -f "$SAVED"' EXIT
mkdir -p "$OUT"

# image CHECK KERNEL IMAGE MAKE-ARGUMENTS COUNTERS
image() {
    cp "$SAVED" "$CONFIG"
    if [ "$5" = counters ]; then
        # No sed -i: GNU and BSD disagree on it.
        sed 's|^//#define ESCAPEMENT_MEASURE_SCHEDULING_COST|#define ESCAPEMENT_MEASURE_SCHEDULING_COST|' \
            "$SAVED" >"$CONFIG"
    fi
    make -s -C "$PICO" clean >/dev/null
    # shellcheck disable=SC2086
    make -s -C "$PICO" $4 "build/$3.elf" >/dev/null
    mkdir -p "$OUT/$1_$2"
    cp "$PICO/build/$3.elf" "$OUT/$1_$2/"
    echo "$1_$2: $3 ($4)"
}

image fourslot hard FourSlotCoresPico "" plain
image fourslot pa   FourSlotCoresPico "KERNEL=PA" plain
image cost     hard TaskLEDPico "" counters
image events   hard TestTimerEventPico "TRACE=1" counters
image events   soft TestTimerEventPico "KERNEL=SOFT TRACE=1" counters
image events   pa   TestTimerEventPico "KERNEL=PA TRACE=1" counters
image dvfs     pa   BenchDVFSPico "KERNEL=PA" plain
make -s -C "$PICO" clean >/dev/null
"${CROSS_COMPILE:-arm-none-eabi-}gcc" --version | head -1 >"$OUT/compiler"
