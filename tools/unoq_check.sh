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
# minutes on Linux's raw clock, which NTP does not pull: against the disciplined one,
# shorter windows read up to 180 ppm off on 2026-09-26, and five minutes over a jittery
# Wi-Fi +278 and -686 ppm on 2026-09-28. The bound is 300 ppm, which a clock left on the
# MSIS (653 ppm locked, 4,800 free, stm32u5.md) still exceeds.
#
# The long run is the service BOARD_SOAK_SERVICE (escapement-soak-u5), its files in
# BOARD_SOAK_DIR (~/soak): stopped for the check, then started again on the commit's image
# and tools if the check passed, on the image it had otherwise, loaded again, since the
# check left the board running another. Carried on to another commit, the run of the
# commit before ends: its last status "board/soak-u5", still pending, is posted again as
# ended, a success, or left as it is if it was a failure, with the token of the board CI.
set -u

ELF=$1
SHA=$2
HERE=$(cd "$(dirname "$0")" && pwd)
SOAK=${BOARD_SOAK_DIR:-$HOME/soak}
SERVICE=${BOARD_SOAK_SERVICE:-escapement-soak-u5}
UNIT=$HOME/.config/systemd/user/$SERVICE.service
LIMIT_PPM=300
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
REPO=${BOARD_CI_REPO:-beber007/escapement}

# close <sha>: the run of the commit sha, carried on to $SHA, ended; otherwise its status
# stayed pending for good (2026-09-28).
close() {
    [ -r "$TOKEN" ] && [ -n "$1" ] && [ "$1" != "$SHA" ] || return 0
    auth="Authorization: Bearer $(cat "$TOKEN")"
    # The combined status, the last of each context: a list of them all has the minute's
    # of every endurance run first.
    last=$(curl -fsS -H "$auth" "https://api.github.com/repos/$REPO/commits/$1/status" |
        jq -c '[.statuses[] | select(.context == "board/soak-u5")][0] // empty') || return 0
    [ "$(echo "$last" | jq -r .state)" = pending ] || return 0
    jq -n --arg d "ended, carried on to $(echo "$SHA" | cut -c1-7): $(echo "$last" | jq -r .description)" \
        '{state: "success", context: "board/soak-u5", description: $d[:140]}' |
    curl -fsS -o /dev/null -X POST -H "$auth" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$1" || true
}

systemctl --user stop "$SERVICE" 2>/dev/null
ok=yes
SLEEP=$(dirname "$ELF")/SleepU5.elf
if [ -f "$SLEEP" ]; then
    sh "$HERE/unoq_load.sh" "$SLEEP" && python3 "$HERE/unoq_sleep.py" 60 || ok=""
fi
[ -n "$ok" ] && { sh "$HERE/unoq_load.sh" "$ELF" || ok=""; }
if [ -n "$ok" ]; then
    # A log and the state soak.py keeps beside it, both dropped after.
    run=$(mktemp -d)
    BOARD_CI_TOKEN=/nonexistent BOARD_SOAK_LOG=$run/soak.log \
        python3 "$HERE/soak.py" uno-q 2m 30s "$ELF" || ok=""
    rm -rf "$run"
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
    if [ -f "$SOAK/soak-u5.log" ]; then
        old=$SOAK/soak-u5-$(date +%Y%m%d-%H%M%S).log
        mv "$SOAK/soak-u5.log" "$old"
        [ -f "$SOAK/soak-u5.log.state" ] && mv "$SOAK/soak-u5.log.state" "$old.state"
    fi
    close "$(sed -n 's/.*BOARD_SOAK_SHA=\([0-9a-f]*\).*/\1/p' "$UNIT")"
    sed "s/BOARD_SOAK_SHA=[0-9a-f]*/BOARD_SOAK_SHA=$SHA/" "$UNIT" >"$UNIT.new" &&
        mv "$UNIT.new" "$UNIT"
    systemctl --user daemon-reload
elif [ -f "$SOAK/SoakU5.elf" ]; then
    sh "$SOAK/unoq_load.sh" "$SOAK/SoakU5.elf"
fi
[ -f "$UNIT" ] && systemctl --user start "$SERVICE"
[ -n "$ok" ]
