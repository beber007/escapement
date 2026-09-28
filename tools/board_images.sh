#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Build the images the board checks run (tools/board_ci.sh), the Pico's and the UNO Q's
# STM32U5's and the Pico 2's, into OUT/<check>_<kernel>/, and that of the PPK2's measurement: by the CI, which hands them to the bench as an
# artifact, so that the bench needs no compiler, or by the bench itself.
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
# The STM32U5 of the UNO Q: its endurance test and the idle task in Stop 2, which
# tools/unoq_check.sh runs.
U5=Escapement/CORTEX-Mx/STM32U5/Examples/uno-q
make -s -C "$U5" clean >/dev/null
make -s -C "$U5" build/SoakU5.elf build/SleepU5.elf >/dev/null
mkdir -p "$OUT/soak_u5"
cp "$U5/build/SoakU5.elf" "$U5/build/SleepU5.elf" "$OUT/soak_u5/"
make -s -C "$U5" clean >/dev/null
echo "soak_u5: SoakU5 SleepU5"
# The Pico 2: the five examples that count in memory, which tools/pico2_check.py runs.
PICO2=Escapement/CORTEX-Mx/RP2350/Examples/pico2
PICO2_IMAGES="FourSlotCoresPico2 ThreeSlotCoresPico2 FIFOCoresPico2 IPCPico2 SoakPico2"
make -s -C "$PICO2" clean >/dev/null
# shellcheck disable=SC2046  # one target per image
make -s -C "$PICO2" $(for i in $PICO2_IMAGES; do echo "build/$i.elf"; done) >/dev/null
mkdir -p "$OUT/pico2"
for i in $PICO2_IMAGES; do cp "$PICO2/build/$i.elf" "$OUT/pico2/"; done
make -s -C "$PICO2" clean >/dev/null
echo "pico2: $PICO2_IMAGES"
# Not a check: SleepU5 alternating 30 s in Stop 2 and 30 s in Sleep, for the PPK2
# (docs/stm32u5.md), built at each commit so that it is known to build, and at hand.
make -s -C "$U5" PHASES=30 build/SleepU5.elf >/dev/null
mkdir -p "$OUT/ppk2_u5"
cp "$U5/build/SleepU5.elf" "$OUT/ppk2_u5/SleepU5-phases30.elf"
make -s -C "$U5" clean >/dev/null
echo "ppk2_u5: SleepU5 PHASES=30"
# The same for the NUCLEO-U575ZI-Q, whose jumper gives the MCU's current alone.
NUCLEO=Escapement/CORTEX-Mx/STM32U5/Examples/nucleo-u575
make -s -C "$NUCLEO" clean >/dev/null
make -s -C "$NUCLEO" PHASES=30 build/SleepU5.elf >/dev/null
cp "$NUCLEO/build/SleepU5.elf" "$OUT/ppk2_u5/SleepU5-nucleo-phases30.elf"
make -s -C "$NUCLEO" clean >/dev/null
make -s -C "$NUCLEO" PHASES=30 SMPS=1 build/SleepU5.elf >/dev/null
cp "$NUCLEO/build/SleepU5.elf" "$OUT/ppk2_u5/SleepU5-nucleo-smps-phases30.elf"
make -s -C "$NUCLEO" clean >/dev/null
echo "ppk2_u5: SleepU5 PHASES=30, NUCLEO-U575ZI-Q, LDO and SMPS"
# Not a check either: the endurance test for a long run on that board (board_ci.md).
make -s -C "$NUCLEO" build/SoakU5.elf >/dev/null
mkdir -p "$OUT/soak_nucleo"
cp "$NUCLEO/build/SoakU5.elf" "$OUT/soak_nucleo/"
make -s -C "$NUCLEO" clean >/dev/null
echo "soak_nucleo: SoakU5, NUCLEO-U575ZI-Q"
"${CROSS_COMPILE:-arm-none-eabi-}gcc" --version | head -1 >"$OUT/compiler"
