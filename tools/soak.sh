#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The endurance test on a Raspberry Pi Pico: load SoakPico, let it run, and read its
# counts at every interval without stopping it, for as long as asked.
#
#   tools/soak.sh DURATION INTERVAL [ELF]      e.g. tools/soak.sh 14d 1m
#
# DURATION and INTERVAL take a suffix s, m, h or d. Each reading appends a line to the
# log (BOARD_SOAK_LOG, soak-<date>.log in the current directory by default): the seconds
# run by the firmware, the wraps of the kernel clock crossed, the activity and the errors
# of each part (SoakPico.c), the worst lateness of the pulse and of the timer events, the
# stack left unused on each core, the load phase and the reason of the last reset the
# watchdog block records; every hour, the lateness of the pulse and of the timer events
# by bins of 10 us, those that are not empty.
#
# The test runs to its end whatever it finds. A reading whose marker is gone or whose
# seconds went back has seen the board restart, into its firmware in flash since the
# image runs from SRAM: it is logged with its reason, the image is loaded again, and the
# counts start over. Errors counted are added up across restarts, as is a part that did
# not move between two readings, or seconds that fell behind the time that passed. The
# test passes if none of these happened.
#
# With a token (BOARD_CI_TOKEN, as tools/board_ci.sh), the state is posted to GitHub as
# the commit status "board/soak" of BOARD_SOAK_SHA (HEAD of this checkout by default):
# pending with the time run, the restarts and the errors at each reading, failure as
# soon as either is not 0, success at the end if both are. While it runs, it holds the
# lock of tools/board_ci.sh (BOARD_CI_LOCK), so that the board CI loads no other image.
#
# Needs OpenOCD and a CMSIS-DAP probe; OpenOCD handles core 0 only (USE_CORE 0), for core
# 1 must run (tools/fourslot_cores.sh).
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
seconds() {   # seconds 2h -> 7200
    n=${1%[smhd]}
    case $1 in
        *d) echo $((n * 86400)) ;; *h) echo $((n * 3600)) ;;
        *m) echo $((n * 60)) ;;    *)  echo "$n" ;;
    esac
}
DURATION=$(seconds "${1:?duration}")
INTERVAL=$(seconds "${2:?interval}")
ELF=${3:-$ROOT/Escapement/CORTEX-Mx/RP2040/Examples/pico/build/SoakPico.elf}
LOG=${BOARD_SOAK_LOG:-soak-$(date +%Y%m%d-%H%M%S).log}
REPO=${BOARD_CI_REPO:-beber007/escapement}
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
SHA=${BOARD_SOAK_SHA:-$(git -C "$ROOT" rev-parse HEAD)}
LOCK=${BOARD_CI_LOCK:-$HOME/escapement-rp2040/board-ci/lock}
PARTS="pulse queue buffer events cores heartbeat interrupt memory"
WATCHDOG_REASON=0x40058008
WORDS=88          # Results, SoakPico.c

[ -f "$ELF" ] || { echo "build SoakPico first" >&2; exit 1; }
RESULTS=$(arm-none-eabi-nm "$ELF" | awk '$3 == "Results" { print "0x"$1 }')
[ -n "$RESULTS" ] || { echo "$ELF is not SoakPico" >&2; exit 1; }

mkdir -p "$(dirname "$LOCK")"
mkdir "$LOCK" 2>/dev/null || { echo "the board is in use ($LOCK)" >&2; exit 1; }
echo $$ >"$LOCK/pid"
trap 'rm -rf "$LOCK"' EXIT

status() {   # state description
    [ -r "$TOKEN" ] || return 0
    printf '{"state":"%s","context":"board/soak","description":"%s"}' "$1" "$2" |
    curl --silent --show-error --fail --output /dev/null -X POST \
        -H "Authorization: Bearer $(cat "$TOKEN")" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$SHA" || true
}
ocd() {
    openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -c 'set USE_CORE 0' \
        -f target/rp2040.cfg -c init "$@" -c exit 2>&1
}
load() { ocd -c 'reset halt' -c "load_image $ELF" -c 'resume 0x20000000' >/dev/null; }
elapsed() {
    d=$1
    printf '%dd%02dh%02dm' $((d / 86400)) $((d % 86400 / 3600)) $((d % 3600 / 60))
}
# bins FIRST: the bins of 10 us that are not empty, from word FIRST of the reading.
bins() {
    out="" i=0
    while [ $i -lt 32 ]; do
        eval "v=\${$(($1 + i + 1))}"
        [ $((0x$v)) -eq 0 ] || out="$out $((i * 10)):$((0x$v))"
        i=$((i + 1))
    done
    echo "${out# }"
}

