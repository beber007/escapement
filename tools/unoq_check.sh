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
#                                      # and SleepNoHSEU5.elf beside it
#   BOARD=u3 tools/unoq_check.sh ELF SHA   # the NUCLEO-U385RG-Q plugged into the UNO Q:
#                                      # SoakU3.elf, SleepU3.elf beside it
#
# With BOARD=u3 the same checks run on the NUCLEO-U385RG-Q, through its ST-LINK, whose
# port and serial $NUCLEO_TTY and $NUCLEO_SERIAL name beside the NUCLEO-U575ZI-Q's, and
# the OpenOCD $OPENOCD names (tools/nucleo_load.sh): SleepU3 a minute at 96 MHz, SoakU3
# for two, its clock against Linux's; its long run is escapement-soak-u3 in ~/soak-u3, its
# status "board/soak-u3".
#
# SleepU5 runs first, if its image is there: the restart of the clock on waking from
# Stop 2 is the board's to check, Renode never entering it (tools/unoq_sleep.py). Then
# SleepNoHSEU5, if there, a minute too: the same, the HSE never started, PLL1 on the MSIS
# of range 2 at every wake-up, the path of a board without the HSE, which no board of the
# bench takes otherwise.
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
# ended, with the token of the board CI, or left as it is if it was not pending. The verdict
# and the counts are those of the state soak.py kept until it was stopped, not of that
# status: posted once an hour, it was up to an hour behind, and a failure whose post was
# lost left it pending, which the end then turned into a success (a review, 2026-09-30).
#
# With an image of the long run linked into the flash beside the checked one,
# flash/<image> (the NUCLEO-U385RG-Q's, tools/board_images.sh), the checks still run from
# SRAM and the long run goes on from the flash: tools/nucleo_flash.sh writes it there only
# if it differs, most commits leaving the firmware alone. Once in 24 hours at most until
# the user lifted that cap on 2026-10-10: each commit that changes the image writes it.
set -u

ELF=$1
SHA=$2
HERE=$(cd "$(dirname "$0")" && pwd)
case ${BOARD:-u5} in
    u5) KIND=uno-q LOADER=unoq_load.sh IMAGE=SoakU5.elf SLEEPS="SleepU5.elf SleepNoHSEU5.elf"
        PORT="" SOAK=${BOARD_SOAK_DIR:-$HOME/soak} ;;
    u3) KIND=nucleo-u3 LOADER=nucleo_load.sh IMAGE=SoakU3.elf SLEEPS=SleepU3.elf
        PORT=nucleo SOAK=${BOARD_SOAK_DIR:-$HOME/soak-u3}
        export NUCLEO_MCU=u385 MHZ=96 ;;
    *) echo "BOARD is u5 or u3" >&2; exit 1 ;;
esac
SERVICE=${BOARD_SOAK_SERVICE:-escapement-soak-${BOARD:-u5}}
CONTEXT=board/soak-${BOARD:-u5}
UNIT=$HOME/.config/systemd/user/$SERVICE.service
LIMIT_PPM=300
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
REPO=${BOARD_CI_REPO:-beber007/escapement}

# loader <elf>: tools/nucleo_flash.sh for an image whose entry point is in the flash,
# $LOADER otherwise.
loader() {
    entry=$(od -An -tx4 -j24 -N4 "$1" | tr -d ' ')
    case $entry in
        08*|09*|0[abcdef]*) echo nucleo_flash.sh ;;
        *) echo "$LOADER" ;;
    esac
}

