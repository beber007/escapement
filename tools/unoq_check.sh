#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The board check of the STM32U5, on the Arduino UNO Q itself: the idle task in Stop 2
# for a minute, the endurance test of a commit for two, then its clock against Linux's;
# and the long endurance run, which holds the board otherwise, carried on to that commit
# if it passes.
#
#   tools/unoq_check.sh ELF SHA        # ELF: SoakU5.elf of the commit SHA, SleepU5.elf
#                                      # beside it
#
# SleepU5 runs first, if its image is there: the restart of the clock on waking from
# Stop 2 is the board's to check, Renode never entering it (tools/unoq_sleep.py).
# SoakU5 checks every part of itself each second (SoakU5.c) and tools/soak.py ends with an
# error on any error or restart it saw. tools/unoq_drift.py then times its reports for five
# minutes: shorter windows read up to 180 ppm off on 2026-09-26, Linux's clock itself
# being pulled by NTP, so the bound is 300 ppm, which a clock left on the MSIS (653 ppm
# locked, 4,800 free, stm32u5.md) still exceeds.
#
# The long run is the service BOARD_SOAK_SERVICE (escapement-soak-u5), its files in
# BOARD_SOAK_DIR (~/soak): stopped for the check, then started again on the commit's image
# and tools if the check passed, on the image it had otherwise, loaded again, since the
# check left the board running another.
set -u

ELF=$1
SHA=$2
HERE=$(cd "$(dirname "$0")" && pwd)
SOAK=${BOARD_SOAK_DIR:-$HOME/soak}
SERVICE=${BOARD_SOAK_SERVICE:-escapement-soak-u5}
UNIT=$HOME/.config/systemd/user/$SERVICE.service
LIMIT_PPM=300

systemctl --user stop "$SERVICE" 2>/dev/null
ok=yes
SLEEP=$(dirname "$ELF")/SleepU5.elf
if [ -f "$SLEEP" ]; then
    sh "$HERE/unoq_load.sh" "$SLEEP" && python3 "$HERE/unoq_sleep.py" 60 || ok=""
fi
[ -n "$ok" ] && { sh "$HERE/unoq_load.sh" "$ELF" || ok=""; }
if [ -n "$ok" ]; then
    BOARD_CI_TOKEN=/nonexistent BOARD_SOAK_LOG=$(mktemp) \
        python3 "$HERE/soak.py" uno-q 2m 30s "$ELF" || ok=""
fi
if [ -n "$ok" ]; then
    drift=$(python3 "$HERE/unoq_drift.py" 300) || ok=""
    echo "$drift"
    ppm=$(echo "$drift" | sed -n 's/.*lasts \([-+0-9.]*\) ppm.*/\1/p')
    [ -n "$ppm" ] && awk -v p="$ppm" -v l="$LIMIT_PPM" 'BEGIN { exit !(p <= l && p >= -l) }' ||
        ok=""
fi

if [ -n "$ok" ] && [ -f "$UNIT" ]; then
    # The long run goes on with this commit: its image, its tools, a log of its own.
    cp "$ELF" "$SOAK/SoakU5.elf"
    cp "$HERE/soak.py" "$HERE/unoq_load.sh" "$SOAK/"
    [ -f "$SOAK/soak-u5.log" ] &&
        mv "$SOAK/soak-u5.log" "$SOAK/soak-u5-$(date +%Y%m%d-%H%M%S).log"
    sed "s/BOARD_SOAK_SHA=[0-9a-f]*/BOARD_SOAK_SHA=$SHA/" "$UNIT" >"$UNIT.new" &&
        mv "$UNIT.new" "$UNIT"
    systemctl --user daemon-reload
elif [ -f "$SOAK/SoakU5.elf" ]; then
    sh "$SOAK/unoq_load.sh" "$SOAK/SoakU5.elf"
fi
[ -f "$UNIT" ] && systemctl --user start "$SERVICE"
[ -n "$ok" ]
