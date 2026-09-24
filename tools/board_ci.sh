#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Run the board checks on the newest commit of main and post the outcome to GitHub as a
# commit status ("board/pico"). Meant for the machine the Pico is wired to, run by a
# timer; see docs/rp2040.md, "Checks on the board".
#
#   tools/board_ci.sh [--force]      # --force: test main again even if already tested
#
# The machine pulls; GitHub never pushes code to it. Only main is ever built, and only
# its owner pushes to it: a pull request, from a fork or not, runs nothing here — which
# a self-hosted runner on a public repository could not promise.
#
# It builds in one container and drives the probe from another, as the bench does:
#   BOARD_CI_WORK    host directory both containers mount (~/escapement-rp2040)
#   BOARD_CI_MOUNT   where they mount it (/work)
#   BOARD_CI_BUILD   container with the ARM toolchain (esc)
#   BOARD_CI_PROBE   container with OpenOCD and the probe (hw)
#   BOARD_CI_REPO    owner/name on GitHub (beber007/escapement)
#   BOARD_CI_TOKEN   file holding a token allowed to write commit statuses, nothing else
#                    (~/.config/escapement-board-ci/token); without it, nothing is posted
set -eu

WORK=${BOARD_CI_WORK:-$HOME/escapement-rp2040}
MOUNT=${BOARD_CI_MOUNT:-/work}
BUILD=${BOARD_CI_BUILD:-esc}
PROBE=${BOARD_CI_PROBE:-hw}
REPO=${BOARD_CI_REPO:-beber007/escapement}
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
DIR=$WORK/board-ci
SRC=$DIR/src
PICO=Escapement/CORTEX-Mx/RP2040/Examples/pico

mkdir -p "$DIR/logs"
exec 9>"$DIR/lock"
flock -n 9 || { echo "another run holds $DIR/lock"; exit 0; }

[ -d "$SRC/.git" ] || git clone --quiet "https://github.com/$REPO.git" "$SRC"
git -C "$SRC" fetch --quiet origin main
SHA=$(git -C "$SRC" rev-parse origin/main)
if [ "${1:-}" != "--force" ] && [ "$(cat "$DIR/last" 2>/dev/null)" = "$SHA" ]; then
    exit 0
fi
git -C "$SRC" checkout --quiet --force --detach "$SHA"
git -C "$SRC" clean --quiet -fdx
LOG=$DIR/logs/$(date +%Y%m%d-%H%M%S)-$(echo "$SHA" | cut -c1-7).log

status() {   # state description
    [ -r "$TOKEN" ] || return 0
    jq -n --arg s "$1" --arg d "$2" '{state: $s, context: "board/pico", description: $d}' |
    curl --silent --show-error --fail --output /dev/null -X POST \
        -H "Authorization: Bearer $(cat "$TOKEN")" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$SHA" || true
}

in_build() { podman exec "$BUILD" sh -c "cd $MOUNT/board-ci/src/$PICO && $1"; }
in_probe() { podman exec "$PROBE" sh -c "cd $MOUNT/board-ci/src && $1"; }

# build <name> <make arguments>: FourSlotCoresPico and TaskLEDPico into $DIR/fw/<name>.
build() {
    in_build "make clean >/dev/null && make $2 build/FourSlotCoresPico.elf build/TaskLEDPico.elf" &&
    mkdir -p "$DIR/fw/$1" && cp "$SRC/$PICO/build/FourSlotCoresPico.elf" \
        "$SRC/$PICO/build/TaskLEDPico.elf" "$DIR/fw/$1/"
}

# fourslot <name>: no read of the 4-slot buffer torn or going backwards, and the plain
# array beside it torn at least once, or the check proved nothing.
fourslot() {
    out=$(in_probe "sh tools/fourslot_cores.sh 10 $MOUNT/board-ci/fw/$1/FourSlotCoresPico.elf")
    echo "$out"
    echo "$out" | awk '
        /^4-slot buffer/ { reads = $4; bad = $6 + $8 }
        /^plain array/   { torn = $6 }
        END { exit !(reads > 100000 && bad == 0 && torn > 0) }'
}

# cost <name>: the 1 ms round of TaskLEDPico, 10 s of it, and at most 5 us on average
# (3.2 measured for the hard kernel on 2026-09-23, docs/rp2040.md).
cost() {
    out=$(in_probe "sh tools/measure_cost.sh 10 $MOUNT/board-ci/fw/$1/TaskLEDPico.elf")
    echo "$out"
    echo "$out" | awk '
        /^rounds/ { rounds = $3 }
        /^mean/   { mean = $3 }
        END { exit !(rounds >= 9990 && rounds <= 10100 && mean <= 5.0) }'
}

status pending "running on the Pico"
failed=""
{
    echo "main at $SHA, $(date)"
    for check in "fourslot hard" "fourslot pa" "cost hard"; do
        set -- $check
        case $2 in
            hard) args="" ;;
            pa)   args="KERNEL=PA" ;;
        esac
        echo "=== $check"
        if [ "$1" = cost ]; then
            # The counters are off in the example as shipped; this checkout is ours.
            sed -i 's|^//#define ESCAPEMENT_MEASURE_SCHEDULING_COST|#define ESCAPEMENT_MEASURE_SCHEDULING_COST|' \
                "$SRC/$PICO/Escapement_Config.h"
            name=cost_$2
        else
            name=$2
        fi
        if build "$name" "$args" >/dev/null 2>&1 && "$1" "$name"; then
            echo "ok"
        else
            echo "FAILED"
            failed="$failed $1/$2"
        fi
        git -C "$SRC" checkout --quiet -- "$PICO/Escapement_Config.h"
    done
} >"$LOG" 2>&1

echo "$SHA" >"$DIR/last"
if [ -z "$failed" ]; then
    status success "4-slot across cores (hard, PA) and round cost"
else
    status failure "failed:$failed"
fi
cat "$LOG"
[ -z "$failed" ]
