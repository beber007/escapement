#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Run the board checks on the newest commit of main and post the outcome to GitHub as a
# commit status ("board/pico"). Meant for the machine the Pico is wired to, run by a
# timer; installing it: tools/board_ci.md, the checks: docs/rp2040.md.
#
#   tools/board_ci.sh [--force]      # --force: test main again even if already tested
#
# The machine pulls; GitHub never pushes code to it. Only main is ever built, and only
# its owner pushes to it: a pull request, from a fork or not, runs nothing here — which
# a self-hosted runner on a public repository could not promise.
#
# On Linux it builds in one container and drives the probe from another; on macOS,
# with the toolchain and OpenOCD from Homebrew, it runs both on the machine itself:
#   BOARD_CI_WORK    working directory (~/escapement-rp2040), which the containers mount
#   BOARD_CI_MOUNT   where they mount it (/work)
#   BOARD_CI_BUILD   container with the ARM toolchain (esc on Linux, none on macOS)
#   BOARD_CI_PROBE   container with OpenOCD and the probe (hw on Linux, none on macOS)
#   PROBE            the Debug Probe the checks use, probe1 by default (tools/probe.sh)
#   BOARD_CI_REPO    owner/name on GitHub (beber007/escapement)
#   BOARD_CI_TOKEN   file holding a token allowed to write commit statuses, nothing else
#                    (~/.config/escapement-board-ci/token); without it, nothing is posted
#   BOARD_CI_IMAGES  where the images come from: build (the default) builds them here,
#                    with the toolchain of the machine; ci takes those the CI built for
#                    the commit (tools/board_images.sh, artifact board-images), and needs
#                    no compiler: the token must then also read Actions. The compiled
#                    order, which the bench checked under a second compiler, is then the
#                    CI's to check (build.yml).
set -eu
STARTED_AS=$(cksum <"$0")   # before the checkout below can replace this very file

if [ "$(uname)" = Darwin ]; then containers=""; else containers=yes; fi
WORK=${BOARD_CI_WORK:-$HOME/escapement-rp2040}
MOUNT=${BOARD_CI_MOUNT:-/work}
BUILD=${BOARD_CI_BUILD-${containers:+esc}}
PROBER=${BOARD_CI_PROBE-${containers:+hw}}
REPO=${BOARD_CI_REPO:-beber007/escapement}
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
IMAGES=${BOARD_CI_IMAGES:-build}
DIR=$WORK/board-ci
SRC=$DIR/src
PICO=Escapement/CORTEX-Mx/RP2040/Examples/pico

mkdir -p "$DIR/logs"
# One run at a time. A directory, since mkdir is atomic everywhere and flock is not on
# macOS; a lock whose run has died is taken over.
LOCK=$DIR/lock
if [ "${BOARD_CI_RESTARTED:-}" = yes ]; then
    :   # started again by itself, below: the lock is already ours
elif ! mkdir "$LOCK" 2>/dev/null; then
    if kill -0 "$(cat "$LOCK/pid" 2>/dev/null)" 2>/dev/null; then
        echo "another run holds $LOCK"; exit 0
    fi
    rm -rf "$LOCK" && mkdir "$LOCK"
fi
echo $$ >"$LOCK/pid"
trap 'rm -rf "$LOCK"' EXIT

[ -d "$SRC/.git" ] || git clone --quiet "https://github.com/$REPO.git" "$SRC"
git -C "$SRC" fetch --quiet origin main
SHA=$(git -C "$SRC" rev-parse origin/main)
if [ "${1:-}" != "--force" ] && [ "$(cat "$DIR/last" 2>/dev/null)" = "$SHA" ]; then
    exit 0
fi
git -C "$SRC" checkout --quiet --force --detach "$SHA"
git -C "$SRC" clean --quiet -fdx
# The timer runs this script from the checkout it has just updated, and the shell goes on
# reading the file it opened: a change to the script took effect one run late, and the
# first run of a new check reported the old ones. Start the new script instead, once.
if [ "${BOARD_CI_RESTARTED:-}" != yes ] &&
   [ "$STARTED_AS" != "$(cksum <"$SRC/tools/board_ci.sh")" ]; then
    BOARD_CI_RESTARTED=yes exec sh "$SRC/tools/board_ci.sh" --force
fi
LOG=$DIR/logs/$(date +%Y%m%d-%H%M%S)-$(echo "$SHA" | cut -c1-7).log

status() {   # state description
    [ -r "$TOKEN" ] || return 0
    jq -n --arg s "$1" --arg d "$2" '{state: $s, context: "board/pico", description: $d}' |
    curl --silent --show-error --fail --output /dev/null -X POST \
        -H "Authorization: Bearer $(cat "$TOKEN")" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$SHA" || true
}