# close <state> <sha>: the run whose state soak.py kept in the file state, of the commit
# sha if the state does not name it, carried on to $SHA, ended; otherwise its status
# stayed pending for good (2026-09-28). Failed if it restarted or found errors, as soak.py
# posts it; its interruptions, which the board did not cause, are only counted.
close() {
    sha=$2 summary="" state=success
    if jq -e .start "$1" >/dev/null 2>&1; then
        sha=$(jq -r '.sha // empty' "$1")
        [ -n "$sha" ] || sha=$2
        # As soak.py's elapsed(): 0d05h12m.
        summary=$(jq -r 'def two: tostring | if length < 2 then "0" + . else . end;
            ((.at - .start) / 60 | floor) as $m |
            "\($m / 1440 | floor)d\($m % 1440 / 60 | floor | two)h\($m % 60 | two)m: " +
            "\(.restarts) restarts, \(.errors) errors, \(.wraps) wraps" +
            if .interruptions > 0 then ", \(.interruptions) interrupted" else "" end' "$1") ||
            summary=""
        [ "$(jq '.restarts + .errors' "$1")" = 0 ] || state=failure
    fi
    [ -r "$TOKEN" ] && [ -n "$sha" ] && [ "$sha" != "$SHA" ] || return 0
    auth="Authorization: Bearer $(cat "$TOKEN")"
    # The combined status, the last of each context: a list of them all has the minute's
    # of every endurance run first.
    last=$(curl -fsS --retry 3 -H "$auth" \
        "https://api.github.com/repos/$REPO/commits/$sha/status" |
        jq -c --arg c "$CONTEXT" '[.statuses[] | select(.context == $c)][0] // empty') || return 0
    [ "$(echo "$last" | jq -r .state)" = pending ] || return 0
    [ -n "$summary" ] || summary=$(echo "$last" | jq -r .description)
    jq -n --arg s "$state" --arg d "ended, carried on to $(echo "$SHA" | cut -c1-7): $summary" \
        --arg c "$CONTEXT" '{state: $s, context: $c, description: $d[:140]}' |
    curl -fsS --retry 3 -o /dev/null -X POST -H "$auth" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$sha" || true
}

systemctl --user stop "$SERVICE" 2>/dev/null
ok=yes
for sleep in $SLEEPS; do
    sleep=$(dirname "$ELF")/$sleep
    if [ -n "$ok" ] && [ -f "$sleep" ]; then
        sh "$HERE/$LOADER" "$sleep" && python3 "$HERE/unoq_sleep.py" 60 $PORT || ok=""
    fi
done
[ -n "$ok" ] && { sh "$HERE/$LOADER" "$ELF" || ok=""; }
if [ -n "$ok" ]; then
    # A log and the state soak.py keeps beside it, both dropped after.
    run=$(mktemp -d)
    BOARD_CI_TOKEN=/nonexistent BOARD_SOAK_LOG=$run/soak.log \
        python3 "$HERE/soak.py" $KIND 2m 30s "$ELF" || ok=""
    rm -rf "$run"
fi
if [ -n "$ok" ]; then
    drift=$(python3 "$HERE/unoq_drift.py" 300 $PORT) || ok=""
    echo "$drift"
    ppm=$(echo "$drift" | sed -n 's/.*lasts \([-+0-9.]*\) ppm.*/\1/p')
    [ -n "$ppm" ] && awk -v p="$ppm" -v l="$LIMIT_PPM" 'BEGIN { exit !(p <= l && p >= -l) }' ||
        ok=""
fi

LONG=$ELF
carry=$ok
FLASH_ELF=$(dirname "$ELF")/flash/$IMAGE
if [ -n "$ok" ] && [ -f "$UNIT" ] && [ -f "$FLASH_ELF" ]; then
    if sh "$HERE/nucleo_flash.sh" "$FLASH_ELF"; then
        LONG=$FLASH_ELF
    else
        ok="" carry=""
    fi
fi

if [ -n "$carry" ] && [ -f "$UNIT" ]; then
    # The long run goes on with this commit: its image, its tools, a log of its own.
    cp "$LONG" "$SOAK/$IMAGE"
    cp "$HERE/soak.py" "$HERE/$LOADER" "$HERE/nucleo_flash.sh" "$SOAK/"
    log=$SOAK/soak-${BOARD:-u5}.log
    old=$SOAK/soak-${BOARD:-u5}-$(date +%Y%m%d-%H%M%S).log
    if [ -f "$log" ]; then
        mv "$log" "$old"
        [ -f "$log.state" ] && mv "$log.state" "$old.state"
    fi
    close "$old.state" "$(sed -n 's/.*BOARD_SOAK_SHA=\([0-9a-f]*\).*/\1/p' "$UNIT")"
    sed "s/BOARD_SOAK_SHA=[0-9a-f]*/BOARD_SOAK_SHA=$SHA/" "$UNIT" >"$UNIT.new" &&
        mv "$UNIT.new" "$UNIT"
    systemctl --user daemon-reload
elif [ -f "$SOAK/$IMAGE" ]; then
    sh "$SOAK/$(loader "$SOAK/$IMAGE")" "$SOAK/$IMAGE"
fi
[ -f "$UNIT" ] && systemctl --user start "$SERVICE"
[ -n "$ok" ]