load
start=$(date +%s)
echo "SoakPico $SHA, $(date), for $(elapsed "$DURATION"), read every ${INTERVAL} s" |
    tee "$LOG"
status pending "running since $(date +%Y-%m-%d)"
previous="" restarts=0 errors=0 wraps=0 last_hour=$start
while :; do
    sleep "$INTERVAL"
    now=$(date +%s)
    set -- $(ocd -c "mdw $RESULTS $WORDS" -c "mdw $WATCHDOG_REASON 1" |
             sed -n 's/^0x[0-9a-f]*: //p')
    if [ $# -lt $((WORDS + 1)) ]; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') no reading from the board" | tee -a "$LOG"
        errors=$((errors + 1))
    else
        secs=$((0x$2)) run_wraps=$((0x$3)) reason=${89}
        line="$(date '+%Y-%m-%d %H:%M:%S') run $secs s, $run_wraps wraps"
        i=0 activity="" found=0
        for part in $PARTS; do
            eval "a=\${$((i + 4))} e=\${$((i + 12))}"
            line="$line, $part $((0x$a))/$((0x$e))"
            activity="$activity $((0x$a))"; found=$((found + 0x$e)); i=$((i + 1))
        done
        line="$line, late max $((0x${20}))/$((0x${21})) us, stack free $((0x${22}))/$((0x${23}))"
        line="$line, load $([ $((0x${24})) -eq 1 ] && echo high || echo low), reset 0x$reason"
        echo "$line" >>"$LOG"
        if [ "$1" != 534f414b ] || { [ -n "$previous" ] && [ "$secs" -lt "${previous%% *}" ]; }; then
            # The board restarted: what it had counted is lost; load the image again.
            restarts=$((restarts + 1))
            echo "$(date '+%Y-%m-%d %H:%M:%S') RESTART $restarts, reset reason 0x$reason" |
                tee -a "$LOG"
            load
            previous=""
        else
            if [ "$found" -gt 0 ]; then
                echo "$(date '+%Y-%m-%d %H:%M:%S') ERRORS $found: $line" | tee -a "$LOG"
            fi
            new=$found
            if [ -n "$previous" ]; then
                set -- $previous
                prev_secs=$1 prev_found=$2; shift 2
                new=$((found - prev_found))
                # The firmware's seconds, against the time this machine saw pass, within 5 %.
                if [ $((secs - prev_secs)) -lt $(((now - prev_now) * 95 / 100)) ]; then
                    echo "$(date '+%Y-%m-%d %H:%M:%S') SECONDS BEHIND: $line" | tee -a "$LOG"
                    new=$((new + 1))
                fi
                for a in $activity; do
                    if [ "$a" -le "$1" ]; then
                        echo "$(date '+%Y-%m-%d %H:%M:%S') STOPPED: $line" | tee -a "$LOG"
                        new=$((new + 1))
                    fi
                    shift
                done
            fi
            errors=$((errors + new))
            wraps=$((wraps + run_wraps - ${prev_wraps:-0}))
            prev_wraps=$run_wraps
            if [ $((now - last_hour)) -ge 3600 ]; then
                set -- $(ocd -c "mdw $RESULTS $WORDS" | sed -n 's/^0x[0-9a-f]*: //p')
                [ $# -lt $WORDS ] || {
                    echo "  pulse late by us: $(bins 24)" >>"$LOG"
                    echo "  events late by us: $(bins 56)" >>"$LOG"
                }
                last_hour=$now
            fi
            previous="$secs $found$activity" prev_now=$now
        fi
        [ -n "$previous" ] || prev_wraps=0
    fi
    run="$(elapsed $((now - start))) of $(elapsed "$DURATION")"
    summary="$restarts restarts, $errors errors, $wraps wraps"
    if [ $((restarts + errors)) -gt 0 ]; then
        status failure "$run: $summary"
    else
        status pending "$run: $summary"
    fi
    [ $((now - start)) -lt "$DURATION" ] || break
done
echo "end after $(elapsed $((now - start))): $summary" | tee -a "$LOG"
if [ $((restarts + errors)) -eq 0 ]; then
    status success "$(elapsed $((now - start))) without error: $wraps wraps"
    exit 0
fi
exit 1