# in_build and in_probe run a command in the example's directory and at the root of the
# checkout, in the container when there is one; SEEN is $DIR as the command sees it.
if [ -n "$BUILD$PROBER" ]; then SEEN=$MOUNT/board-ci; else SEEN=$DIR; fi
in_build() {
    if [ -n "$BUILD" ]; then podman exec "$BUILD" sh -c "cd $SEEN/src/$PICO && $1"
    else (cd "$SRC/$PICO" && sh -c "$1"); fi
}
in_probe() {
    if [ -n "$PROBER" ]; then podman exec "$PROBER" sh -c "cd $SEEN/src && $1"
    else (cd "$SRC" && sh -c "$1"); fi
}

api() {   # path: a GET on the repository's API
    curl --silent --show-error --fail -H "Authorization: Bearer $(cat "$TOKEN")" \
        -H "Accept: application/vnd.github+json" "https://api.github.com/repos/$REPO/$1"
}

# ci_images: the images the CI built for $SHA into $DIR/fw. Returns 1 while its run is
# not over, which the next run of this script waits for, 2 if there are none.
ci_images() {
    run=$(api "actions/workflows/build.yml/runs?head_sha=$SHA&event=push" |
          jq -r '.workflow_runs[0] | "\(.status) \(.id)"') || return 2
    set -- $run
    [ "$1" = completed ] || return 1
    url=$(api "actions/runs/$2/artifacts" |
          jq -r '.artifacts[] | select(.name == "board-images") | .archive_download_url') ||
        return 2
    [ -n "$url" ] || return 2
    rm -rf "$DIR/fw" "$DIR/fw.zip" && mkdir -p "$DIR/fw"
    curl --silent --show-error --fail --location -H "Authorization: Bearer $(cat "$TOKEN")" \
        --output "$DIR/fw.zip" "$url" && unzip -q -o "$DIR/fw.zip" -d "$DIR/fw" || return 2
}

# build <name> <image> <make arguments>: one image into $DIR/fw/<name>.
build() {
    in_build "make clean >/dev/null && make $3 build/$2.elf" &&
    mkdir -p "$DIR/fw/$1" && cp "$SRC/$PICO/build/$2.elf" "$DIR/fw/$1/"
}

# fourslot <name>: no read of the 4-slot buffer torn or going backwards, and the plain
# array beside it torn at least once, or the check proved nothing.
fourslot() {
    out=$(in_probe "sh tools/fourslot_cores.sh 10 $SEEN/fw/$1/FourSlotCoresPico.elf")
    echo "$out"
    echo "$out" | awk '
        /^4-slot buffer/ { reads = $4; bad = $6 + $8 }
        /^plain array/   { torn = $6 }
        END { exit !(reads > 100000 && bad == 0 && torn > 0) }'
}

# order <name>: tools/check_order.py on the kernel this machine's compiler built, which is
# not the CI's: the order of the slot buffers and of the task-level stores holds under
# both, or one of them breaks it.
order() {
    in_build "arm-none-eabi-gcc --version | head -1 &&
              python3 $SEEN/src/tools/check_order.py \$(ls build/Escapement*.o | grep -v _)"
}

# cost <name>: the 1 ms round of TaskLEDPico, 10 s of it, and at most 5 us on average
# (3.2 measured for the hard kernel on 2026-09-23, docs/rp2040.md). The rounds count from
# the load to the reading, which takes the probe longer on some machines than on others:
# 10,067 on a Linux machine, 10,135 and 10,255 on a Mac mini, 10,116 on an Arduino UNO Q.
# The upper bound only catches a kernel that runs its round too often.
cost() {
    out=$(in_probe "sh tools/measure_cost.sh 10 $SEEN/fw/$1/TaskLEDPico.elf")
    echo "$out"
    echo "$out" | awk '
        /^rounds/ { rounds = $3 }
        /^mean/   { mean = $3 }
        END { exit !(rounds >= 9990 && rounds <= 11000 && mean <= 5.0) }'
}

# events <name>: TestTimerEventPico, five readings of its trace. GPIO 2 every 5 ms and
# GPIO 3 every 10 ms within 5 us, high for 1 and 2 ms plus the path from the alarm to the
# event-driven task (12 to 13 us on 2026-09-23, docs/rp2040.md) within 20 us, and every
# event delivered at most 2 us after its alarm (0 then).
events() {
    out=$(in_probe "python3 tools/timer_events.py $SEEN/fw/$1/TestTimerEventPico.elf")
    echo "$out"
    echo "$out" | awk '
        function within(value, low, high) { return value >= low && value <= high }
        $1 == "gpio" && $2 == 2 { pin2 = within($5, 4995, 5005) && within($6, 4995, 5005) &&
                                          within($10, 1000, 1020) && within($11, 1000, 1020) &&
                                          substr($8, 2) + 0 >= 20 }
        $1 == "gpio" && $2 == 3 { pin3 = within($5, 9995, 10005) && within($6, 9995, 10005) &&
                                          within($10, 2000, 2020) && within($11, 2000, 2020) &&
                                          substr($8, 2) + 0 >= 10 }
        /^events delivered/     { events = $4 + 0 > 0 && $7 <= 2 }
        END { exit !(pin2 && pin3 && events) }'
}

# dvfs <name>: BenchDVFSPico, each change of speed 10,000 times. Means of 2026-09-24 with
# two reads of the counter included (docs/rp2040.md): 8.7 us from 12 to 125 MHz at most,
# 6.0 by the wake-up path; allowed about a quarter more, rounded up to the microsecond.
# The wake-up path, which exists to be the shorter one, must stay shorter than the general
# one from the same speed.
dvfs() {
    out=$(in_probe "python3 tools/dvfs_bench.py $SEEN/fw/$1/BenchDVFSPico.elf")
    echo "$out"
    echo "$out" | awk -F': ' '
        { gsub(/ +$/, "", $1); us[$1] = $2 + 0 }
        END {
            ok = us["12 -> 125 MHz"] <= 11 && us["125 -> 12 MHz"] <= 6 &&
                 us["50 -> 125 MHz"] <= 7 && us["125 -> 50 MHz"] <= 7 &&
                 us["12 -> 50 MHz"] <= 10 && us["50 -> 12 MHz"] <= 6 &&
                 us["wake from 12 MHz"] <= 8 && us["wake from 50 MHz"] <= 6 &&
                 us["wake from 12 MHz"] < us["12 -> 125 MHz"] &&
                 us["wake from 50 MHz"] < us["50 -> 125 MHz"]
            for (row in us) if (row ~ /->|^wake/) { rows += 1; if (us[row] < 1) ok = 0 }
            exit !(ok && rows == 8) }'
}

checks="fourslot_hard fourslot_pa cost_hard events_hard events_soft events_pa dvfs_pa"
if [ "$IMAGES" = ci ]; then
    [ -r "$TOKEN" ] || { echo "BOARD_CI_IMAGES=ci needs $TOKEN"; exit 1; }
    got=0
    ci_images || got=$?
    if [ $got -eq 1 ]; then
        echo "the CI has not finished with $SHA"; exit 0
    elif [ $got -ne 0 ]; then
        echo "$SHA" >"$DIR/last"
        status error "no images from the CI for this commit"
        exit 1
    fi
else
    checks="order_hard order_soft order_pa $checks"
fi

status pending "running on the Pico"
failed=""
{
    echo "main at $SHA, $(date), images: $IMAGES"
    [ "$IMAGES" = build ] || cat "$DIR/fw/compiler" 2>/dev/null || true
    for check in $checks; do
        set -- $(echo "$check" | tr _ ' ')
        case $2 in
            hard) args="" ;;
            soft) args="KERNEL=SOFT" ;;
            pa)   args="KERNEL=PA" ;;
        esac
        echo "=== $1 $2"
        case $1 in
            order)    image=TaskLEDPico ;;
            fourslot) image=FourSlotCoresPico ;;
            cost)     image=TaskLEDPico ;;
            events)   image=TestTimerEventPico; args="$args TRACE=1" ;;
            dvfs)     image=BenchDVFSPico ;;
        esac
        if [ "$1" = cost ] || [ "$1" = events ]; then
            # The counters are off in the example as shipped; this checkout is ours. (No
            # sed -i: GNU and BSD disagree on it.)
            config=$SRC/$PICO/Escapement_Config.h
            sed 's|^//#define ESCAPEMENT_MEASURE_SCHEDULING_COST|#define ESCAPEMENT_MEASURE_SCHEDULING_COST|' \
                "$config" >"$config.new" && mv "$config.new" "$config"
        fi
        name=$1_$2
        if { [ "$IMAGES" = ci ] || build "$name" "$image" "$args" >/dev/null 2>&1; } &&
           "$1" "$name"; then
            echo "ok"
        else
            echo "FAILED"
            failed="$failed $1/$2"
        fi
        git -C "$SRC" checkout --quiet -- "$PICO/Escapement_Config.h"
    done
} >"$LOG" 2>&1

echo "$SHA" >"$DIR/last"
if [ -n "$failed" ]; then
    status failure "failed:$failed"
elif [ "$IMAGES" = ci ]; then
    status success "4-slot across cores, round cost, timer events, DVFS, on the CI's images"
else
    status success "compiled order, 4-slot across cores, round cost, timer events, DVFS"
fi
cat "$LOG"
[ -z "$failed" ]
